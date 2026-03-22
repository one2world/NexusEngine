#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include <vector>
#include <functional>

namespace nexus::physics {

// ─────────────────────────────────────────────────────────────────────────────
// 3D collision structures
// ─────────────────────────────────────────────────────────────────────────────

struct Contact3D {
    Vec3  point;
    Vec3  normal;
    float depth;
};

struct CollisionPair3D {
    u32 body_a;
    u32 body_b;
    Contact3D contact;
};

struct RayHit3D {
    u32   body_id;
    Vec3  point;
    Vec3  normal;
    float distance;
};

// ─────────────────────────────────────────────────────────────────────────────
// Body3D - internal physics body
// ─────────────────────────────────────────────────────────────────────────────

struct Body3D {
    enum Type : u8 { Static = 0, Dynamic = 1, Kinematic = 2 };
    enum Shape : u8 { Box = 0, Sphere = 1, Capsule = 2 };

    u32   id{0};
    u32   entity{0};
    Type  type{Dynamic};
    Shape shape{Box};

    // Transform
    Vec3  position{0.0f};
    Quat  rotation{1.0f, 0.0f, 0.0f, 0.0f};

    // Shape
    Vec3  half_extents{0.5f};    // Box
    float radius{0.5f};          // Sphere/Capsule
    float height{1.0f};          // Capsule

    // Material
    float mass{1.0f};
    float inv_mass{1.0f};
    Mat3  inertia_tensor{1.0f};
    Mat3  inv_inertia_tensor{0.0f};
    float friction{0.5f};
    float restitution{0.3f};

    // Dynamics
    Vec3  velocity{0.0f};
    Vec3  angular_velocity{0.0f};
    Vec3  force{0.0f};
    Vec3  torque_accum{0.0f};
    float linear_damping{0.0f};
    float angular_damping{0.05f};
    float gravity_scale{1.0f};

    // Collision
    bool  is_trigger{false};
    u16   layer{1};
    u16   mask{0xFFFF};

    void compute_mass();
};

// ─────────────────────────────────────────────────────────────────────────────
// PhysicsWorld3D
// ─────────────────────────────────────────────────────────────────────────────

using ContactCallback3D = std::function<void(const CollisionPair3D&)>;

class PhysicsWorld3D {
public:
    explicit PhysicsWorld3D(Vec3 gravity = {0.0f, -9.81f, 0.0f});

    // Body management
    u32  create_body(const Body3D& desc);
    void destroy_body(u32 id);
    Body3D*       get_body(u32 id);
    const Body3D* get_body(u32 id) const;

    // Simulation
    void step(float dt, u32 iterations = 8);

    // Forces
    void apply_force(u32 id, Vec3 force);
    void apply_impulse(u32 id, Vec3 impulse);
    void apply_torque(u32 id, Vec3 torque);

    // Queries
    bool raycast(Vec3 origin, Vec3 direction, float max_distance,
                 RayHit3D& hit, u16 layer_mask = 0xFFFF) const;
    std::vector<u32> overlap_sphere(Vec3 center, float radius,
                                     u16 layer_mask = 0xFFFF) const;
    std::vector<u32> overlap_aabb(Vec3 min, Vec3 max,
                                   u16 layer_mask = 0xFFFF) const;

    // Callbacks
    void set_contact_callback(ContactCallback3D cb) { contact_callback_ = std::move(cb); }

    // Config
    void set_gravity(Vec3 gravity) { gravity_ = gravity; }
    Vec3 gravity() const { return gravity_; }

    const std::vector<Body3D>& bodies() const { return bodies_; }
    const std::vector<CollisionPair3D>& contacts() const { return contacts_; }

private:
    void integrate(float dt);
    void broadphase();
    bool narrowphase(const Body3D& a, const Body3D& b, Contact3D& contact) const;
    void resolve_collision(Body3D& a, Body3D& b, const Contact3D& contact);

    bool sphere_vs_sphere(const Body3D& a, const Body3D& b, Contact3D& c) const;
    bool box_vs_box(const Body3D& a, const Body3D& b, Contact3D& c) const;
    bool sphere_vs_box(const Body3D& sphere, const Body3D& box, Contact3D& c) const;

    Vec3 gravity_;
    std::vector<Body3D> bodies_;
    std::vector<CollisionPair3D> contacts_;
    ContactCallback3D contact_callback_;
    u32 next_id_{1};
};

} // namespace nexus::physics
