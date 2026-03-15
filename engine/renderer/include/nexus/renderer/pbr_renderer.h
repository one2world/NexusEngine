#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include "nexus/rhi/rhi.h"
#include <string>
#include <vector>

namespace nexus {

// ─────────────────────────────────────────────────────────────────────────────
// PBRMaterial - metallic-roughness PBR workflow
// ─────────────────────────────────────────────────────────────────────────────

struct PBRMaterial {
    // Base color (albedo)
    Vec4  albedo{1.0f, 1.0f, 1.0f, 1.0f};
    rhi::TextureHandle albedo_map{rhi::INVALID_HANDLE};

    // Metallic-roughness
    float metallic{0.0f};
    float roughness{0.5f};
    rhi::TextureHandle metallic_roughness_map{rhi::INVALID_HANDLE};

    // Normal map
    float normal_strength{1.0f};
    rhi::TextureHandle normal_map{rhi::INVALID_HANDLE};

    // Ambient occlusion
    float ao_strength{1.0f};
    rhi::TextureHandle ao_map{rhi::INVALID_HANDLE};

    // Emission
    Vec3  emissive{0.0f};
    float emissive_strength{1.0f};
    rhi::TextureHandle emissive_map{rhi::INVALID_HANDLE};

    // Alpha
    float alpha_cutoff{0.5f};
    bool  transparent{false};
    bool  double_sided{false};
};

// ─────────────────────────────────────────────────────────────────────────────
// IBLData - image-based lighting environment
// ─────────────────────────────────────────────────────────────────────────────

struct IBLData {
    rhi::TextureHandle irradiance_map{rhi::INVALID_HANDLE};     // diffuse IBL
    rhi::TextureHandle prefiltered_map{rhi::INVALID_HANDLE};    // specular IBL (mipmapped)
    rhi::TextureHandle brdf_lut{rhi::INVALID_HANDLE};           // BRDF integration LUT
    float              intensity{1.0f};
};

// ─────────────────────────────────────────────────────────────────────────────
// PBR shader source (GLSL 330) — vertex + fragment
// ─────────────────────────────────────────────────────────────────────────────

namespace pbr_shaders {

extern const char* VERTEX_SHADER;
extern const char* FRAGMENT_SHADER;

} // namespace pbr_shaders

// ─────────────────────────────────────────────────────────────────────────────
// PBRRenderer - renders meshes with PBR materials
// ─────────────────────────────────────────────────────────────────────────────

class PBRRenderer {
public:
    void init(rhi::RHI* rhi);
    void shutdown();

    void begin(const class Camera3D& camera);
    void end();

    /// Set environment lighting.
    void set_environment(const IBLData& ibl);

    /// Set directional light.
    void set_sun(Vec3 direction, Vec3 color, float intensity);

    /// Submit a mesh draw with PBR material.
    void draw(rhi::BufferHandle vbo, rhi::BufferHandle ibo,
              u32 index_count, const Mat4& transform,
              const PBRMaterial& material);

    /// Exposure for tone mapping.
    void set_exposure(float exposure) { exposure_ = exposure; }
    float exposure() const { return exposure_; }

private:
    rhi::RHI*           rhi_{nullptr};
    rhi::ShaderHandle   shader_{rhi::INVALID_HANDLE};
    rhi::PipelineHandle pipeline_{rhi::INVALID_HANDLE};

    Mat4 view_projection_{1.0f};
    Vec3 camera_position_{0.0f};

    // Lighting
    Vec3  sun_direction_{0.0f, -1.0f, 0.0f};
    Vec3  sun_color_{1.0f};
    float sun_intensity_{1.0f};
    IBLData ibl_;
    float exposure_{1.0f};
};

} // namespace nexus
