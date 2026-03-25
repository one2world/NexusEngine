#include "nexus/animation/ik_solver.h"
#include <cmath>
#include <algorithm>

namespace nexus::anim {

// ── TwoBoneIK ─────────────────────────────────────────────────────────────────

TwoBoneIKResult TwoBoneIK::solve(Vec3 root_pos, Vec3 mid_pos, Vec3 end_pos,
                                  Vec3 target, Vec3 pole_target) {
    TwoBoneIKResult result;

    float upper_len = glm::length(mid_pos - root_pos);
    float lower_len = glm::length(end_pos - mid_pos);
    float target_dist = glm::length(target - root_pos);

    if (upper_len < math::EPSILON || lower_len < math::EPSILON) {
        return result;
    }

    // Clamp target distance to reachable range
    float max_reach = upper_len + lower_len;
    float min_reach = std::abs(upper_len - lower_len);
    bool clamped = false;
    if (target_dist > max_reach - math::EPSILON) {
        target_dist = max_reach - math::EPSILON;
        clamped = true;
    } else if (target_dist < min_reach + math::EPSILON) {
        target_dist = min_reach + math::EPSILON;
        clamped = true;
    }
    result.reached = !clamped;

    // Direction from root to target
    Vec3 target_dir = glm::normalize(target - root_pos);

    // Law of cosines: angle at root joint
    float cos_root = (upper_len * upper_len + target_dist * target_dist -
                      lower_len * lower_len) / (2.0f * upper_len * target_dist);
    cos_root = math::clamp(cos_root, -1.0f, 1.0f);
    float root_angle = std::acos(cos_root);

    // Law of cosines: angle at mid joint
    float cos_mid = (upper_len * upper_len + lower_len * lower_len -
                     target_dist * target_dist) / (2.0f * upper_len * lower_len);
    cos_mid = math::clamp(cos_mid, -1.0f, 1.0f);
    float mid_angle = std::acos(cos_mid);

    // Build bend plane from pole target
    Vec3 pole_dir = pole_target - root_pos;
    Vec3 chain_plane_normal = glm::cross(target_dir, pole_dir);
    if (glm::length(chain_plane_normal) < math::EPSILON) {
        // Pole and target are collinear — pick arbitrary perpendicular
        chain_plane_normal = glm::cross(target_dir, Vec3(0, 1, 0));
        if (glm::length(chain_plane_normal) < math::EPSILON)
            chain_plane_normal = glm::cross(target_dir, Vec3(1, 0, 0));
    }
    chain_plane_normal = glm::normalize(chain_plane_normal);

    // Root rotation: rotate upper bone toward target with root_angle offset
    result.root_rotation = glm::angleAxis(root_angle, chain_plane_normal) *
                            glm::rotation(glm::normalize(mid_pos - root_pos), target_dir);

    // Mid rotation: bend by pi - mid_angle
    float bend = 3.14159265f - mid_angle;
    result.mid_rotation = glm::angleAxis(bend, chain_plane_normal);

    return result;
}

// ── FABRIKSolver ──────────────────────────────────────────────────────────────

void FABRIKSolver::set_chain(const std::vector<Vec3>& positions) {
    positions_ = positions;
    lengths_.clear();
    total_length_ = 0.0f;
    for (size_t i = 1; i < positions_.size(); ++i) {
        float len = glm::length(positions_[i] - positions_[i - 1]);
        lengths_.push_back(len);
        total_length_ += len;
    }
}

bool FABRIKSolver::solve(Vec3 target) {
    if (positions_.size() < 2) return false;

    Vec3 origin = positions_.front();
    float dist_to_target = glm::length(target - origin);

    // Unreachable
    if (dist_to_target > total_length_) {
        Vec3 dir = glm::normalize(target - origin);
        for (size_t i = 1; i < positions_.size(); ++i) {
            positions_[i] = positions_[i - 1] + dir * lengths_[i - 1];
        }
        return false;
    }

    size_t n = positions_.size();

    for (u32 iter = 0; iter < max_iterations_; ++iter) {
        float end_dist = glm::length(positions_[n - 1] - target);
        if (end_dist < tolerance_) return true;

        // Forward pass: set end to target, work backward
        positions_[n - 1] = target;
        for (size_t i = n - 2; i < n; --i) { // wraps around for size_t
            Vec3 dir = glm::normalize(positions_[i] - positions_[i + 1]);
            positions_[i] = positions_[i + 1] + dir * lengths_[i];
            if (i == 0) break;
        }

        // Backward pass: set root to origin, work forward
        positions_[0] = origin;
        for (size_t i = 1; i < n; ++i) {
            Vec3 dir = glm::normalize(positions_[i] - positions_[i - 1]);
            positions_[i] = positions_[i - 1] + dir * lengths_[i - 1];
        }
    }

    return glm::length(positions_[n - 1] - target) < tolerance_;
}

} // namespace nexus::anim
