#include "nexus/physics/physics_world_2d.h"
#include "nexus/core/log.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace nexus::physics {

// ── Body2D mass computation ─────────────────────────────────────────────────

void Body2D::compute_mass() {
    if (type == Static) {
        mass = 0.0f;
        inv_mass = 0.0f;
        inertia = 0.0f;
        inv_inertia = 0.0f;
        return;
    }

    float area = 0.0f;
    if (shape == Box) {
        area = 4.0f * half_size.x * half_size.y;
    } else {
        area = math::PI * radius * radius;
    }

    mass = density * area;
    inv_mass = (mass > 0.0f) ? 1.0f / mass : 0.0f;

    // Moment of inertia
    if (shape == Box) {
        float w = 2.0f * half_size.x;
        float h = 2.0f * half_size.y;
        inertia = mass * (w * w + h * h) / 12.0f;
    } else {
        inertia = 0.5f * mass * radius * radius;
    }

    if (fixed_rotation) {
        inertia = 0.0f;
        inv_inertia = 0.0f;
    } else {
        inv_inertia = (inertia > 0.0f) ? 1.0f / inertia : 0.0f;
    }
}

// ── PhysicsWorld2D ──────────────────────────────────────────────────────────

PhysicsWorld2D::PhysicsWorld2D(Vec2 gravity) : gravity_(gravity) {
    bodies_.reserve(256);
}

u32 PhysicsWorld2D::create_body(const Body2D& desc) {
    Body2D body = desc;
    body.id = next_id_++;
    body.compute_mass();
    bodies_.push_back(body);
    return body.id;
}

void PhysicsWorld2D::destroy_body(u32 id) {
    bodies_.erase(
        std::remove_if(bodies_.begin(), bodies_.end(),
            [id](const Body2D& b) { return b.id == id; }),
        bodies_.end());
}

Body2D* PhysicsWorld2D::get_body(u32 id) {
    for (auto& b : bodies_) {
        if (b.id == id) return &b;
    }
    return nullptr;
}

const Body2D* PhysicsWorld2D::get_body(u32 id) const {
    for (const auto& b : bodies_) {
        if (b.id == id) return &b;
    }
    return nullptr;
}

void PhysicsWorld2D::step(float dt, u32 velocity_iterations, u32 /*position_iterations*/) {
    if (dt <= 0.0f) return;

    // Integrate forces/velocity
    integrate(dt);

    // Detect collisions
    broadphase();

    // Resolve (iterative impulse solver)
    for (u32 iter = 0; iter < velocity_iterations; ++iter) {
        for (auto& pair : contacts_) {
            Body2D* a = get_body(pair.body_a);
            Body2D* b = get_body(pair.body_b);
            if (a && b) resolve_collision(*a, *b, pair.contact);
        }
    }

    // Fire contact callbacks
    if (contact_callback_) {
        for (const auto& pair : contacts_) {
            contact_callback_(pair);
        }
    }
}

void PhysicsWorld2D::apply_force(u32 id, Vec2 force) {
    if (auto* b = get_body(id)) b->force += force;
}

void PhysicsWorld2D::apply_impulse(u32 id, Vec2 impulse) {
    if (auto* b = get_body(id)) {
        b->velocity += impulse * b->inv_mass;
    }
}

void PhysicsWorld2D::apply_torque(u32 id, float torque) {
    if (auto* b = get_body(id)) b->torque += torque;
}

// ── Integration ─────────────────────────────────────────────────────────────

void PhysicsWorld2D::integrate(float dt) {
    for (auto& b : bodies_) {
        if (b.type == Body2D::Static) continue;

        if (b.type == Body2D::Dynamic) {
            // Apply gravity
            Vec2 accel = gravity_ * b.gravity_scale + b.force * b.inv_mass;
            b.velocity += accel * dt;
            b.angular_velocity += b.torque * b.inv_inertia * dt;

            // Damping
            b.velocity *= 1.0f / (1.0f + b.linear_damping * dt);
            b.angular_velocity *= 1.0f / (1.0f + b.angular_damping * dt);
        }

        // Integrate position
        b.position += b.velocity * dt;
        b.rotation += b.angular_velocity * dt;

        // Clear accumulators
        b.force = {0.0f, 0.0f};
        b.torque = 0.0f;
    }
}

