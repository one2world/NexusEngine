#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include "nexus/renderer/debug_renderer.h"
#include "nexus/physics/physics_world_3d.h"
#include "nexus/physics/physics_world_2d.h"

namespace nexus::physics {

// ─────────────────────────────────────────────────────────────────────────────
// PhysicsDebugDraw - draws physics shapes, contacts, and joints using
// the existing DebugRenderer infrastructure.
// ─────────────────────────────────────────────────────────────────────────────

class PhysicsDebugDraw {
public:
    explicit PhysicsDebugDraw(DebugRenderer& renderer) : renderer_(renderer) {}

    /// Draw all bodies in the 3D physics world.
    void draw_world(const PhysicsWorld3D& world) const;

    /// Draw all contacts (contact points and normals).
    void draw_contacts(const PhysicsWorld3D& world) const;

    /// Draw all joints/constraints.
    void draw_joints(const PhysicsWorld3D& world) const;

    // Color configuration
    Vec4 static_color{0.5f, 0.5f, 0.5f, 1.0f};    // gray
    Vec4 dynamic_color{0.2f, 0.8f, 0.2f, 1.0f};    // green
    Vec4 kinematic_color{0.2f, 0.4f, 0.9f, 1.0f};  // blue
    Vec4 sleeping_color{0.3f, 0.3f, 0.1f, 1.0f};   // dark yellow
    Vec4 trigger_color{1.0f, 1.0f, 0.0f, 0.5f};    // yellow semi-transparent
    Vec4 contact_color{1.0f, 0.0f, 0.0f, 1.0f};    // red
    Vec4 joint_color{0.0f, 1.0f, 1.0f, 1.0f};      // cyan

private:
    void draw_body(const Body3D& body) const;
    void draw_capsule_wireframe(Vec3 center, Quat rotation, float radius, float height, Vec4 color) const;
    Vec4 body_color(const Body3D& body) const;

    DebugRenderer& renderer_;
};

} // namespace nexus::physics
