#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include "nexus/physics/spatial_hash.h"
#include <vector>
#include <functional>

namespace nexus::physics {

// ─────────────────────────────────────────────────────────────────────────────
// Collision result structures
// ─────────────────────────────────────────────────────────────────────────────

struct Contact2D {
    Vec2  point;
    Vec2  normal;
    float depth;
};

struct CollisionPair2D {
    u32 body_a;
    u32 body_b;
    Contact2D contact;
};

struct RayHit2D {
    u32   body_id;
    Vec2  point;
    Vec2  normal;
    float distance;
};

// ─────────────────────────────────────────────────────────────────────────────
// Body2D - internal physics body representation
// ─────────────────────────────────────────────────────────────────────────────

struct Body2D {
    enum Type : u8 { Static = 0, Dynamic = 1, Kinematic = 2 };
    enum Shape : u8 { Box = 0, Circle = 1 };

    // Identity
    u32 id{0};
    u32 entity{0};  // ECS entity this body belongs to

    // Type
    Type  type{Dynamic};
    Shape shape{Box};

    // Transform
    Vec2  position{0.0f, 0.0f};
    float rotation{0.0f};

    // Shape data
    Vec2  half_size{0.5f, 0.5f};  // for Box
    float radius{0.5f};           // for Circle

    // Material
    float density{1.0f};
    float friction{0.3f};
    float restitution{0.0f};

    // Dynamics
    float mass{1.0f};
    float inv_mass{1.0f};
    float inertia{1.0f};
    float inv_inertia{1.0f};
    Vec2  velocity{0.0f, 0.0f};
    float angular_velocity{0.0f};
    Vec2  force{0.0f, 0.0f};
    float torque{0.0f};
    float linear_damping{0.0f};
    float angular_damping{0.05f};
    float gravity_scale{1.0f};
    bool  fixed_rotation{false};

    // Collision
    bool is_trigger{false};
    u16  layer{1};
    u16  mask{0xFFFF};

    // Sleeping
    bool  sleeping{false};
    float sleep_timer{0.0f};
    static constexpr float SLEEP_THRESHOLD = 0.005f;
    static constexpr float SLEEP_TIME      = 0.5f;

    void compute_mass();
    void wake() { sleeping = false; sleep_timer = 0.0f; }
};

// ─────────────────────────────────────────────────────────────────────────────
// PhysicsWorld2D - the 2D simulation
// ─────────────────────────────────────────────────────────────────────────────

using ContactCallback2D = std::function<void(const CollisionPair2D&)>;

class PhysicsWorld2D {
public:
    explicit PhysicsWorld2D(Vec2 gravity = {0.0f, -9.81f});

    // Body management
    u32  create_body(const Body2D& desc);
    void destroy_body(u32 id);
    Body2D*       get_body(u32 id);
    const Body2D* get_body(u32 id) const;

    // Simulation
    void step(float dt, u32 velocity_iterations = 8, u32 position_iterations = 3);

    // Forces
    void apply_force(u32 id, Vec2 force);
    void apply_impulse(u32 id, Vec2 impulse);
    void apply_torque(u32 id, float torque);

    // Queries
    bool raycast(Vec2 origin, Vec2 direction, float max_distance,
                 RayHit2D& hit, u16 layer_mask = 0xFFFF) const;
    std::vector<u32> overlap_circle(Vec2 center, float radius,
                                     u16 layer_mask = 0xFFFF) const;
    std::vector<u32> overlap_aabb(Vec2 min, Vec2 max,
                                   u16 layer_mask = 0xFFFF) const;

    // Callbacks
    void set_contact_callback(ContactCallback2D cb) { contact_callback_ = std::move(cb); }

    // Configuration
    void set_gravity(Vec2 gravity) { gravity_ = gravity; }
    Vec2 gravity() const { return gravity_; }

    // Access
    const std::vector<Body2D>& bodies() const { return bodies_; }
    const std::vector<CollisionPair2D>& contacts() const { return contacts_; }

private:
    void integrate(float dt);
    void broadphase();
    bool narrowphase(const Body2D& a, const Body2D& b, Contact2D& contact) const;
    void resolve_collision(Body2D& a, Body2D& b, const Contact2D& contact);
    void update_sleeping(float dt);

    // Narrow-phase helpers
    bool circle_vs_circle(const Body2D& a, const Body2D& b, Contact2D& c) const;
    bool box_vs_box(const Body2D& a, const Body2D& b, Contact2D& c) const;
    bool circle_vs_box(const Body2D& circle, const Body2D& box, Contact2D& c) const;

    Vec2 gravity_;
    std::vector<Body2D> bodies_;
    std::vector<CollisionPair2D> contacts_;
    ContactCallback2D contact_callback_;
    u32 next_id_{1};
    mutable SpatialHash2D spatial_hash_{2.0f};
};

} // namespace nexus::physics