// ── Broadphase (AABB overlap, N^2 for now) ──────────────────────────────────

static void body_aabb(const Body2D& b, Vec2& out_min, Vec2& out_max) {
    if (b.shape == Body2D::Circle) {
        out_min = b.position - Vec2(b.radius);
        out_max = b.position + Vec2(b.radius);
    } else {
        // Conservative AABB for rotated box
        float c = std::abs(std::cos(b.rotation));
        float s = std::abs(std::sin(b.rotation));
        float hx = b.half_size.x * c + b.half_size.y * s;
        float hy = b.half_size.x * s + b.half_size.y * c;
        out_min = b.position - Vec2(hx, hy);
        out_max = b.position + Vec2(hx, hy);
    }
}

void PhysicsWorld2D::broadphase() {
    contacts_.clear();

    for (size_t i = 0; i < bodies_.size(); ++i) {
        for (size_t j = i + 1; j < bodies_.size(); ++j) {
            auto& a = bodies_[i];
            auto& b = bodies_[j];

            // Skip static-static
            if (a.type == Body2D::Static && b.type == Body2D::Static) continue;

            // Layer check
            if (!(a.layer & b.mask) || !(b.layer & a.mask)) continue;

            // AABB overlap test
            Vec2 a_min, a_max, b_min, b_max;
            body_aabb(a, a_min, a_max);
            body_aabb(b, b_min, b_max);

            if (a_max.x < b_min.x || a_min.x > b_max.x ||
                a_max.y < b_min.y || a_min.y > b_max.y) continue;

            // Narrow phase
            Contact2D contact;
            if (narrowphase(a, b, contact)) {
                contacts_.push_back({a.id, b.id, contact});
            }
        }
    }
}

// ── Narrow phase ────────────────────────────────────────────────────────────

bool PhysicsWorld2D::narrowphase(const Body2D& a, const Body2D& b, Contact2D& contact) const {
    if (a.shape == Body2D::Circle && b.shape == Body2D::Circle) {
        return circle_vs_circle(a, b, contact);
    }
    if (a.shape == Body2D::Box && b.shape == Body2D::Box) {
        return box_vs_box(a, b, contact);
    }
    if (a.shape == Body2D::Circle && b.shape == Body2D::Box) {
        return circle_vs_box(a, b, contact);
    }
    if (a.shape == Body2D::Box && b.shape == Body2D::Circle) {
        bool result = circle_vs_box(b, a, contact);
        if (result) contact.normal = -contact.normal;
        return result;
    }
    return false;
}

bool PhysicsWorld2D::circle_vs_circle(const Body2D& a, const Body2D& b, Contact2D& c) const {
    Vec2 diff = b.position - a.position;
    float dist_sq = glm::dot(diff, diff);
    float sum_r = a.radius + b.radius;

    if (dist_sq > sum_r * sum_r) return false;

    float dist = std::sqrt(dist_sq);
    if (dist < math::EPSILON) {
        c.normal = {0.0f, 1.0f};
        c.depth = sum_r;
        c.point = a.position;
    } else {
        c.normal = diff / dist;
        c.depth = sum_r - dist;
        c.point = a.position + c.normal * a.radius;
    }
    return true;
}

