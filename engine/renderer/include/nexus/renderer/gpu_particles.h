#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include "nexus/rhi/rhi.h"
#include <vector>

namespace nexus {

// ============================================================================
// GPU Particle System - compute shader based emission and simulation
// ============================================================================

struct GPUParticleEmitterConfig {
    Vec3 position{0.0f};
    Vec3 direction{0.0f, 1.0f, 0.0f};
    float spread_angle{45.0f};      // cone spread in degrees
    float min_speed{1.0f};
    float max_speed{5.0f};
    float min_lifetime{1.0f};
    float max_lifetime{3.0f};
    Vec4 start_color{1.0f};
    Vec4 end_color{1.0f, 1.0f, 1.0f, 0.0f};
    float start_size{0.1f};
    float end_size{0.0f};
    float gravity{-9.81f};
    u32 max_particles{100000};
    float emission_rate{1000.0f};   // particles per second
    bool additive_blend{true};
};

struct GPUParticle {
    Vec3 position;
    float lifetime;
    Vec3 velocity;
    float max_lifetime;
    Vec4 color;
    float size;
    float pad[3];
};

class GPUParticleSystem {
public:
    void init(rhi::RHI* rhi, const GPUParticleEmitterConfig& config);
    void shutdown();

    /// Emit new particles and simulate existing ones.
    void update(float dt);

    /// Render all alive particles as billboards.
    void render(const Mat4& view, const Mat4& projection, Vec3 camera_right, Vec3 camera_up);

    /// Get current alive particle count.
    u32 alive_count() const { return alive_count_; }

    /// Access config for runtime modification.
    GPUParticleEmitterConfig& config() { return config_; }
    const GPUParticleEmitterConfig& config() const { return config_; }

    /// Set texture for particle rendering.
    void set_texture(rhi::TextureHandle tex) { texture_ = tex; }

private:
    void emit_particles(float dt);
    void simulate_particles(float dt);
    void compact_dead_particles();

    rhi::RHI* rhi_{nullptr};
    GPUParticleEmitterConfig config_;

    // Particle storage (CPU-side for OpenGL compatibility, GPU compute for Vulkan)
    std::vector<GPUParticle> particles_;
    u32 alive_count_{0};

    // Rendering resources
    rhi::ShaderHandle shader_{rhi::INVALID_HANDLE};
    rhi::PipelineHandle pipeline_{rhi::INVALID_HANDLE};
    rhi::BufferHandle vbo_{rhi::INVALID_HANDLE};
    rhi::TextureHandle texture_{rhi::INVALID_HANDLE};

    // Emission accumulator
    float emission_accumulator_{0.0f};

    // Random state
    u32 rng_state_{12345};
    float rand_float();  // [0, 1]
    float rand_range(float min, float max);
    Vec3 rand_cone(Vec3 direction, float angle);
};

} // namespace nexus
