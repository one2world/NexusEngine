#include <gtest/gtest.h>
#include <nexus/animation/particle_system.h>

namespace nexus::anim::tests {

TEST(ParticleSystem, EmitsParticles) {
    EmitterConfig cfg;
    cfg.emit_rate = 100.0f;
    cfg.lifetime_min = 1.0f;
    cfg.lifetime_max = 1.0f;
    cfg.max_particles = 500;

    ParticleSystem ps(cfg);
    ps.update(0.1f); // should emit ~10 particles

    EXPECT_GT(ps.alive_count(), 0u);
    EXPECT_LE(ps.alive_count(), 15u); // roughly 10 +/- rounding
}

TEST(ParticleSystem, ParticlesDie) {
    EmitterConfig cfg;
    cfg.emit_rate = 0.0f; // no continuous emission
    cfg.lifetime_min = 0.1f;
    cfg.lifetime_max = 0.1f;
    cfg.max_particles = 100;

    ParticleSystem ps(cfg);
    ps.burst(10);
    EXPECT_EQ(ps.alive_count(), 10u);

    ps.update(0.2f); // all should be dead
    EXPECT_EQ(ps.alive_count(), 0u);
}

TEST(ParticleSystem, Burst) {
    EmitterConfig cfg;
    cfg.emit_rate = 0.0f;
    cfg.max_particles = 100;

    ParticleSystem ps(cfg);
    ps.burst(25);
    EXPECT_EQ(ps.alive_count(), 25u);
}

TEST(ParticleSystem, MaxParticlesRespected) {
    EmitterConfig cfg;
    cfg.emit_rate = 10000.0f;
    cfg.lifetime_min = 10.0f;
    cfg.lifetime_max = 10.0f;
    cfg.max_particles = 50;

    ParticleSystem ps(cfg);
    ps.update(1.0f);

    EXPECT_LE(ps.particles().size(), 50u);
}

TEST(ParticleSystem, GravityAffector) {
    EmitterConfig cfg;
    cfg.emit_rate = 0.0f;
    cfg.speed_min = 0.0f;
    cfg.speed_max = 0.0f;
    cfg.lifetime_min = 10.0f;
    cfg.lifetime_max = 10.0f;
    cfg.max_particles = 10;

    ParticleSystem ps(cfg);
    ps.add_affector(affectors::gravity({0.0f, -10.0f, 0.0f}));
    ps.burst(1);

    ps.update(1.0f);

    ASSERT_GE(ps.particles().size(), 1u);
    EXPECT_LT(ps.particles()[0].position.y, 0.0f);
}

TEST(ParticleSystem, ColorInterpolation) {
    EmitterConfig cfg;
    cfg.emit_rate = 0.0f;
    cfg.lifetime_min = 1.0f;
    cfg.lifetime_max = 1.0f;
    cfg.color_start = {1, 0, 0, 1};
    cfg.color_end = {0, 1, 0, 1};
    cfg.speed_min = 0.0f;
    cfg.speed_max = 0.0f;
    cfg.max_particles = 10;

    ParticleSystem ps(cfg);
    ps.burst(1);
    ps.update(0.5f);

    ASSERT_GE(ps.particles().size(), 1u);
    const auto& p = ps.particles()[0];
    EXPECT_NEAR(p.color.r, 0.5f, 0.05f);
    EXPECT_NEAR(p.color.g, 0.5f, 0.05f);
}

TEST(ParticleSystem, SizeInterpolation) {
    EmitterConfig cfg;
    cfg.emit_rate = 0.0f;
    cfg.lifetime_min = 1.0f;
    cfg.lifetime_max = 1.0f;
    cfg.size_min = 2.0f;
    cfg.size_max = 2.0f;
    cfg.end_size_min = 0.0f;
    cfg.end_size_max = 0.0f;
    cfg.speed_min = 0.0f;
    cfg.speed_max = 0.0f;
    cfg.max_particles = 10;

    ParticleSystem ps(cfg);
    ps.burst(1);
    ps.update(0.5f);

    ASSERT_GE(ps.particles().size(), 1u);
    EXPECT_NEAR(ps.particles()[0].size, 1.0f, 0.1f);
}

TEST(ParticleSystem, EmitterShapeBox) {
    EmitterConfig cfg;
    cfg.shape = EmitterShape::Box;
    cfg.box_extents = {1, 1, 1};
    cfg.emit_rate = 0.0f;
    cfg.lifetime_min = 10.0f;
    cfg.lifetime_max = 10.0f;
    cfg.speed_min = 0.0f;
    cfg.speed_max = 0.0f;
    cfg.max_particles = 100;

    ParticleSystem ps(cfg);
    ps.burst(50);

    for (const auto& p : ps.particles()) {
        EXPECT_LE(std::abs(p.position.x), 1.0f + 0.01f);
        EXPECT_LE(std::abs(p.position.y), 1.0f + 0.01f);
        EXPECT_LE(std::abs(p.position.z), 1.0f + 0.01f);
    }
}

TEST(ParticleSystem, StopEmitting) {
    EmitterConfig cfg;
    cfg.emit_rate = 1000.0f;
    cfg.lifetime_min = 0.01f;
    cfg.lifetime_max = 0.01f;
    cfg.max_particles = 500;

    ParticleSystem ps(cfg);
    ps.set_emitting(false);
    ps.update(1.0f);

    EXPECT_EQ(ps.alive_count(), 0u);
}

TEST(ParticleSystem, Clear) {
    EmitterConfig cfg;
    cfg.emit_rate = 0.0f;
    cfg.max_particles = 100;

    ParticleSystem ps(cfg);
    ps.burst(50);
    EXPECT_EQ(ps.alive_count(), 50u);

    ps.clear();
    EXPECT_EQ(ps.alive_count(), 0u);
}

} // namespace nexus::anim::tests
