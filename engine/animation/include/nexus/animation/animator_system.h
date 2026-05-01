#pragma once

#include "nexus/core/types.h"

#include <functional>

namespace nexus {
class Registry;
namespace anim { class AnimationClip; }
}

namespace nexus::anim {

// ─────────────────────────────────────────────────────────────────────────────
// AnimatorSystem — drives AnimatorComponent playback once per frame
// ─────────────────────────────────────────────────────────────────────────────
//
// Scope (M10):
//   • Walks every entity that has both AnimatorComponent and
//     Transform3DComponent.
//   • Resolves AnimationClip from `AnimatorComponent::clip_id` via a caller-
//     supplied resolver — keeps engine/animation independent of
//     editor/AnimationAssetCache.
//   • Advances `time` by `dt * speed` when `playing` is true; loops modulo
//     `clip.duration()` when `looping`; clamps to end and stops when not.
//   • Samples the clip's bone-0 channel and writes into the entity's
//     Transform3DComponent — the common "animate this object" workflow.
//
// The system is intentionally pure-data: no ImGui, no rendering, easy to
// test with a hand-built Registry + a synthetic clip.  Future work:
//   - SkeletonComponent + bone-array-output path for full character rigs.
//   - State machine integration (engine/animation/animation_state_machine).
//   - Event dispatch via AnimationEventCallback.
class AnimatorSystem {
public:
    /// Caller hands the system a way to look up AnimationClip pointers by
    /// the editor / runtime's clip id.  Returns nullptr for unknown ids;
    /// system treats this as "no clip bound" (no-op for that entity).
    using ClipResolver = std::function<const AnimationClip*(u32 clip_id)>;

    AnimatorSystem() = default;
    explicit AnimatorSystem(ClipResolver resolver)
        : resolver_(std::move(resolver)) {}

    void set_resolver(ClipResolver r) { resolver_ = std::move(r); }
    bool has_resolver() const { return static_cast<bool>(resolver_); }

    /// Advance every animator on the registry by `dt` seconds.  Safe to
    /// call with no resolver — animators just don't progress.  Counts the
    /// number of entities ticked for telemetry / tests.
    u32 tick(Registry& registry, f32 dt) const;

    /// One-shot helper: flip every animator's `playing` flag from
    /// `play_on_start` when the scene enters Play mode.  Idempotent —
    /// call again does nothing surprising because the animator overwrites
    /// `playing` directly.
    static u32 start_autoplay(Registry& registry);

private:
    ClipResolver resolver_;
};

}  // namespace nexus::anim