bool PhysicsWorld2D::box_vs_box(const Body2D& a, const Body2D& b, Contact2D& c) const {
    // OBB vs OBB using Separating Axis Theorem with rotation support
    float cos_a = std::cos(a.rotation), sin_a = std::sin(a.rotation);
    float cos_b = std::cos(b.rotation), sin_b = std::sin(b.rotation);

    // Build local axes for each OBB
    Vec2 axes[4] = {
        {cos_a, sin_a}, {-sin_a, cos_a},   // A's local X and Y axes
        {cos_b, sin_b}, {-sin_b, cos_b}    // B's local X and Y axes
    };

    Vec2 diff = b.position - a.position;
    float min_overlap = std::numeric_limits<float>::max();
    Vec2 min_axis{0.0f, 0.0f};

    // Get corner vertices of each OBB
    auto get_corners = [](Vec2 pos, Vec2 half, float cs, float sn) {
        Vec2 ax{cs, sn};
        Vec2 ay{-sn, cs};
        Vec2 ex = ax * half.x;
        Vec2 ey = ay * half.y;
        return std::array<Vec2, 4>{{
            pos - ex - ey, pos + ex - ey,
            pos + ex + ey, pos - ex + ey
        }};
    };

    auto corners_a = get_corners(a.position, a.half_size, cos_a, sin_a);
    auto corners_b = get_corners(b.position, b.half_size, cos_b, sin_b);

    // Test all 4 separating axes
    for (int i = 0; i < 4; ++i) {
        Vec2 axis = axes[i];

        // Project both OBBs onto this axis
        float min_a_proj =  std::numeric_limits<float>::max();
        float max_a_proj = -std::numeric_limits<float>::max();
        float min_b_proj =  std::numeric_limits<float>::max();
        float max_b_proj = -std::numeric_limits<float>::max();

        for (int j = 0; j < 4; ++j) {
            float pa = glm::dot(corners_a[j], axis);
            float pb = glm::dot(corners_b[j], axis);
            min_a_proj = std::min(min_a_proj, pa);
            max_a_proj = std::max(max_a_proj, pa);
            min_b_proj = std::min(min_b_proj, pb);
            max_b_proj = std::max(max_b_proj, pb);
        }

        // Check for separation
        float overlap = std::min(max_a_proj, max_b_proj) - std::max(min_a_proj, min_b_proj);
        if (overlap <= 0.0f) return false; // Separating axis found

        if (overlap < min_overlap) {
            min_overlap = overlap;
            min_axis = axis;
        }
    }

    // Ensure normal points from A to B
    if (glm::dot(min_axis, diff) < 0.0f) {
        min_axis = -min_axis;
    }

    c.normal = min_axis;
    c.depth = min_overlap;
    c.point = a.position + c.normal * glm::dot(a.half_size, Vec2(std::abs(glm::dot(axes[0], min_axis)),
                                                                    std::abs(glm::dot(axes[1], min_axis))));
    return true;
}

bool PhysicsWorld2D::circle_vs_box(const Body2D& circle, const Body2D& box, Contact2D& c) const {
    Vec2 diff = circle.position - box.position;

    // Clamp to box extents
    Vec2 closest;
    closest.x = math::clamp(diff.x, -box.half_size.x, box.half_size.x);
    closest.y = math::clamp(diff.y, -box.half_size.y, box.half_size.y);

    Vec2 delta = diff - closest;
    float dist_sq = glm::dot(delta, delta);

    if (dist_sq > circle.radius * circle.radius) return false;

    float dist = std::sqrt(dist_sq);
    if (dist < math::EPSILON) {
        // Circle center inside box
        float pen_x = box.half_size.x - std::abs(diff.x);
        float pen_y = box.half_size.y - std::abs(diff.y);
        if (pen_x < pen_y) {
            c.normal = {(diff.x < 0.0f) ? -1.0f : 1.0f, 0.0f};
            c.depth = pen_x + circle.radius;
        } else {
            c.normal = {0.0f, (diff.y < 0.0f) ? -1.0f : 1.0f};
            c.depth = pen_y + circle.radius;
        }
    } else {
        c.normal = delta / dist;
        c.depth = circle.radius - dist;
    }
    c.point = circle.position - c.normal * circle.radius;
    return true;
}

// ── Collision resolution ────────────────────────────────────────────────────

