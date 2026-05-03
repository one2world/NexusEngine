#include "nexus/animation/particle_emitter_system.h"

#include "nexus/scene/components.h"
#include "nexus/scene/registry.h"

#include <algorithm>

namespace nexus::anim {

// ── LCG helpers ────────────────────────────────────────────────────────────
//
// Numerical Recipes' MMIX constants — fast, well-known, sufficient for
// particle jitter where cryptographic strength isn't needed.  Pure
// statics so tests can use them too.

f32 ParticleEmitterSystem::lcg_unit(u64& state) {
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    // Top 24 bits → uniform mantissa-friendly float in [0, 1).
    const u32 top = static_cast<u32>(state >> 40);
    return static_cast<f32>(top) /
           static_cast<f32>(0x01000000u);  // 2^24
}

f32 ParticleEmitterSystem::lcg_range(u64& state, f32 lo, f32 hi) {
    if (hi < lo) std::swap(lo, hi);
    return lo + lcg_unit(state) * (hi - lo);
}

// ── tick / start_autoplay / particles_for / clear ──────────────────────────

u32 ParticleEmitterSystem::tick(Registry& registry, f32 dt) {
    u32 total_alive = 0;
    if (dt < 0.0f) dt = 0.0f;

    registry.each<ParticleEmitterComponent, Transform3DComponent>(
        [&](u32 entity_id, ParticleEmitterComponent& em,
            Transform3DComponent& tc) {
            // Resolve (or create) per-entity particle storage and rng.
            auto& parts = particles_[entity_id];
            auto rng_it = rng_.find(entity_id);
            if (rng_it == rng_.end()) {
                // Seed from entity id so reruns of the same scene are
                // bit-exact in tests.  Mix in a constant so id 0 doesn't
                // give all-zeros.
                rng_it = rng_.emplace(entity_id,
                    static_cast<u64>(entity_id) * 0x9E3779B97F4A7C15ULL +
                    0xDEADBEEF12345678ULL).first;
            }
            u64& rng = rng_it->second;

            // 1. Age + cull.
            for (auto& p : parts) {
                p.age += dt;
            }
            parts.erase(
                std::remove_if(parts.begin(), parts.end(),
                    [](const Particle& p) {
                        return p.age >= p.lifetime;
                    }),
                parts.end());

            // 2. Accumulate fractional emissions.  Negative emit_rate is
            // clamped — emitters with rate <= 0 simply don't spawn.
            const f32 effective_rate =
                em.emitting ? std::max(em.emit_rate, 0.0f) : 0.0f;
            em.emit_accumulator += dt * effective_rate;

            // 3. Spawn integer count, capped by max_particles.
            while (em.emit_accumulator >= 1.0f &&
                   parts.size() < em.max_particles) {
                em.emit_accumulator -= 1.0f;

                Particle p;
                p.position = tc.position;

                // Spawn velocity along +Y by default with a small jitter
                // in X/Z so a freshly-added emitter actually shows motion
                // without requiring extra config.  The 0.5 jitter scale
                // keeps spawned cones modest.
                const f32 speed = lcg_range(rng, em.speed_min, em.speed_max);
                const f32 jitter_x = (lcg_unit(rng) - 0.5f);
                const f32 jitter_z = (lcg_unit(rng) - 0.5f);
                p.velocity = Vec3(jitter_x * speed * 0.5f,
                                   speed,
                                   jitter_z * speed * 0.5f);

                p.lifetime = lcg_range(rng, em.lifetime_min, em.lifetime_max);
                p.age      = 0.0f;
                p.color_start = em.color_start;
                p.color_end   = em.color_end;
                p.color       = em.color_start;
                p.size_start  = em.size_start;
                p.size_end    = em.size_end;
                p.size        = em.size_start;

                parts.push_back(p);
            }
            // Drain any fractional accumulator that exceeded the cap so
            // we don't store an unbounded debt that pops as a burst when
            // the user lifts the cap later.
            if (parts.size() >= em.max_particles && em.emit_accumulator > 1.0f) {
                em.emit_accumulator = 0.0f;
            }

            // 4. Apply gravity + advance.  Per-particle colour / size are
            // lerped from start → end across [0, lifetime].
            for (auto& p : parts) {
                p.velocity.y += em.gravity * dt;
                p.position   += p.velocity * dt;
                const f32 t = p.lifetime > 0.0f
                    ? std::clamp(p.age / p.lifetime, 0.0f, 1.0f)
                    : 0.0f;
                p.color = p.color_start + (p.color_end - p.color_start) * t;
                p.size  = p.size_start  + (p.size_end  - p.size_start ) * t;
            }

            // 5. Sync read-back fields for the Inspector / Stats overlay.
            em.alive_count = static_cast<u32>(parts.size());
            total_alive   += em.alive_count;
        });

    return total_alive;
}

u32 ParticleEmitterSystem::start_autoplay(Registry& registry) {
    u32 started = 0;
    registry.each<ParticleEmitterComponent>(
        [&](u32 /*entity_id*/, ParticleEmitterComponent& em) {
            if (em.play_on_start && !em.emitting) {
                em.emitting        = true;
                em.emit_accumulator = 0.0f;
                ++started;
            }
        });
    return started;
}

const std::vector<ParticleEmitterSystem::Particle>*
ParticleEmitterSystem::particles_for(u32 entity_id) const {
    auto it = particles_.find(entity_id);
    return it == particles_.end() ? nullptr : &it->second;
}

u32 ParticleEmitterSystem::particle_count_for(u32 entity_id) const {
    auto it = particles_.find(entity_id);
    return it == particles_.end()
        ? 0u
        : static_cast<u32>(it->second.size());
}

void ParticleEmitterSystem::clear() {
    particles_.clear();
    rng_.clear();
}

}  // namespace nexus::anim
