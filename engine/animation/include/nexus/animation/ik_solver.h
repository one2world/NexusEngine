#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include "nexus/animation/skeleton.h"
#include <vector>

namespace nexus::anim {

// ─────────────────────────────────────────────────────────────────────────────
// TwoBoneIK — analytical two-bone IK solver (e.g., arm: shoulder→elbow→hand)
// ─────────────────────────────────────────────────────────────────────────────

struct TwoBoneIKResult {
    Quat root_rotation{1, 0, 0, 0};
    Quat mid_rotation{1, 0, 0, 0};
    bool reached{false};
};

class TwoBoneIK {
public:
    /// Solve two-bone IK analytically.
    /// root_pos: position of the root joint (e.g., shoulder)
    /// mid_pos: position of the middle joint (e.g., elbow)
    /// end_pos: position of the end effector (e.g., hand)
    /// target: desired end effector position
    /// pole_target: hint direction for the bend plane (e.g., elbow direction)
    static TwoBoneIKResult solve(Vec3 root_pos, Vec3 mid_pos, Vec3 end_pos,
                                  Vec3 target, Vec3 pole_target);
};

// ─────────────────────────────────────────────────────────────────────────────
// FABRIKSolver — Forward And Backward Reaching Inverse Kinematics
// Solves an arbitrary-length chain to reach a target
// ─────────────────────────────────────────────────────────────────────────────

class FABRIKSolver {
public:
    FABRIKSolver() = default;

    /// Set the joint positions of the chain.
    void set_chain(const std::vector<Vec3>& positions);

    /// Set tolerance for convergence.
    void set_tolerance(float tol) { tolerance_ = tol; }

    /// Set maximum iterations.
    void set_max_iterations(u32 max_iter) { max_iterations_ = max_iter; }

    /// Solve toward the target. Returns true if reached within tolerance.
    bool solve(Vec3 target);

    /// Get the solved joint positions.
    const std::vector<Vec3>& positions() const { return positions_; }

private:
    std::vector<Vec3> positions_;
    std::vector<float> lengths_;
    float total_length_{0.0f};
    float tolerance_{0.001f};
    u32 max_iterations_{10};
};

} // namespace nexus::anim
