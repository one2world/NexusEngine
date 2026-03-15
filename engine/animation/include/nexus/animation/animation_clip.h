#pragma once

#include "nexus/core/types.h"
#include "nexus/animation/skeleton.h"
#include <string>
#include <vector>

namespace nexus::anim {

// ─────────────────────────────────────────────────────────────────────────────
// KeyFrame - a single keyframe for one channel (position, rotation, or scale)
// ─────────────────────────────────────────────────────────────────────────────

template <typename T>
struct KeyFrame {
    float time{0.0f};
    T     value{};
};

using PositionKey = KeyFrame<Vec3>;
using RotationKey = KeyFrame<Quat>;
using ScaleKey    = KeyFrame<Vec3>;

// ─────────────────────────────────────────────────────────────────────────────
// BoneChannel - keyframes for one bone in an animation
// ─────────────────────────────────────────────────────────────────────────────

struct BoneChannel {
    i32 bone_index{-1};
    std::vector<PositionKey> positions;
    std::vector<RotationKey> rotations;
    std::vector<ScaleKey>    scales;

    /// Sample the channel at a given time, producing a BonePose.
    BonePose sample(float time) const;
};

// ─────────────────────────────────────────────────────────────────────────────
// AnimationClip - a set of bone channels over time
// ─────────────────────────────────────────────────────────────────────────────

class AnimationClip {
public:
    AnimationClip() = default;
    AnimationClip(const std::string& name, float duration)
        : name_(name), duration_(duration) {}

    void add_channel(BoneChannel channel);

    /// Sample all channels at a given time. Output: array of BonePose (one per bone).
    /// bones not present in the clip retain the provided default_pose.
    void sample(float time, std::vector<BonePose>& out_poses) const;

    /// Blend two sampled pose arrays together.
    static void blend(const std::vector<BonePose>& a,
                      const std::vector<BonePose>& b,
                      float weight,
                      std::vector<BonePose>& out);

    /// Additive blend: out = base + (additive - reference) * weight.
    static void blend_additive(const std::vector<BonePose>& base,
                               const std::vector<BonePose>& additive,
                               const std::vector<BonePose>& reference,
                               float weight,
                               std::vector<BonePose>& out);

    // Accessors
    const std::string& name() const { return name_; }
    float duration() const { return duration_; }
    const std::vector<BoneChannel>& channels() const { return channels_; }

private:
    std::string name_;
    float duration_{0.0f};
    std::vector<BoneChannel> channels_;
};

} // namespace nexus::anim
