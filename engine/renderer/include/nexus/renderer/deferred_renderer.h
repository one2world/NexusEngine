#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include "nexus/rhi/rhi.h"
#include "nexus/renderer/pbr_renderer.h"
#include <vector>

namespace nexus {

// ─────────────────────────────────────────────────────────────────────────────
// GBuffer layout:
//   RT0: albedo.rgb + metallic (RGBA8 or RGBA16F)
//   RT1: normal.xyz encoded (RGBA16F)
//   RT2: roughness, AO, emissive_strength, flags (RGBA8)
//   Depth: Depth32F
// ─────────────────────────────────────────────────────────────────────────────

struct GBuffer {
    rhi::FramebufferHandle framebuffer{rhi::INVALID_HANDLE};
    rhi::TextureHandle     albedo_metallic{rhi::INVALID_HANDLE};   // RT0
    rhi::TextureHandle     normal{rhi::INVALID_HANDLE};            // RT1
    rhi::TextureHandle     roughness_ao{rhi::INVALID_HANDLE};      // RT2
    rhi::TextureHandle     depth{rhi::INVALID_HANDLE};
    u32 width{0}, height{0};
};

// ─────────────────────────────────────────────────────────────────────────────
// PointLight - used by deferred lighting pass
// ─────────────────────────────────────────────────────────────────────────────

struct PointLight {
    Vec3  position{0.0f};
    float radius{10.0f};
    Vec3  color{1.0f};
    float intensity{1.0f};
};

// ─────────────────────────────────────────────────────────────────────────────
// DeferredRenderer - G-buffer geometry pass + deferred lighting pass
// ─────────────────────────────────────────────────────────────────────────────

class DeferredRenderer {
public:
    void init(rhi::RHI* rhi, u32 width, u32 height);
    void shutdown();
    void resize(u32 width, u32 height);

    /// Begin the geometry pass (binds G-buffer).
    void begin_geometry_pass(const class Camera3D& camera);

    /// Submit a mesh to the G-buffer.
    void submit_geometry(rhi::BufferHandle vbo, rhi::BufferHandle ibo,
                         u32 index_count, const Mat4& transform,
                         const PBRMaterial& material);

    /// End geometry pass.
    void end_geometry_pass();

    /// Run the deferred lighting pass. Output goes to dest framebuffer.
    void lighting_pass(const class Camera3D& camera,
                       Vec3 sun_direction, Vec3 sun_color, float sun_intensity,
                       const std::vector<PointLight>& point_lights,
                       rhi::FramebufferHandle dest);

    /// Access the G-buffer textures (for debug visualization, SSAO, etc.).
    const GBuffer& gbuffer() const { return gbuffer_; }

    /// Maximum point lights per pass.
    static constexpr u32 MAX_POINT_LIGHTS = 32;

private:
    void create_gbuffer(u32 width, u32 height);
    void destroy_gbuffer();

    rhi::RHI* rhi_{nullptr};
    GBuffer gbuffer_;

    // Geometry pass
    rhi::ShaderHandle   geom_shader_{rhi::INVALID_HANDLE};
    rhi::PipelineHandle geom_pipeline_{rhi::INVALID_HANDLE};

    // Lighting pass
    rhi::ShaderHandle   light_shader_{rhi::INVALID_HANDLE};
    rhi::PipelineHandle light_pipeline_{rhi::INVALID_HANDLE};
    rhi::BufferHandle   quad_vbo_{rhi::INVALID_HANDLE};

    Mat4 view_projection_{1.0f};
    Vec3 camera_position_{0.0f};
};

} // namespace nexus
