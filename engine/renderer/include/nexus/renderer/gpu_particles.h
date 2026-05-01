#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include "nexus/rhi/rhi.h"
#include <vector>

namespace nexus {

// ============================================================================
// GPU Particle System — real compute-shader emission + simulation.
//
// The particle pool lives entirely in an SSBO.  A 64-thread-per-group
// compute shader advances every slot per frame (integrate + kill + emit).
// A separate atomic counter SSBO tracks alive count across dispatches.
// CPU only uploads the per-frame uniforms and issues dispatch / draw.
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

// Matches the std430 layout used in the compute shader.  16-byte aligned.
struct alignas(16) GPUParticle {
    Vec3  position;
    float lifetime;
    Vec3  velocity;
    float max_lifetime;
    Vec4  color;
    float size;
    float _pad[3];
};

class GPUParticleSystem {
public:
    void init(rhi::RHI* rhi, const GPUParticleEmitterConfig& config);
    void shutdown();

    /// Emit new particles and simulate existing ones on the GPU.
    void update(float dt);

    /// Render all alive particles as billboards.
    void render(const Mat4& view, const Mat4& projection, Vec3 camera_right, Vec3 camera_up);

    /// Get current alive particle count (last frame's snapshot).
    u32 alive_count() const { return alive_count_; }

    /// Access config for runtime modification.
    GPUParticleEmitterConfig& config() { return config_; }
    const GPUParticleEmitterConfig& config() const { return config_; }

    /// Set texture for particle rendering.
    void set_texture(rhi::TextureHandle tex) { texture_ = tex; }

    /// True when the RHI supports GPU compute and the system uses it.
    bool uses_gpu_compute() const { return uses_gpu_compute_; }

private:
    void update_gpu(float dt);
    void update_cpu_fallback(float dt);
    void read_back_alive_count();

    rhi::RHI* rhi_{nullptr};
    GPUParticleEmitterConfig config_;

    // GPU resources
    rhi::ShaderHandle render_shader_{rhi::INVALID_HANDLE};
    rhi::ShaderHandle compute_shader_{rhi::INVALID_HANDLE};
    rhi::PipelineHandle pipeline_{rhi::INVALID_HANDLE};
    rhi::BufferHandle particle_ssbo_{rhi::INVALID_HANDLE};   // array<GPUParticle>
    rhi::BufferHandle counter_ssbo_{rhi::INVALID_HANDLE};    // single u32 alive-count
    rhi::BufferHandle quad_vbo_{rhi::INVALID_HANDLE};        // 6-vertex unit quad
    rhi::TextureHandle texture_{rhi::INVALID_HANDLE};

    // CPU fallback state (only used when RHI lacks compute support)
    bool uses_gpu_compute_{false};
    std::vector<GPUParticle> cpu_particles_;
    rhi::BufferHandle cpu_vbo_{rhi::INVALID_HANDLE};

    u32 alive_count_{0};
    float emission_accumulator_{0.0f};
    u32 frame_seed_{0};

    // CPU fallback RNG
    u32 rng_state_{12345};
    float rand_float();
    float rand_range(float min, float max);
    Vec3  rand_cone(Vec3 direction, float angle);
};

} // namespace nexus
