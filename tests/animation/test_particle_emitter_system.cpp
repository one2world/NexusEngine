#include <gtest/gtest.h>

#include "nexus/animation/particle_emitter_system.h"
#include "nexus/scene/scene.h"
#include "nexus/scene/components.h"

namespace nexus::anim::tests {

// ── LCG helpers — pure function correctness ──────────────────────────────────

TEST(ParticleEmitterSystem, LcgUnitInRangeAndAdvancesState) {
    u64 s = 0x123456789ABCDEF0ULL;
    for (int i = 0; i < 1000; ++i) {
        const f32 v = ParticleEmitterSystem::lcg_unit(s);
        EXPECT_GE(v, 0.0f);
        EXPECT_LT(v, 1.0f);
    }
}

TEST(ParticleEmitterSystem, LcgIsDeterministicForSameSeed) {
    u64 a = 42, b = 42;
    for (int i = 0; i < 16; ++i) {
        EXPECT_FLOAT_EQ(ParticleEmitterSystem::lcg_unit(a),
                        ParticleEmitterSystem::lcg_unit(b));
    }
}

TEST(ParticleEmitterSystem, LcgRangeRespectsBoundsAndSwapsReversed) {
    u64 s = 1;
    for (int i = 0; i < 200; ++i) {
        const f32 v = ParticleEmitterSystem::lcg_range(s, 2.0f, 5.0f);
        EXPECT_GE(v, 2.0f);
        EXPECT_LE(v, 5.0f);
    }
    s = 1;
    for (int i = 0; i < 200; ++i) {
        // Reversed bounds — system swaps internally so the sample stays
        // within the well-formed range.
        const f32 v = ParticleEmitterSystem::lcg_range(s, 5.0f, 2.0f);
        EXPECT_GE(v, 2.0f);
        EXPECT_LE(v, 5.0f);
    }
}

// ── tick() emission accounting ───────────────────────────────────────────────

TEST(ParticleEmitterSystem, NotEmittingDoesNotSpawn) {
    Scene scene;
    Entity e = scene.create_entity_3d("Emit");
    auto& reg = scene.registry();
    ParticleEmitterComponent em;
    em.emit_rate    = 100.0f;
    em.emitting     = false;        // gate
    em.play_on_start = false;
    reg.add_component<ParticleEmitterComponent>(e, em);

    ParticleEmitterSystem sys;
    EXPECT_EQ(sys.tick(reg, 0.5f), 0u);
    EXPECT_EQ(sys.particle_count_for(static_cast<u32>(e)), 0u);
}

TEST(ParticleEmitterSystem, EmitRateAccumulatesParticles) {
    Scene scene;
    Entity e = scene.create_entity_3d("Emit");
    auto& reg = scene.registry();
    ParticleEmitterComponent em;
    em.emit_rate    = 100.0f;       // 100/s
    em.emitting     = true;
    em.lifetime_min = 100.0f;        // never die during the test
    em.lifetime_max = 100.0f;
    em.gravity      = 0.0f;
    reg.add_component<ParticleEmitterComponent>(e, em);

    ParticleEmitterSystem sys;
    // 0.05s * 100/s = 5 particles.
    sys.tick(reg, 0.05f);
    EXPECT_EQ(sys.particle_count_for(static_cast<u32>(e)), 5u);
    // Read-back value also synced.
    EXPECT_EQ(reg.get_component<ParticleEmitterComponent>(e).alive_count, 5u);
}

TEST(ParticleEmitterSystem, MaxParticlesCapsPopulation) {
    Scene scene;
    Entity e = scene.create_entity_3d("Cap");
    auto& reg = scene.registry();
    ParticleEmitterComponent em;
    em.emit_rate     = 1000.0f;
    em.emitting      = true;
    em.lifetime_min  = 100.0f;
    em.lifetime_max  = 100.0f;
    em.gravity       = 0.0f;
    em.max_particles = 8;
    reg.add_component<ParticleEmitterComponent>(e, em);

    ParticleEmitterSystem sys;
    sys.tick(reg, 1.0f);  // would spawn 1000, cap at 8
    EXPECT_EQ(sys.particle_count_for(static_cast<u32>(e)), 8u);
}

TEST(ParticleEmitterSystem, ParticlesAgeAndCullAfterLifetime) {
    Scene scene;
    Entity e = scene.create_entity_3d("Lif");
    auto& reg = scene.registry();
    ParticleEmitterComponent em;
    em.emit_rate    = 10.0f;
    em.emitting     = true;
    em.lifetime_min = 0.5f;
    em.lifetime_max = 0.5f;
    em.gravity      = 0.0f;
    reg.add_component<ParticleEmitterComponent>(e, em);

    ParticleEmitterSystem sys;
    sys.tick(reg, 0.1f);  // spawn 1 particle
    EXPECT_EQ(sys.particle_count_for(static_cast<u32>(e)), 1u);
    // Stop emission so existing particle ages out cleanly.
    reg.get_component<ParticleEmitterComponent>(e).emitting = false;
    sys.tick(reg, 0.6f);  // age past lifetime → cull
    EXPECT_EQ(sys.particle_count_for(static_cast<u32>(e)), 0u);
}

TEST(ParticleEmitterSystem, GravityAffectsVelocityYOverTime) {
    Scene scene;
    Entity e = scene.create_entity_3d("Grav");
    auto& reg = scene.registry();
    ParticleEmitterComponent em;
    em.emit_rate     = 10.0f;
    em.emitting      = true;
    em.speed_min     = 0.0f;
    em.speed_max     = 0.0f;        // no initial speed
    em.lifetime_min  = 100.0f;
    em.lifetime_max  = 100.0f;
    em.gravity       = -10.0f;
    reg.add_component<ParticleEmitterComponent>(e, em);

    ParticleEmitterSystem sys;
    sys.tick(reg, 0.1f);  // spawn
    sys.tick(reg, 1.0f);  // gravity acts for ~1s
    const auto* parts = sys.particles_for(static_cast<u32>(e));
    ASSERT_NE(parts, nullptr);
    ASSERT_FALSE(parts->empty());
    EXPECT_LT(parts->front().velocity.y, -5.0f);
}

TEST(ParticleEmitterSystem, NegativeDtIsClampedToZero) {
    Scene scene;
    Entity e = scene.create_entity_3d("Dt");
    auto& reg = scene.registry();
    ParticleEmitterComponent em;
    em.emit_rate = 10.0f; em.emitting = true;
    reg.add_component<ParticleEmitterComponent>(e, em);

    ParticleEmitterSystem sys;
    EXPECT_EQ(sys.tick(reg, -1.0f), 0u);
    EXPECT_EQ(sys.particle_count_for(static_cast<u32>(e)), 0u);
}

TEST(ParticleEmitterSystem, EmitsRespectTransform3DPosition) {
    Scene scene;
    Entity e = scene.create_entity_3d("Pos");
    auto& reg = scene.registry();
    reg.get_component<Transform3DComponent>(e).position = Vec3(7.0f, 2.0f, -3.0f);
    ParticleEmitterComponent em;
    em.emit_rate    = 10.0f;
    em.emitting     = true;
    em.speed_min    = 0.0f; em.speed_max = 0.0f;
    em.lifetime_min = 100.0f; em.lifetime_max = 100.0f;
    em.gravity      = 0.0f;
    reg.add_component<ParticleEmitterComponent>(e, em);

    ParticleEmitterSystem sys;
    sys.tick(reg, 0.1f);
    const auto* parts = sys.particles_for(static_cast<u32>(e));
    ASSERT_NE(parts, nullptr);
    ASSERT_FALSE(parts->empty());
    EXPECT_NEAR(parts->front().position.x,  7.0f, 1e-5f);
    EXPECT_NEAR(parts->front().position.y,  2.0f, 1e-5f);
    EXPECT_NEAR(parts->front().position.z, -3.0f, 1e-5f);
}

// ── start_autoplay() ─────────────────────────────────────────────────────────

TEST(ParticleEmitterSystem, StartAutoplayFlipsPlayOnStart) {
    Scene scene;
    auto& reg = scene.registry();
    Entity a = scene.create_entity_3d("A");
    {
        ParticleEmitterComponent em;
        em.play_on_start = true;
        em.emitting       = false;
        em.emit_accumulator = 99.0f;
        reg.add_component<ParticleEmitterComponent>(a, em);
    }
    Entity b = scene.create_entity_3d("B");
    {
        ParticleEmitterComponent em;
        em.play_on_start = false;
        em.emitting       = false;
        reg.add_component<ParticleEmitterComponent>(b, em);
    }

    EXPECT_EQ(ParticleEmitterSystem::start_autoplay(reg), 1u);
    EXPECT_TRUE(reg.get_component<ParticleEmitterComponent>(a).emitting);
    EXPECT_FLOAT_EQ(reg.get_component<ParticleEmitterComponent>(a).emit_accumulator, 0.0f);
    EXPECT_FALSE(reg.get_component<ParticleEmitterComponent>(b).emitting);
}

// ── clear() resets internal storage ──────────────────────────────────────────

TEST(ParticleEmitterSystem, ClearWipesPerEntityStorage) {
    Scene scene;
    Entity e = scene.create_entity_3d("X");
    auto& reg = scene.registry();
    ParticleEmitterComponent em;
    em.emit_rate = 100.0f; em.emitting = true;
    em.lifetime_min = 10.0f; em.lifetime_max = 10.0f;
    reg.add_component<ParticleEmitterComponent>(e, em);

    ParticleEmitterSystem sys;
    sys.tick(reg, 0.1f);
    EXPECT_GT(sys.particle_count_for(static_cast<u32>(e)), 0u);
    sys.clear();
    EXPECT_EQ(sys.particle_count_for(static_cast<u32>(e)), 0u);
}

// ── Particles_for unknown entity ─────────────────────────────────────────────

TEST(ParticleEmitterSystem, ParticlesForUnknownEntityReturnsNull) {
    ParticleEmitterSystem sys;
    EXPECT_EQ(sys.particles_for(0xDEADBEEFu), nullptr);
    EXPECT_EQ(sys.particle_count_for(0xDEADBEEFu), 0u);
}

}  // namespace nexus::anim::tests
