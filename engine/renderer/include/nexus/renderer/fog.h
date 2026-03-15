#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include "nexus/rhi/rhi.h"

namespace nexus {

// ─────────────────────────────────────────────────────────────────────────────
// FogSettings - configurable fog parameters
// ─────────────────────────────────────────────────────────────────────────────

struct FogSettings {
    enum class Mode : u8 {
        None,
        Linear,
        Exponential,
        ExponentialSquared,
        HeightBased
    };

    Mode  mode{Mode::None};
    Vec3  color{0.7f, 0.8f, 0.9f};
    float density{0.02f};            // for exponential modes

    // Linear fog
    float near_distance{10.0f};
    float far_distance{100.0f};

    // Height-based fog
    float height_falloff{0.1f};      // how quickly fog fades with altitude
    float base_height{0.0f};         // fog base altitude
    float max_height{50.0f};         // fog ceiling

    /// Compute fog factor (0 = fully fogged, 1 = no fog) for a given distance/height.
    float compute_factor(float distance, float world_y = 0.0f) const;
};

// ─────────────────────────────────────────────────────────────────────────────
// FogPass - applies fog as a post-process using the depth buffer
// ─────────────────────────────────────────────────────────────────────────────

class FogPass {
public:
    void init(rhi::RHI* rhi);
    void shutdown();

    /// Apply fog to the scene. Reads from scene_color and depth_texture.
    void apply(rhi::RHI* rhi,
               rhi::TextureHandle scene_color,
               rhi::TextureHandle depth_texture,
               rhi::FramebufferHandle dest,
               const FogSettings& settings,
               const Mat4& inverse_view_projection,
               Vec3 camera_position,
               float near_clip,
               float far_clip);

private:
    rhi::ShaderHandle   shader_{rhi::INVALID_HANDLE};
    rhi::PipelineHandle pipeline_{rhi::INVALID_HANDLE};
    rhi::BufferHandle   quad_vbo_{rhi::INVALID_HANDLE};
};

} // namespace nexus
