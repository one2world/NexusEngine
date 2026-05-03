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
        [&](u32 entity_id, AnimatorComponent& anim, Transform3DComponent& tc) {
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

            // Sample the clip.  Resize pose buffer to the largest bone
            // index in the clip so every channel writes a valid slot.
            i32 max_bone = 0;
            for (const auto& ch : clip->channels()) {
                if (ch.bone_index > max_bone) max_bone = ch.bone_index;
            }
            poses.assign(static_cast<size_t>(max_bone) + 1u, BonePose{});
            clip->sample(anim.time, poses);

            // Multi-bone path (M20): when the entity has a non-empty
            // SkeletonComponent, route each clip channel's pose into the
            // matching bone entity's Transform3D.  Bone 0 still writes to
            // the entity's own transform when the skeleton's first slot
            // points at it (typical convention).  Out-of-range pose
            // indices and dead bone entities are skipped silently — the
            // common cause is a clip authored against a different rig
            // than the one currently bound, which we shouldn't crash on.
            //
            // Single-transform fallback (no skeleton): write bone 0 into
            // the entity's own Transform3D.  Matches the M10 behaviour.
            const Entity self_e = static_cast<Entity>(entity_id);
            const SkeletonComponent* skel =
                registry.has_component<SkeletonComponent>(self_e)
                ? &registry.get_component<SkeletonComponent>(self_e)
                : nullptr;

            if (skel && !skel->bone_entities.empty()) {
                const size_t pose_count = poses.size();
                // Snapshot the bone list before iterating so a registry
                // mutation triggered inside (none today, but guards
                // against future writers) doesn't invalidate our walk.
                const std::vector<u32> bones_copy = skel->bone_entities;
                for (size_t i = 0; i < bones_copy.size(); ++i) {
                    if (i >= pose_count) break;
                    const Entity bone_e =
                        static_cast<Entity>(bones_copy[i]);
                    if (bone_e == INVALID_ENTITY) continue;
                    if (!registry.alive(bone_e)) continue;
                    if (!registry.has_component<Transform3DComponent>(bone_e)) {
                        continue;
                    }
                    auto& bone_tc =
                        registry.get_component<Transform3DComponent>(bone_e);
                    bone_tc.position = poses[i].position;
                    bone_tc.rotation = poses[i].rotation;
                    bone_tc.scale    = poses[i].scale;
                }
            } else {
                const BonePose& root = poses.empty() ? BonePose{} : poses[0];
                tc.position = root.position;
                tc.rotation = root.rotation;
                tc.scale    = root.scale;
            }
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
