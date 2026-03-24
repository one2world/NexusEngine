#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include <vector>
#include <memory>

namespace nexus::physics {

struct Body3D; // forward

// ─────────────────────────────────────────────────────────────────────────────
// Constraint - base class for physics constraints/joints
// ─────────────────────────────────────────────────────────────────────────────

class Constraint {
public:
    virtual ~Constraint() = default;

    /// Prepare constraint data for the current frame.
    virtual void prepare(float dt) = 0;

    /// Solve velocity constraints (called iteratively).
    virtual void solve() = 0;

    u32 id{0};
    u32 body_a_id{0};
    u32 body_b_id{0};
    Body3D* body_a{nullptr};
    Body3D* body_b{nullptr};
    bool enabled{true};
};

// ─────────────────────────────────────────────────────────────────────────────
// DistanceJoint - maintains a fixed distance between two body anchor points
// ─────────────────────────────────────────────────────────────────────────────

class DistanceJoint : public Constraint {
public:
    Vec3  anchor_a{0.0f};       // local-space anchor on body A
    Vec3  anchor_b{0.0f};       // local-space anchor on body B
    float distance{1.0f};       // target distance
    float stiffness{1.0f};      // 0..1 compliance (1 = rigid)
    float damping{0.0f};

    void prepare(float dt) override;
    void solve() override;

private:
    Vec3  axis_{0.0f};
    float effective_mass_{0.0f};
    float bias_{0.0f};
    float impulse_{0.0f};
    float dt_{0.0f};
};

// ─────────────────────────────────────────────────────────────────────────────
// HingeJoint - constrains two bodies to rotate around a shared axis
// ─────────────────────────────────────────────────────────────────────────────

class HingeJoint : public Constraint {
public:
    Vec3  anchor_a{0.0f};
    Vec3  anchor_b{0.0f};
    Vec3  axis{0.0f, 1.0f, 0.0f};   // hinge axis in world space
    bool  enable_limits{false};
    float lower_limit{0.0f};         // radians
    float upper_limit{0.0f};

    void prepare(float dt) override;
    void solve() override;

private:
    Vec3  r_a_{0.0f}, r_b_{0.0f};
    float effective_mass_{0.0f};
    float bias_{0.0f};
    Vec3  impulse_{0.0f};
    float dt_{0.0f};
};

// ─────────────────────────────────────────────────────────────────────────────
// BallJoint - constrains two anchor points to the same world position
// ─────────────────────────────────────────────────────────────────────────────

class BallJoint : public Constraint {
public:
    Vec3 anchor_a{0.0f};
    Vec3 anchor_b{0.0f};

    void prepare(float dt) override;
    void solve() override;

private:
    Vec3  r_a_{0.0f}, r_b_{0.0f};
    Mat3  effective_mass_{0.0f};
    Vec3  bias_{0.0f};
    Vec3  impulse_{0.0f};
    float dt_{0.0f};
};

// ─────────────────────────────────────────────────────────────────────────────
// SpringJoint - spring-damper force between two anchor points
// ─────────────────────────────────────────────────────────────────────────────

class SpringJoint : public Constraint {
public:
    Vec3  anchor_a{0.0f};
    Vec3  anchor_b{0.0f};
    float rest_length{1.0f};
    float stiffness{10.0f};
    float damping{0.5f};

    void prepare(float dt) override;
    void solve() override;

private:
    float dt_{0.0f};
};

} // namespace nexus::physics
