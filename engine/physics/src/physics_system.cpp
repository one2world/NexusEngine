#include "nexus/physics/physics_system.h"
#include "nexus/scene/components.h"
#include "nexus/core/log.h"

namespace nexus::physics {

PhysicsSystem::PhysicsSystem()
    : world_2d_({0.0f, -9.81f})
    , world_3d_({0.0f, -9.81f, 0.0f}) {}

// ── Sync ECS → Physics ─────────────────────────────────────────────────────

void PhysicsSystem::sync_to_physics(Registry& reg) {
    // 2D bodies
    reg.each<RigidBody2DComponent, Transform2DComponent>(
        [&](Entity e, RigidBody2DComponent& rb, Transform2DComponent& t) {
            auto it = entity_to_body_2d_.find(e);
            if (it == entity_to_body_2d_.end()) {
                // Create new body
                Body2D desc;
                desc.entity = e;
                desc.type = static_cast<Body2D::Type>(rb.type);
                desc.position = t.position;
                desc.rotation = t.rotation;
                desc.density = rb.density;
                desc.friction = rb.friction;
                desc.restitution = rb.restitution;
                desc.linear_damping = rb.linear_damping;
                desc.angular_damping = rb.angular_damping;
                desc.gravity_scale = rb.gravity_scale;
                desc.fixed_rotation = rb.fixed_rotation;
                desc.velocity = rb.velocity;
                desc.angular_velocity = rb.angular_velocity;

                // Check for collider
                if (reg.has_component<Collider2DComponent>(e)) {
                    auto& col = reg.get_component<Collider2DComponent>(e);
                    desc.shape = static_cast<Body2D::Shape>(col.shape);
                    desc.half_size = col.half_size;
                    desc.radius = col.radius;
                    desc.is_trigger = col.is_trigger;
                    desc.layer = col.layer;
                    desc.mask = col.mask;
                } else {
                    desc.shape = Body2D::Box;
                    desc.half_size = t.scale * 0.5f;
                }

                u32 body_id = world_2d_.create_body(desc);
                entity_to_body_2d_[e] = body_id;
                body_to_entity_2d_[body_id] = e;
            } else {
                // Update existing body
                if (auto* body = world_2d_.get_body(it->second)) {
                    if (rb.type == RigidBody2DComponent::Kinematic) {
                        body->position = t.position;
                        body->rotation = t.rotation;
                    }
                }
            }
        });

    // 3D bodies
    reg.each<RigidBody3DComponent, Transform3DComponent>(
        [&](Entity e, RigidBody3DComponent& rb, Transform3DComponent& t) {
            auto it = entity_to_body_3d_.find(e);
            if (it == entity_to_body_3d_.end()) {
                Body3D desc;
                desc.entity = e;
                desc.type = static_cast<Body3D::Type>(rb.type);
                desc.position = t.position;
                desc.rotation = t.rotation;
                desc.mass = rb.mass;
                desc.friction = rb.friction;
                desc.restitution = rb.restitution;
                desc.linear_damping = rb.linear_damping;
                desc.angular_damping = rb.angular_damping;
                desc.gravity_scale = rb.gravity_scale;
                desc.velocity = rb.velocity;
                desc.angular_velocity = rb.angular_velocity;

                if (reg.has_component<Collider3DComponent>(e)) {
                    auto& col = reg.get_component<Collider3DComponent>(e);
                    desc.shape = static_cast<Body3D::Shape>(col.shape);
                    desc.half_extents = col.half_extents;
                    desc.radius = col.radius;
                    desc.height = col.height;
                    desc.is_trigger = col.is_trigger;
                    desc.layer = col.layer;
                    desc.mask = col.mask;
                } else {
                    desc.shape = Body3D::Box;
                    desc.half_extents = t.scale * 0.5f;
                }

                u32 body_id = world_3d_.create_body(desc);
                entity_to_body_3d_[e] = body_id;
                body_to_entity_3d_[body_id] = e;
            } else {
                if (auto* body = world_3d_.get_body(it->second)) {
                    if (rb.type == RigidBody3DComponent::Kinematic) {
                        body->position = t.position;
                        body->rotation = t.rotation;
                    }
                }
            }
        });
}

// ── Step ────────────────────────────────────────────────────────────────────

void PhysicsSystem::step(float dt) {
    accumulator_ += dt;

    while (accumulator_ >= fixed_timestep_) {
        world_2d_.step(fixed_timestep_);
        world_3d_.step(fixed_timestep_);
        accumulator_ -= fixed_timestep_;
    }
}

// ── Sync Physics → ECS ─────────────────────────────────────────────────────

void PhysicsSystem::sync_from_physics(Registry& reg) {
    // 2D
    for (const auto& [entity_id, body_id] : entity_to_body_2d_) {
        Entity e = entity_id;
        if (!reg.has_component<Transform2DComponent>(e)) continue;
        if (!reg.has_component<RigidBody2DComponent>(e)) continue;

        const auto* body = world_2d_.get_body(body_id);
        if (!body) continue;

        auto& t = reg.get_component<Transform2DComponent>(e);
        auto& rb = reg.get_component<RigidBody2DComponent>(e);

        t.position = body->position;
        t.rotation = body->rotation;
        rb.velocity = body->velocity;
        rb.angular_velocity = body->angular_velocity;
    }

    // 3D
    for (const auto& [entity_id, body_id] : entity_to_body_3d_) {
        Entity e = entity_id;
        if (!reg.has_component<Transform3DComponent>(e)) continue;
        if (!reg.has_component<RigidBody3DComponent>(e)) continue;

        const auto* body = world_3d_.get_body(body_id);
        if (!body) continue;

        auto& t = reg.get_component<Transform3DComponent>(e);
        auto& rb = reg.get_component<RigidBody3DComponent>(e);

        t.position = body->position;
        t.rotation = body->rotation;
        rb.velocity = body->velocity;
        rb.angular_velocity = body->angular_velocity;
    }
}

// ── Convenience ─────────────────────────────────────────────────────────────

void PhysicsSystem::update(Registry& reg, float dt) {
    sync_to_physics(reg);
    step(dt);
    sync_from_physics(reg);
}

// ── Unified queries ─────────────────────────────────────────────────────────

bool PhysicsSystem::raycast_2d(Vec2 origin, Vec2 direction, float max_distance,
                                RayHit& hit, u16 layer_mask) const {
    RayHit2D hit2d;
    if (world_2d_.raycast(origin, direction, max_distance, hit2d, layer_mask)) {
        auto it = body_to_entity_2d_.find(hit2d.body_id);
        hit.entity = (it != body_to_entity_2d_.end()) ? it->second : 0;
        hit.point = Vec3(hit2d.point, 0.0f);
        hit.normal = Vec3(hit2d.normal, 0.0f);
        hit.distance = hit2d.distance;
        hit.is_3d = false;
        return true;
    }
    return false;
}

bool PhysicsSystem::raycast_3d(Vec3 origin, Vec3 direction, float max_distance,
                                RayHit& hit, u16 layer_mask) const {
    RayHit3D hit3d;
    if (world_3d_.raycast(origin, direction, max_distance, hit3d, layer_mask)) {
        auto it = body_to_entity_3d_.find(hit3d.body_id);
        hit.entity = (it != body_to_entity_3d_.end()) ? it->second : 0;
        hit.point = hit3d.point;
        hit.normal = hit3d.normal;
        hit.distance = hit3d.distance;
        hit.is_3d = true;
        return true;
    }
    return false;
}

} // namespace nexus::physics
