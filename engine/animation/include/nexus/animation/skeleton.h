#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include <string>
#include <vector>

namespace nexus::anim {

// ─────────────────────────────────────────────────────────────────────────────
// Bone - a joint in the skeleton hierarchy
// ─────────────────────────────────────────────────────────────────────────────

struct Bone {
    std::string name;
    i32         parent_index{-1};   // -1 = root
    Vec3        local_position{0.0f};
    Quat        local_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    Vec3        local_scale{1.0f};
    Mat4        inverse_bind_pose{1.0f};
};

// ─────────────────────────────────────────────────────────────────────────────
// BonePose - runtime transform for a single bone
// ─────────────────────────────────────────────────────────────────────────────

struct BonePose {
    Vec3 position{0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f};

    [[nodiscard]] Mat4 to_matrix() const {
        Mat4 m(1.0f);
        m = glm::translate(m, position);
        m *= glm::toMat4(rotation);
        m = glm::scale(m, scale);
        return m;
    }

    /// Linearly interpolate between two poses.
    static BonePose lerp(const BonePose& a, const BonePose& b, float t) {
        BonePose result;
        result.position = math::lerp(a.position, b.position, t);
        result.rotation = glm::slerp(a.rotation, b.rotation, t);
        result.scale = math::lerp(a.scale, b.scale, t);
        return result;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Skeleton - a hierarchy of bones
// ─────────────────────────────────────────────────────────────────────────────

class Skeleton {
public:
    /// Add a bone. Returns the bone index.
    u32 add_bone(const Bone& bone);

    /// Find a bone by name. Returns -1 if not found.
    i32 find_bone(const std::string& name) const;

    /// Get the rest pose (bind pose local transforms).
    std::vector<BonePose> get_bind_pose() const;

    /// Compute world-space matrices from local bone poses.
    /// Result: bone_count matrices, each = parent_world * local * inverse_bind.
    std::vector<Mat4> compute_skin_matrices(const std::vector<BonePose>& local_poses) const;

    // Accessors
    u32 bone_count() const { return static_cast<u32>(bones_.size()); }
    const Bone& bone(u32 index) const { return bones_[index]; }
    const std::vector<Bone>& bones() const { return bones_; }

    static constexpr u32 MAX_BONES = 128;

private:
    std::vector<Bone> bones_;
};

} // namespace nexus::anim
