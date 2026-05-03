#pragma once

#include "nexus/core/types.h"
#include "nexus/animation/skeleton.h"
#include <string>
#include <vector>
#include <functional>

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

// ─────────────────────────────────────────────────────────────────────────────
// AnimationEvent - a named event at a specific time in a clip
// ─────────────────────────────────────────────────────────────────────────────

struct AnimationEvent {
    float       time{0.0f};
    std::string name;
};

using AnimationEventCallback = std::function<void(const std::string& event_name)>;

class AnimationClip {
public:
    AnimationClip() = default;
    AnimationClip(const std::string& name, float duration)
        : name_(name), duration_(duration) {}

    void add_channel(BoneChannel channel);

    /// Add an event at a specific time.
    void add_event(float time, const std::string& event_name);

    /// Collect events that fire when playback advances from prev_time to curr_time.
    /// Handles looping: if curr_time < prev_time, fires events in [prev_time, duration) + [0, curr_time).
    void collect_events(float prev_time, float curr_time, bool looping,
                        std::vector<const AnimationEvent*>& out) const;

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
    void set_name(std::string n) { name_ = std::move(n); }

    float duration() const { return duration_; }
    /// Editor authoring uses this to extend a clip when a keyframe is
    /// added past the current end.  Runtime sampling clamps `time` to
    /// `duration` already, so shrinking is safe; growing widens the
    /// authored window.
    void set_duration(float d) { duration_ = d < 0.0f ? 0.0f : d; }

    const std::vector<BoneChannel>& channels() const { return channels_; }
    /// Mutable accessor — used by the Animation Window's editing path
    /// (M23) to insert / remove keyframes in place.  Direct ref because
    /// editor edits are a hot path and copying a vector of channels per
    /// keystroke would be wasteful.
    std::vector<BoneChannel>& channels() { return channels_; }

    const std::vector<AnimationEvent>& events() const { return events_; }
    std::vector<AnimationEvent>& events() { return events_; }

    // ── Keyframe edit helpers (M23) ──────────────────────────────────────
    //
    // Pure functions for the dopesheet's edit toolbar.  Inserts maintain
    // sorted-by-time invariant so Sample() can keep using upper_bound or
    // a linear walk.  Removal is indexed; out-of-range indices are
    // silently ignored.
    enum class KeyType : u8 { Position, Rotation, Scale };

    /// Find an existing channel by bone index, or create one if missing.
    /// Returns the channel reference for chained edits.  bone_index < 0
    /// is normalised to 0.
    BoneChannel& ensure_channel(i32 bone_index);

    /// Insert a position key at `time` for `bone_index`, maintaining the
    /// sorted-by-time invariant.  Updates duration to max(duration, time).
    void add_position_key(i32 bone_index, float time, Vec3 value);

    /// Same shape for rotation / scale.
    void add_rotation_key(i32 bone_index, float time, Quat value);
    void add_scale_key   (i32 bone_index, float time, Vec3 value);

    /// Remove a key by position in the channel's per-component vector.
    /// Returns true if removal happened.  Out-of-range and empty-channel
    /// requests are silent no-ops.
    bool remove_position_key(i32 bone_index, u32 key_index);
    bool remove_rotation_key(i32 bone_index, u32 key_index);
    bool remove_scale_key   (i32 bone_index, u32 key_index);

private:
    std::string name_;
    float duration_{0.0f};
    std::vector<BoneChannel> channels_;
    std::vector<AnimationEvent> events_;
};

} // namespace nexus::anim
