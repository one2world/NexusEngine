#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include "nexus/rhi/rhi.h"
#include <array>

namespace nexus {

// ─────────────────────────────────────────────────────────────────────────────
// SkyboxRenderer — cubemap skybox rendered behind all scene geometry
// ─────────────────────────────────────────────────────────────────────────────

class SkyboxRenderer {
public:
    void init(rhi::RHI* rhi);
    void shutdown();

    /// Set the cubemap texture (6-face cubemap).
    void set_cubemap(rhi::TextureHandle cubemap) { cubemap_ = cubemap; }

    /// Set a solid color fallback (used when no cubemap is set).
    void set_clear_color(Vec3 color) { clear_color_ = color; }

    /// Set exposure for HDR skybox.
    void set_exposure(float exposure) { exposure_ = exposure; }

    /// Set rotation offset (radians around Y axis).
    void set_rotation(float radians) { rotation_ = radians; }

    /// Render the skybox. Call after opaque geometry, before transparent.
    /// Uses the camera's view matrix with translation removed so the skybox
    /// appears infinitely far away.
    void render(const Mat4& view, const Mat4& projection);

    [[nodiscard]] bool has_cubemap() const { return cubemap_ != rhi::INVALID_HANDLE; }

private:
    rhi::RHI* rhi_{nullptr};
    rhi::TextureHandle cubemap_{rhi::INVALID_HANDLE};
    rhi::ShaderHandle shader_{rhi::INVALID_HANDLE};
    rhi::PipelineHandle pipeline_{rhi::INVALID_HANDLE};
    rhi::BufferHandle cube_vbo_{rhi::INVALID_HANDLE};

    Vec3 clear_color_{0.1f, 0.1f, 0.15f};
    float exposure_{1.0f};
    float rotation_{0.0f};
};

// ─────────────────────────────────────────────────────────────────────────────
// ProceduralSky — Hosek-Wilkie analytic sky model
// ─────────────────────────────────────────────────────────────────────────────

class ProceduralSky {
public:
    void init(rhi::RHI* rhi);
    void shutdown();

    /// Set sun direction (normalized, pointing toward sun).
    void set_sun_direction(Vec3 dir) { sun_direction_ = glm::normalize(dir); }

    /// Set turbidity (2.0 = clear, 10.0 = hazy).
    void set_turbidity(float t) { turbidity_ = t; }

    /// Set ground albedo for bounced light approximation.
    void set_ground_albedo(Vec3 albedo) { ground_albedo_ = albedo; }

    /// Set exposure multiplier for the sky.
    void set_exposure(float e) { exposure_ = e; }

    /// Render the procedural sky. Same depth trick as cubemap skybox.
    void render(const Mat4& view, const Mat4& projection);

    [[nodiscard]] Vec3 sun_direction() const { return sun_direction_; }

private:
    rhi::RHI* rhi_{nullptr};
    rhi::ShaderHandle shader_{rhi::INVALID_HANDLE};
    rhi::PipelineHandle pipeline_{rhi::INVALID_HANDLE};
    rhi::BufferHandle cube_vbo_{rhi::INVALID_HANDLE};

    Vec3 sun_direction_{0.0f, 1.0f, 0.0f};
    float turbidity_{3.0f};
    Vec3 ground_albedo_{0.3f};
    float exposure_{1.0f};
};

// ─────────────────────────────────────────────────────────────────────────────
// ReflectionProbe — baked cubemap capture for local reflections
// ─────────────────────────────────────────────────────────────────────────────

struct ReflectionProbe {
    Vec3 position{0.0f};
    Vec3 box_min{-10.0f};           // AABB for parallax correction
    Vec3 box_max{10.0f};
    float intensity{1.0f};
    u32 resolution{256};
    rhi::TextureHandle cubemap{rhi::INVALID_HANDLE};
    bool needs_rebake{true};
};

class ReflectionProbeManager {
public:
    void init(rhi::RHI* rhi);
    void shutdown();

    /// Add a reflection probe. Returns its index.
    u32 add_probe(const ReflectionProbe& probe);

    /// Remove a probe by index.
    void remove_probe(u32 index);

    /// Get probe for reading/writing.
    ReflectionProbe& get_probe(u32 index) { return probes_[index]; }
    [[nodiscard]] const ReflectionProbe& get_probe(u32 index) const { return probes_[index]; }

    /// Find the most relevant probe for a world position.
    /// Returns nullptr if no probes exist.
    [[nodiscard]] const ReflectionProbe* find_probe(Vec3 world_pos) const;

    /// Number of probes.
    [[nodiscard]] u32 probe_count() const { return static_cast<u32>(probes_.size()); }

    /// Bake a single probe (renders scene into cubemap).
    /// The callback is invoked 6 times (once per face) with the face view-projection.
    using RenderCallback = std::function<void(const Mat4& view_proj, u32 face)>;
    void bake_probe(u32 index, RenderCallback render_scene);

private:
    rhi::RHI* rhi_{nullptr};
    std::vector<ReflectionProbe> probes_;
    rhi::FramebufferHandle capture_fb_{rhi::INVALID_HANDLE};
};

} // namespace nexus
