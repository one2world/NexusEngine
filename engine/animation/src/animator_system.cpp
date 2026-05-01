#include "nexus/animation/animator_system.h"

#include "nexus/animation/animation_clip.h"
#include "nexus/scene/components.h"
#include "nexus/scene/registry.h"

#include <vector>

namespace nexus::anim {

u32 AnimatorSystem::tick(Registry& registry, f32 dt) const {
    u32 ticked = 0;

    // Pre-allocate the pose buffer once and resize per-clip — avoids a
    // heap alloc per entity.  Bone 0 is the convention: clips authored
    // for "single transform" entities use channel index 0; full skeletal
    // rigs author higher indices for additional bones (sampled but
    // ignored here until SkeletonComponent lands).
    std::vector<BonePose> poses;

    registry.each<AnimatorComponent, Transform3DComponent>(
        [&](u32 /*entity_id*/, AnimatorComponent& anim, Transform3DComponent& tc) {
            if (!anim.playing) return;
            if (anim.clip_id == 0u) return;
            if (!resolver_) return;
            const AnimationClip* clip = resolver_(anim.clip_id);
            if (!clip) return;

            const f32 duration = clip->duration();
            anim.time += dt * anim.speed;
            if (duration > 0.0f) {
                if (anim.looping) {
                    // Wrap modulo duration; std::fmod of a negative value
                    // would be negative, so add duration once for negative
                    // speeds (rewind) before re-normalising.
                    f32 t = std::fmod(anim.time, duration);
                    if (t < 0.0f) t += duration;
                    anim.time = t;
                } else if (anim.time >= duration) {
                    anim.time = duration;
                    anim.playing = false;
                } else if (anim.time < 0.0f) {
                    anim.time = 0.0f;
                    anim.playing = false;
                }
            }

            // Sample the clip — bone 0 is the entity's transform.  Resize
            // pose buffer to the largest bone index in the clip so every
            // channel writes a valid slot.
            i32 max_bone = 0;
            for (const auto& ch : clip->channels()) {
                if (ch.bone_index > max_bone) max_bone = ch.bone_index;
            }
            poses.assign(static_cast<size_t>(max_bone) + 1u, BonePose{});
            clip->sample(anim.time, poses);

            const BonePose& root = poses.empty() ? BonePose{} : poses[0];
            tc.position = root.position;
            tc.rotation = root.rotation;
            tc.scale    = root.scale;
            ++ticked;
        });

    return ticked;
}

u32 AnimatorSystem::start_autoplay(Registry& registry) {
    u32 started = 0;
    registry.each<AnimatorComponent>(
        [&](u32 /*entity_id*/, AnimatorComponent& anim) {
            if (anim.play_on_start && !anim.playing) {
                anim.playing = true;
                anim.time    = 0.0f;
                ++started;
            }
        });
    return started;
}

}  // namespace nexus::anim
