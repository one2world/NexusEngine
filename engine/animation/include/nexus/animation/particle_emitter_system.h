#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"

#include <unordered_map>
#include <vector>

namespace nexus {
class Registry;
}

namespace nexus::anim {

// ─────────────────────────────────────────────────────────────────────────────
// ParticleEmitterSystem — drives ParticleEmitterComponent every frame
// ─────────────────────────────────────────────────────────────────────────────
//
// Walks ECS for entities that have both a ParticleEmitterComponent and a
// Transform3DComponent.  Per entity it owns a small std::vector of live
// Particle records so the runtime state doesn't bloat the component (and
// so save/load via scene_serializer stays trivial).
//
// Per tick:
//   1. Age existing particles by `dt`; cull those past lifetime.
//   2. Accumulate fractional new particles (emit_accumulator += dt * emit_rate).
//   3. Spawn integer particle count, soft-capped at max_particles.
//   4. Apply constant gravity to alive velocities, advance position.
//   5. Sync `alive_count` back into the component for the Inspector.
//
// Spawning uses a deterministic LCG seeded per entity so tests can verify
// behaviour without random flakiness.  Production code can opt into a
// per-frame seed by calling `set_seed_strategy`.
class ParticleEmitterSystem {
public:
    /// Lightweight per-particle record, kept in a std::vector inside the
    /// system's per-entity storage.  Deliberately a POD so future render
    /// integration can submit it directly.
    struct Particle {
        Vec3 position{0.0f};
        Vec3 velocity{0.0f};
        Vec4 color{1.0f};
        f32  size{0.0f};
        f32  age{0.0f};
        f32  lifetime{1.0f};
        // Authored at birth so the system can lerp colour/size from
        // start→end per particle without re-reading the component.
        Vec4 color_start{1.0f};
        Vec4 color_end{0.0f};
        f32  size_start{0.0f};
        f32  size_end{0.0f};
    };

    ParticleEmitterSystem() = default;

    /// Walk ECS, advance every emitter, return total particles alive
    /// across all systems for telemetry.
    u32 tick(Registry& registry, f32 dt);

    /// Flip `emitting=true` on every emitter that has `play_on_start`,
    /// reset accumulators, clear stale particles.  Called by SceneBridge
    /// when the user enters Play mode (mirrors AnimatorSystem::start_autoplay).
    static u32 start_autoplay(Registry& registry);

    /// Direct access to a per-entity particle store — used by tests and
    /// renderer hooks.  Returns nullptr if the entity has no particles.
    const std::vector<Particle>* particles_for(u32 entity_id) const;
    u32 particle_count_for(u32 entity_id) const;

    /// Reset every per-entity store (test convenience).
    void clear();

    // ── Pure helpers (test-friendly) ─────────────────────────────────────

    /// LCG step for deterministic uniform[0,1).  Updates `state` in place.
    static f32 lcg_unit(u64& state);

    /// Sample uniform [lo, hi] — clamps to [lo, hi] when reversed.
    static f32 lcg_range(u64& state, f32 lo, f32 hi);

private:
    // Per-entity packed particle list keyed by entity id.
    std::unordered_map<u32, std::vector<Particle>> particles_;
    // Per-entity LCG state — seeded once on first sighting from the
    // entity id so reruns are bit-exact.
    std::unordered_map<u32, u64> rng_;
};

}  // namespace nexus::anim
