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

// ── Keyframe edit helpers (M23) ─────────────────────────────────────────────
//
// Maintain sorted-by-time invariant via insertion-point search.  Channel
// lookup is linear; clips with hundreds of bones would warrant a hash
// map but for typical character rigs (~60 bones) the linear walk is far
// cheaper than the indirection of a map.

BoneChannel& AnimationClip::ensure_channel(i32 bone_index) {
    if (bone_index < 0) bone_index = 0;
    for (auto& ch : channels_) {
        if (ch.bone_index == bone_index) return ch;
    }
    BoneChannel fresh;
    fresh.bone_index = bone_index;
    channels_.push_back(std::move(fresh));
    return channels_.back();
}

namespace {

template <typename Key>
void insert_sorted(std::vector<Key>& keys, Key k) {
    auto it = std::upper_bound(keys.begin(), keys.end(), k,
        [](const Key& a, const Key& b) { return a.time < b.time; });
    keys.insert(it, std::move(k));
}

template <typename Key>
bool remove_at(std::vector<Key>& keys, u32 idx) {
    if (idx >= keys.size()) return false;
    keys.erase(keys.begin() + idx);
    return true;
}

}  // namespace

void AnimationClip::add_position_key(i32 bone_index, float time, Vec3 value) {
    if (time < 0.0f) time = 0.0f;
    auto& ch = ensure_channel(bone_index);
    insert_sorted<PositionKey>(ch.positions, {time, value});
    if (time > duration_) duration_ = time;
}

void AnimationClip::add_rotation_key(i32 bone_index, float time, Quat value) {
    if (time < 0.0f) time = 0.0f;
    auto& ch = ensure_channel(bone_index);
    insert_sorted<RotationKey>(ch.rotations, {time, value});
    if (time > duration_) duration_ = time;
}

void AnimationClip::add_scale_key(i32 bone_index, float time, Vec3 value) {
    if (time < 0.0f) time = 0.0f;
    auto& ch = ensure_channel(bone_index);
    insert_sorted<ScaleKey>(ch.scales, {time, value});
    if (time > duration_) duration_ = time;
}

bool AnimationClip::remove_position_key(i32 bone_index, u32 key_index) {
    for (auto& ch : channels_) {
        if (ch.bone_index == bone_index) {
            return remove_at(ch.positions, key_index);
        }
    }
    return false;
}

bool AnimationClip::remove_rotation_key(i32 bone_index, u32 key_index) {
    for (auto& ch : channels_) {
        if (ch.bone_index == bone_index) {
            return remove_at(ch.rotations, key_index);
        }
    }
    return false;
}

bool AnimationClip::remove_scale_key(i32 bone_index, u32 key_index) {
    for (auto& ch : channels_) {
        if (ch.bone_index == bone_index) {
            return remove_at(ch.scales, key_index);
        }
    }
    return false;
}

void AnimationClip::add_event(float time, const std::string& event_name) {
    events_.push_back({time, event_name});
    // Keep sorted by time
    std::sort(events_.begin(), events_.end(),
              [](const AnimationEvent& a, const AnimationEvent& b) {
                  return a.time < b.time;
              });
}

void AnimationClip::collect_events(float prev_time, float curr_time, bool looping,
                                    std::vector<const AnimationEvent*>& out) const {
    if (events_.empty()) return;

    if (curr_time >= prev_time) {
        // Normal forward playback (no wrap)
        for (const auto& ev : events_) {
            if (ev.time > prev_time && ev.time <= curr_time) {
                out.push_back(&ev);
            }
        }
    } else if (looping) {
        // Wrapped around: fire events in [prev_time, duration) and [0, curr_time]
        for (const auto& ev : events_) {
            if (ev.time > prev_time || ev.time <= curr_time) {
                out.push_back(&ev);
            }
        }
    }
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
