#include "nexus/animation/animation_clip.h"
#include <algorithm>

namespace nexus::anim {

// ── Keyframe interpolation helpers ──────────────────────────────────────────

template <typename T, typename KeyType>
static T sample_keys(const std::vector<KeyType>& keys, float time, const T& default_val) {
    if (keys.empty()) return default_val;
    if (keys.size() == 1) return keys[0].value;

    // Before first key
    if (time <= keys.front().time) return keys.front().value;
    // After last key
    if (time >= keys.back().time) return keys.back().value;

    // Find two bounding keys
    for (size_t i = 0; i + 1 < keys.size(); ++i) {
        if (time >= keys[i].time && time <= keys[i + 1].time) {
            float t = (time - keys[i].time) / (keys[i + 1].time - keys[i].time);
            return math::lerp(keys[i].value, keys[i + 1].value, t);
        }
    }

    return keys.back().value;
}

// Specialization for quaternion (use slerp)
static Quat sample_rotation_keys(const std::vector<RotationKey>& keys, float time) {
    Quat default_q{1.0f, 0.0f, 0.0f, 0.0f};
    if (keys.empty()) return default_q;
    if (keys.size() == 1) return keys[0].value;
    if (time <= keys.front().time) return keys.front().value;
    if (time >= keys.back().time) return keys.back().value;

    for (size_t i = 0; i + 1 < keys.size(); ++i) {
        if (time >= keys[i].time && time <= keys[i + 1].time) {
            float t = (time - keys[i].time) / (keys[i + 1].time - keys[i].time);
            return glm::slerp(keys[i].value, keys[i + 1].value, t);
        }
    }
    return keys.back().value;
}

// ── BoneChannel ─────────────────────────────────────────────────────────────

BonePose BoneChannel::sample(float time) const {
    BonePose pose;
    pose.position = sample_keys<Vec3>(positions, time, Vec3(0.0f));
    pose.rotation = sample_rotation_keys(rotations, time);
    pose.scale    = sample_keys<Vec3>(scales, time, Vec3(1.0f));
    return pose;
}

// ── AnimationClip ───────────────────────────────────────────────────────────

void AnimationClip::add_channel(BoneChannel channel) {
    channels_.push_back(std::move(channel));
}

void AnimationClip::sample(float time, std::vector<BonePose>& out_poses) const {
    for (const auto& channel : channels_) {
        if (channel.bone_index >= 0 &&
            channel.bone_index < static_cast<i32>(out_poses.size())) {
            out_poses[static_cast<size_t>(channel.bone_index)] = channel.sample(time);
        }
    }
}

void AnimationClip::blend(const std::vector<BonePose>& a,
                           const std::vector<BonePose>& b,
                           float weight,
                           std::vector<BonePose>& out) {
    size_t count = std::min(a.size(), b.size());
    out.resize(count);
    for (size_t i = 0; i < count; ++i) {
        out[i] = BonePose::lerp(a[i], b[i], weight);
    }
}

void AnimationClip::blend_additive(const std::vector<BonePose>& base,
                                    const std::vector<BonePose>& additive,
                                    const std::vector<BonePose>& reference,
                                    float weight,
                                    std::vector<BonePose>& out) {
    size_t count = std::min({base.size(), additive.size(), reference.size()});
    out.resize(count);
    for (size_t i = 0; i < count; ++i) {
        Vec3 delta_pos = (additive[i].position - reference[i].position) * weight;
        Quat delta_rot = glm::slerp(Quat(1, 0, 0, 0),
                                     additive[i].rotation * glm::inverse(reference[i].rotation),
                                     weight);
        Vec3 delta_scale = Vec3(1.0f) + (additive[i].scale - reference[i].scale) * weight;

        out[i].position = base[i].position + delta_pos;
        out[i].rotation = delta_rot * base[i].rotation;
        out[i].scale = base[i].scale * delta_scale;
    }
}

} // namespace nexus::anim
