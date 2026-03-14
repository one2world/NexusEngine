#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include "nexus/physics/physics_world_2d.h"
#include "nexus/physics/physics_world_3d.h"
#include "nexus/scene/registry.h"
#include <variant>
#include <functional>

namespace nexus::physics {

// ─────────────────────────────────────────────────────────────────────────────
// Unified raycast result (works for both 2D and 3D)
// ─────────────────────────────────────────────────────────────────────────────

struct RayHit {
    u32   entity{0};
    Vec3  point{0.0f};
    Vec3  normal{0.0f};
    float distance{0.0f};
    bool  is_3d{false};
};

// ─────────────────────────────────────────────────────────────────────────────
// PhysicsSystem - unified ECS-integrated physics for 2D and 3D
// ─────────────────────────────────────────────────────────────────────────────

class PhysicsSystem {
public:
    PhysicsSystem();

    /// Sync ECS components → physics bodies (call before stepping).
    void sync_to_physics(Registry& reg);

    /// Step both worlds with fixed timestep.
    void step(float dt);

    /// Sync physics bodies → ECS components (call after stepping).
    void sync_from_physics(Registry& reg);

    /// Full update: sync_to → step → sync_from (convenience).
    void update(Registry& reg, float dt);

    // ── Unified queries ─────────────────────────────────────────────────────

    /// Raycast in 2D (z is ignored).
    bool raycast_2d(Vec2 origin, Vec2 direction, float max_distance,
                    RayHit& hit, u16 layer_mask = 0xFFFF) const;

    /// Raycast in 3D.
    bool raycast_3d(Vec3 origin, Vec3 direction, float max_distance,
                    RayHit& hit, u16 layer_mask = 0xFFFF) const;

    // ── Configuration ───────────────────────────────────────────────────────

    void set_gravity_2d(Vec2 gravity) { world_2d_.set_gravity(gravity); }
    void set_gravity_3d(Vec3 gravity) { world_3d_.set_gravity(gravity); }

    void set_fixed_timestep(float dt) { fixed_timestep_ = dt; }
    float fixed_timestep() const { return fixed_timestep_; }

    // Direct access for advanced use
    PhysicsWorld2D& world_2d() { return world_2d_; }
    PhysicsWorld3D& world_3d() { return world_3d_; }
    const PhysicsWorld2D& world_2d() const { return world_2d_; }
    const PhysicsWorld3D& world_3d() const { return world_3d_; }

private:
    PhysicsWorld2D world_2d_;
    PhysicsWorld3D world_3d_;
    float fixed_timestep_{1.0f / 60.0f};
    float accumulator_{0.0f};

    // Entity → body ID mapping
    std::unordered_map<u32, u32> entity_to_body_2d_;
    std::unordered_map<u32, u32> entity_to_body_3d_;
    std::unordered_map<u32, u32> body_to_entity_2d_;
    std::unordered_map<u32, u32> body_to_entity_3d_;
};

} // namespace nexus::physics
