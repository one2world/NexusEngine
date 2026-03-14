#include "nexus/physics/physics_world_3d.h"
#include "nexus/core/log.h"
#include <algorithm>
#include <cmath>

namespace nexus::physics {

// ── Body3D ──────────────────────────────────────────────────────────────────

void Body3D::compute_mass() {
    if (type == Static) {
        mass = 0.0f;
        inv_mass = 0.0f;
        return;
    }
    inv_mass = (mass > 0.0f) ? 1.0f / mass : 0.0f;
}

// ── PhysicsWorld3D ──────────────────────────────────────────────────────────

PhysicsWorld3D::PhysicsWorld3D(Vec3 gravity) : gravity_(gravity) {
    bodies_.reserve(256);
}

u32 PhysicsWorld3D::create_body(const Body3D& desc) {
    Body3D body = desc;
    body.id = next_id_++;
    body.compute_mass();
    bodies_.push_back(body);
    return body.id;
}

void PhysicsWorld3D::destroy_body(u32 id) {
    bodies_.erase(
        std::remove_if(bodies_.begin(), bodies_.end(),
            [id](const Body3D& b) { return b.id == id; }),
        bodies_.end());
}

Body3D* PhysicsWorld3D::get_body(u32 id) {
    for (auto& b : bodies_) {
        if (b.id == id) return &b;
    }
    return nullptr;
}

const Body3D* PhysicsWorld3D::get_body(u32 id) const {
    for (const auto& b : bodies_) {
        if (b.id == id) return &b;
    }
    return nullptr;
}

void PhysicsWorld3D::step(float dt, u32 iterations) {
    if (dt <= 0.0f) return;

    integrate(dt);
    broadphase();

    for (u32 iter = 0; iter < iterations; ++iter) {
        for (auto& pair : contacts_) {
            Body3D* a = get_body(pair.body_a);
            Body3D* b = get_body(pair.body_b);
            if (a && b) resolve_collision(*a, *b, pair.contact);
        }
    }

    if (contact_callback_) {
        for (const auto& pair : contacts_) {
            contact_callback_(pair);
        }
    }
}

void PhysicsWorld3D::apply_force(u32 id, Vec3 force) {
    if (auto* b = get_body(id)) b->force += force;
}

void PhysicsWorld3D::apply_impulse(u32 id, Vec3 impulse) {
    if (auto* b = get_body(id)) {
        b->velocity += impulse * b->inv_mass;
    }
}

void PhysicsWorld3D::apply_torque(u32 id, Vec3 torque) {
    if (auto* b = get_body(id)) b->torque_accum += torque;
}

// ── Integration ─────────────────────────────────────────────────────────────

void PhysicsWorld3D::integrate(float dt) {
    for (auto& b : bodies_) {
        if (b.type == Body3D::Static) continue;

        if (b.type == Body3D::Dynamic) {
            Vec3 accel = gravity_ * b.gravity_scale + b.force * b.inv_mass;
            b.velocity += accel * dt;
            b.angular_velocity += b.torque_accum * dt; // simplified

            b.velocity *= 1.0f / (1.0f + b.linear_damping * dt);
            b.angular_velocity *= 1.0f / (1.0f + b.angular_damping * dt);
        }

        b.position += b.velocity * dt;

        // Integrate angular velocity into quaternion
        float ang_speed = glm::length(b.angular_velocity);
        if (ang_speed > math::EPSILON) {
            Vec3 axis = b.angular_velocity / ang_speed;
            float half_angle = ang_speed * dt * 0.5f;
            Quat dq = Quat(std::cos(half_angle),
                           axis.x * std::sin(half_angle),
                           axis.y * std::sin(half_angle),
                           axis.z * std::sin(half_angle));
            b.rotation = glm::normalize(dq * b.rotation);
        }

        b.force = Vec3(0.0f);
        b.torque_accum = Vec3(0.0f);
    }
}

// ── Broadphase ──────────────────────────────────────────────────────────────

static AABB body3d_aabb(const Body3D& b) {
    AABB aabb;
    if (b.shape == Body3D::Sphere) {
        aabb.min = b.position - Vec3(b.radius);
        aabb.max = b.position + Vec3(b.radius);
    } else if (b.shape == Body3D::Capsule) {
        float h = b.height * 0.5f;
        float r = b.radius;
        float ext = h + r;
        aabb.min = b.position - Vec3(r, ext, r);
        aabb.max = b.position + Vec3(r, ext, r);
    } else {
        // Conservative AABB for box (ignoring rotation for broadphase)
        float maxExt = std::max({b.half_extents.x, b.half_extents.y, b.half_extents.z});
        float diag = maxExt * 1.7321f; // sqrt(3)
        aabb.min = b.position - Vec3(diag);
        aabb.max = b.position + Vec3(diag);
    }
    return aabb;
}

void PhysicsWorld3D::broadphase() {
    contacts_.clear();

    for (size_t i = 0; i < bodies_.size(); ++i) {
        for (size_t j = i + 1; j < bodies_.size(); ++j) {
            auto& a = bodies_[i];
            auto& b = bodies_[j];

            if (a.type == Body3D::Static && b.type == Body3D::Static) continue;
            if (!(a.layer & b.mask) || !(b.layer & a.mask)) continue;

            AABB aa = body3d_aabb(a);
            AABB ab = body3d_aabb(b);
            if (!aa.overlaps(ab)) continue;

            Contact3D contact;
            if (narrowphase(a, b, contact)) {
                contacts_.push_back({a.id, b.id, contact});
            }
        }
    }
}

// ── Narrow phase ────────────────────────────────────────────────────────────

bool PhysicsWorld3D::narrowphase(const Body3D& a, const Body3D& b, Contact3D& contact) const {
    if (a.shape == Body3D::Sphere && b.shape == Body3D::Sphere) {
        return sphere_vs_sphere(a, b, contact);
    }
    if (a.shape == Body3D::Box && b.shape == Body3D::Box) {
        return box_vs_box(a, b, contact);
    }
    if (a.shape == Body3D::Sphere && b.shape == Body3D::Box) {
        return sphere_vs_box(a, b, contact);
    }
    if (a.shape == Body3D::Box && b.shape == Body3D::Sphere) {
        bool result = sphere_vs_box(b, a, contact);
        if (result) contact.normal = -contact.normal;
        return result;
    }
    // Capsule treated as sphere for basic collision
    if (a.shape == Body3D::Capsule || b.shape == Body3D::Capsule) {
        // Approximate capsule as sphere with combined radius
        Body3D sa = a, sb = b;
        if (sa.shape == Body3D::Capsule) {
            sa.shape = Body3D::Sphere;
            sa.radius = a.radius + a.height * 0.5f;
        }
        if (sb.shape == Body3D::Capsule) {
            sb.shape = Body3D::Sphere;
            sb.radius = b.radius + b.height * 0.5f;
        }
        return narrowphase(sa, sb, contact);
    }
    return false;
}

bool PhysicsWorld3D::sphere_vs_sphere(const Body3D& a, const Body3D& b, Contact3D& c) const {
    Vec3 diff = b.position - a.position;
    float dist_sq = glm::dot(diff, diff);
    float sum_r = a.radius + b.radius;

    if (dist_sq > sum_r * sum_r) return false;

    float dist = std::sqrt(dist_sq);
    if (dist < math::EPSILON) {
        c.normal = {0.0f, 1.0f, 0.0f};
        c.depth = sum_r;
        c.point = a.position;
    } else {
        c.normal = diff / dist;
        c.depth = sum_r - dist;
        c.point = a.position + c.normal * a.radius;
    }
    return true;
}

bool PhysicsWorld3D::box_vs_box(const Body3D& a, const Body3D& b, Contact3D& c) const {
    // AABB vs AABB (ignoring rotation for simplicity)
    Vec3 diff = b.position - a.position;
    Vec3 overlap;
    for (int i = 0; i < 3; ++i) {
        overlap[i] = a.half_extents[i] + b.half_extents[i] - std::abs(diff[i]);
        if (overlap[i] <= 0.0f) return false;
    }

    // Find minimum overlap axis
    int min_axis = 0;
    if (overlap[1] < overlap[min_axis]) min_axis = 1;
    if (overlap[2] < overlap[min_axis]) min_axis = 2;

    c.normal = Vec3(0.0f);
    c.normal[min_axis] = (diff[min_axis] < 0.0f) ? -1.0f : 1.0f;
    c.depth = overlap[min_axis];
    c.point = a.position + c.normal * a.half_extents;
    return true;
}

bool PhysicsWorld3D::sphere_vs_box(const Body3D& sphere, const Body3D& box, Contact3D& c) const {
    Vec3 diff = sphere.position - box.position;

    Vec3 closest;
    for (int i = 0; i < 3; ++i) {
        closest[i] = math::clamp(diff[i], -box.half_extents[i], box.half_extents[i]);
    }

    Vec3 delta = diff - closest;
    float dist_sq = glm::dot(delta, delta);

    if (dist_sq > sphere.radius * sphere.radius) return false;

    float dist = std::sqrt(dist_sq);
    if (dist < math::EPSILON) {
        // Sphere center inside box
        Vec3 pen;
        for (int i = 0; i < 3; ++i) {
            pen[i] = box.half_extents[i] - std::abs(diff[i]);
        }
        int min_axis = 0;
        if (pen[1] < pen[min_axis]) min_axis = 1;
        if (pen[2] < pen[min_axis]) min_axis = 2;

        c.normal = Vec3(0.0f);
        c.normal[min_axis] = (diff[min_axis] < 0.0f) ? -1.0f : 1.0f;
        c.depth = pen[min_axis] + sphere.radius;
    } else {
        c.normal = delta / dist;
        c.depth = sphere.radius - dist;
    }
    c.point = sphere.position - c.normal * sphere.radius;
    return true;
}

// ── Resolution ──────────────────────────────────────────────────────────────

void PhysicsWorld3D::resolve_collision(Body3D& a, Body3D& b, const Contact3D& contact) {
    if (a.is_trigger || b.is_trigger) return;

    float inv_mass_sum = a.inv_mass + b.inv_mass;
    if (inv_mass_sum <= 0.0f) return;

    // Positional correction
    const float percent = 0.8f;
    const float slop = 0.01f;
    Vec3 correction = contact.normal *
        (std::max(contact.depth - slop, 0.0f) / inv_mass_sum) * percent;

    a.position -= correction * a.inv_mass;
    b.position += correction * b.inv_mass;

    // Impulse resolution
    Vec3 rel_vel = b.velocity - a.velocity;
    float vel_along_normal = glm::dot(rel_vel, contact.normal);

    if (vel_along_normal > 0.0f) return;

    float e = std::min(a.restitution, b.restitution);
    float j = -(1.0f + e) * vel_along_normal / inv_mass_sum;

    Vec3 impulse = j * contact.normal;
    a.velocity -= impulse * a.inv_mass;
    b.velocity += impulse * b.inv_mass;

    // Friction
    Vec3 tangent = rel_vel - contact.normal * vel_along_normal;
    float tangent_len = glm::length(tangent);
    if (tangent_len > math::EPSILON) {
        tangent /= tangent_len;
        float jt = -glm::dot(rel_vel, tangent) / inv_mass_sum;
        float mu = std::sqrt(a.friction * b.friction);

        Vec3 friction_impulse;
        if (std::abs(jt) < j * mu) {
            friction_impulse = jt * tangent;
        } else {
            friction_impulse = -j * mu * tangent;
        }

        a.velocity -= friction_impulse * a.inv_mass;
        b.velocity += friction_impulse * b.inv_mass;
    }
}

// ── Queries ─────────────────────────────────────────────────────────────────

bool PhysicsWorld3D::raycast(Vec3 origin, Vec3 direction, float max_distance,
                              RayHit3D& hit, u16 layer_mask) const {
    Vec3 dir = glm::normalize(direction);
    float closest = max_distance;
    bool found = false;

    for (const auto& b : bodies_) {
        if (!(b.layer & layer_mask)) continue;

        if (b.shape == Body3D::Sphere || b.shape == Body3D::Capsule) {
            float r = (b.shape == Body3D::Capsule) ? b.radius + b.height * 0.5f : b.radius;
            Vec3 oc = origin - b.position;
            float a_c = glm::dot(dir, dir);
            float b_c = 2.0f * glm::dot(oc, dir);
            float c_c = glm::dot(oc, oc) - r * r;
            float disc = b_c * b_c - 4.0f * a_c * c_c;
            if (disc < 0.0f) continue;

            float t = (-b_c - std::sqrt(disc)) / (2.0f * a_c);
            if (t >= 0.0f && t < closest) {
                closest = t;
                hit.body_id = b.id;
                hit.point = origin + dir * t;
                hit.normal = glm::normalize(hit.point - b.position);
                hit.distance = t;
                found = true;
            }
        } else {
            // AABB ray intersection
            Vec3 bmin = b.position - b.half_extents;
            Vec3 bmax = b.position + b.half_extents;

            float tmin_val = 0.0f, tmax_val = max_distance;
            for (int axis = 0; axis < 3; ++axis) {
                if (std::abs(dir[axis]) < math::EPSILON) {
                    if (origin[axis] < bmin[axis] || origin[axis] > bmax[axis])
                        goto next_body_3d;
                    continue;
                }
                float inv_d = 1.0f / dir[axis];
                float t1 = (bmin[axis] - origin[axis]) * inv_d;
                float t2 = (bmax[axis] - origin[axis]) * inv_d;
                if (inv_d < 0.0f) std::swap(t1, t2);
                tmin_val = std::max(tmin_val, t1);
                tmax_val = std::min(tmax_val, t2);
                if (tmax_val < tmin_val) goto next_body_3d;
            }

            if (tmin_val < closest) {
                closest = tmin_val;
                hit.body_id = b.id;
                hit.point = origin + dir * tmin_val;
                hit.distance = tmin_val;
                Vec3 p = hit.point - b.position;
                Vec3 abs_p = glm::abs(p);
                if (abs_p.x > abs_p.y && abs_p.x > abs_p.z)
                    hit.normal = {(p.x > 0.0f) ? 1.0f : -1.0f, 0.0f, 0.0f};
                else if (abs_p.y > abs_p.z)
                    hit.normal = {0.0f, (p.y > 0.0f) ? 1.0f : -1.0f, 0.0f};
                else
                    hit.normal = {0.0f, 0.0f, (p.z > 0.0f) ? 1.0f : -1.0f};
                found = true;
            }
        }
        next_body_3d:;
    }
    return found;
}

std::vector<u32> PhysicsWorld3D::overlap_sphere(Vec3 center, float radius,
                                                 u16 layer_mask) const {
    std::vector<u32> result;
    for (const auto& b : bodies_) {
        if (!(b.layer & layer_mask)) continue;

        if (b.shape == Body3D::Sphere) {
            float dist = glm::length(b.position - center);
            if (dist < radius + b.radius) result.push_back(b.id);
        } else {
            Vec3 closest;
            for (int i = 0; i < 3; ++i) {
                closest[i] = math::clamp(center[i],
                    b.position[i] - b.half_extents[i],
                    b.position[i] + b.half_extents[i]);
            }
            float dist_sq = glm::dot(center - closest, center - closest);
            if (dist_sq < radius * radius) result.push_back(b.id);
        }
    }
    return result;
}

std::vector<u32> PhysicsWorld3D::overlap_aabb(Vec3 min_pt, Vec3 max_pt,
                                               u16 layer_mask) const {
    std::vector<u32> result;
    for (const auto& b : bodies_) {
        if (!(b.layer & layer_mask)) continue;
        AABB aabb = body3d_aabb(b);
        if (aabb.max.x >= min_pt.x && aabb.min.x <= max_pt.x &&
            aabb.max.y >= min_pt.y && aabb.min.y <= max_pt.y &&
            aabb.max.z >= min_pt.z && aabb.min.z <= max_pt.z) {
            result.push_back(b.id);
        }
    }
    return result;
}

} // namespace nexus::physics
