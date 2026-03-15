#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include <vector>
#include <functional>
#include <random>

namespace nexus::anim {

// ─────────────────────────────────────────────────────────────────────────────
// Particle - a single particle instance
// ─────────────────────────────────────────────────────────────────────────────

struct Particle {
    Vec3  position{0.0f};
    Vec3  velocity{0.0f};
    Vec4  color{1.0f};
    Vec4  start_color{1.0f};
    Vec4  end_color{1.0f};
    float size{1.0f};
    float start_size{1.0f};
    float end_size{0.0f};
    float lifetime{1.0f};
    float age{0.0f};
    float rotation{0.0f};
    float angular_velocity{0.0f};
    bool  alive{true};
};

// ─────────────────────────────────────────────────────────────────────────────
// EmitterShape
// ─────────────────────────────────────────────────────────────────────────────

enum class EmitterShape : u8 {
    Point,
    Box,
    Sphere,
    Cone,
    Circle,
};

struct EmitterConfig {
    EmitterShape shape{EmitterShape::Point};
    Vec3  position{0.0f};
    Vec3  direction{0.0f, 1.0f, 0.0f};

    // Shape params
    Vec3  box_extents{1.0f};       // for Box
    float sphere_radius{1.0f};     // for Sphere/Circle
    float cone_angle{30.0f};       // degrees, for Cone

    // Emission
    float emit_rate{10.0f};        // particles per second
    u32   burst_count{0};          // one-shot burst

    // Particle initial properties
    float speed_min{1.0f};
    float speed_max{3.0f};
    float lifetime_min{1.0f};
    float lifetime_max{2.0f};
    float size_min{0.1f};
    float size_max{0.5f};
    float end_size_min{0.0f};
    float end_size_max{0.0f};
    Vec4  color_start{1.0f};
    Vec4  color_end{1.0f, 1.0f, 1.0f, 0.0f};
    float rotation_min{0.0f};
    float rotation_max{0.0f};
    float angular_velocity_min{0.0f};
    float angular_velocity_max{0.0f};

    u32   max_particles{1000};
};

// ─────────────────────────────────────────────────────────────────────────────
// Affector - modifies particles each frame
// ─────────────────────────────────────────────────────────────────────────────

using Affector = std::function<void(Particle& p, float dt)>;

namespace affectors {

/// Apply constant gravity.
inline Affector gravity(Vec3 g = {0.0f, -9.81f, 0.0f}) {
    return [g](Particle& p, float dt) { p.velocity += g * dt; };
}

/// Apply wind.
inline Affector wind(Vec3 force) {
    return [force](Particle& p, float dt) { p.velocity += force * dt; };
}

/// Linear drag.
inline Affector drag(float coefficient = 0.1f) {
    return [coefficient](Particle& p, float dt) {
        p.velocity *= 1.0f / (1.0f + coefficient * dt);
    };
}

/// Turbulence (simple random jitter).
inline Affector turbulence(float strength = 1.0f) {
    return [strength](Particle& p, float dt) {
        // Simple deterministic-ish jitter from position
        float jx = std::sin(p.position.x * 3.7f + p.age * 2.1f) * strength * dt;
        float jy = std::cos(p.position.y * 5.3f + p.age * 1.7f) * strength * dt;
        float jz = std::sin(p.position.z * 4.1f + p.age * 3.3f) * strength * dt;
        p.velocity += Vec3(jx, jy, jz);
    };
}

} // namespace affectors

// ─────────────────────────────────────────────────────────────────────────────
// ParticleSystem - emits, simulates, and manages particles
// ─────────────────────────────────────────────────────────────────────────────

class ParticleSystem {
public:
    explicit ParticleSystem(const EmitterConfig& config = {});

    void update(float dt);

    /// Emit a burst of particles immediately.
    void burst(u32 count);

    /// Add an affector.
    void add_affector(Affector affector);

    /// Clear all particles.
    void clear();

    // Control
    void set_emitting(bool emit) { emitting_ = emit; }
    bool is_emitting() const { return emitting_; }

    void set_position(Vec3 pos) { config_.position = pos; }

    // Access
    const std::vector<Particle>& particles() const { return particles_; }
    u32 alive_count() const;
    const EmitterConfig& config() const { return config_; }
    EmitterConfig& config() { return config_; }

private:
    void emit_particle();
    Vec3 random_direction();
    float random_range(float lo, float hi);

    EmitterConfig config_;
    std::vector<Particle> particles_;
    std::vector<Affector> affectors_;
    float emit_accumulator_{0.0f};
    bool emitting_{true};
    std::mt19937 rng_{42};
};

} // namespace nexus::anim