void PhysicsWorld2D::resolve_collision(Body2D& a, Body2D& b, const Contact2D& contact) {
    // Don't resolve triggers
    if (a.is_trigger || b.is_trigger) return;

    float inv_mass_sum = a.inv_mass + b.inv_mass;
    if (inv_mass_sum <= 0.0f) return;

    // Positional correction (prevent sinking)
    const float percent = 0.8f;
    const float slop = 0.01f;
    Vec2 correction = contact.normal *
        (std::max(contact.depth - slop, 0.0f) / inv_mass_sum) * percent;

    a.position -= correction * a.inv_mass;
    b.position += correction * b.inv_mass;

    // Relative velocity
    Vec2 rel_vel = b.velocity - a.velocity;
    float vel_along_normal = glm::dot(rel_vel, contact.normal);

    // Don't resolve if separating
    if (vel_along_normal > 0.0f) return;

    // Restitution
    float e = std::min(a.restitution, b.restitution);

    // Impulse magnitude
    float j = -(1.0f + e) * vel_along_normal / inv_mass_sum;

    Vec2 impulse = j * contact.normal;
    a.velocity -= impulse * a.inv_mass;
    b.velocity += impulse * b.inv_mass;

    // Friction
    Vec2 tangent = rel_vel - contact.normal * vel_along_normal;
    float tangent_len = glm::length(tangent);
    if (tangent_len > math::EPSILON) {
        tangent /= tangent_len;
        float jt = -glm::dot(rel_vel, tangent) / inv_mass_sum;
        float mu = std::sqrt(a.friction * b.friction);

        Vec2 friction_impulse;
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

bool PhysicsWorld2D::raycast(Vec2 origin, Vec2 direction, float max_distance,
                              RayHit2D& hit, u16 layer_mask) const {
    Vec2 dir = glm::normalize(direction);
    float closest = max_distance;
    bool found = false;

    for (const auto& b : bodies_) {
        if (!(b.layer & layer_mask)) continue;

        if (b.shape == Body2D::Circle) {
            Vec2 oc = origin - b.position;
            float a_coeff = glm::dot(dir, dir);
            float b_coeff = 2.0f * glm::dot(oc, dir);
            float c_coeff = glm::dot(oc, oc) - b.radius * b.radius;
            float disc = b_coeff * b_coeff - 4.0f * a_coeff * c_coeff;
            if (disc < 0.0f) continue;

            float t = (-b_coeff - std::sqrt(disc)) / (2.0f * a_coeff);
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
            Vec2 bmin = b.position - b.half_size;
            Vec2 bmax = b.position + b.half_size;

            float tmin_val = 0.0f, tmax_val = max_distance;
            for (int axis = 0; axis < 2; ++axis) {
                float inv_d = 1.0f / dir[axis];
                float t1 = (bmin[axis] - origin[axis]) * inv_d;
                float t2 = (bmax[axis] - origin[axis]) * inv_d;
                if (inv_d < 0.0f) std::swap(t1, t2);
                tmin_val = std::max(tmin_val, t1);
                tmax_val = std::min(tmax_val, t2);
                if (tmax_val < tmin_val) goto next_body;
            }

            if (tmin_val < closest) {
                closest = tmin_val;
                hit.body_id = b.id;
                hit.point = origin + dir * tmin_val;
                hit.distance = tmin_val;
                // Determine normal
                Vec2 p = hit.point - b.position;
                if (std::abs(p.x) > std::abs(p.y))
                    hit.normal = {(p.x > 0.0f) ? 1.0f : -1.0f, 0.0f};
                else
                    hit.normal = {0.0f, (p.y > 0.0f) ? 1.0f : -1.0f};
                found = true;
            }
        }
        next_body:;
    }
    return found;
}

std::vector<u32> PhysicsWorld2D::overlap_circle(Vec2 center, float radius,
                                                 u16 layer_mask) const {
    std::vector<u32> result;
    for (const auto& b : bodies_) {
        if (!(b.layer & layer_mask)) continue;

        if (b.shape == Body2D::Circle) {
            float dist = glm::length(b.position - center);
            if (dist < radius + b.radius) result.push_back(b.id);
        } else {
            // Circle vs AABB
            Vec2 closest;
            closest.x = math::clamp(center.x, b.position.x - b.half_size.x,
                                    b.position.x + b.half_size.x);
            closest.y = math::clamp(center.y, b.position.y - b.half_size.y,
                                    b.position.y + b.half_size.y);
            float dist_sq = glm::dot(center - closest, center - closest);
            if (dist_sq < radius * radius) result.push_back(b.id);
        }
    }
    return result;
}

std::vector<u32> PhysicsWorld2D::overlap_aabb(Vec2 min_pt, Vec2 max_pt,
                                               u16 layer_mask) const {
    std::vector<u32> result;
    for (const auto& b : bodies_) {
        if (!(b.layer & layer_mask)) continue;

        Vec2 bmin, bmax;
        body_aabb(b, bmin, bmax);

        if (bmax.x >= min_pt.x && bmin.x <= max_pt.x &&
            bmax.y >= min_pt.y && bmin.y <= max_pt.y) {
            result.push_back(b.id);
        }
    }
    return result;
}

} // namespace nexus::physics
