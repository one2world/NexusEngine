#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include "nexus/rhi/rhi.h"
#include "nexus/renderer/camera.h"
#include <vector>
#include <array>

namespace nexus {

// ─────────────────────────────────────────────────────────────────────────────
// CascadedShadowMap - directional light shadows with multiple cascades
// ─────────────────────────────────────────────────────────────────────────────

class CascadedShadowMap {
public:
    static constexpr u32 MAX_CASCADES = 4;

    struct Config {
        u32   resolution{1024};
        u32   num_cascades{3};
        float cascade_split_lambda{0.75f};  // blend between logarithmic and uniform
        float shadow_distance{100.0f};
        float bias{0.005f};
        float normal_bias{0.02f};
    };

    struct CascadeData {
        Mat4  light_view_projection;
        float split_depth;
    };

    void init(rhi::RHI* rhi, const Config& config);
    void shutdown();

    /// Compute cascade splits and light matrices for the given camera & light.
    void update(const Camera3D& camera, Vec3 light_direction);

    /// Begin rendering the depth pass for a specific cascade.
    void begin_pass(u32 cascade);

    /// Submit geometry to be rendered into the current shadow cascade.
    void submit_geometry(rhi::BufferHandle vbo, rhi::BufferHandle ibo,
                         u32 index_count, const Mat4& model);

    /// End the current cascade pass.
    void end_pass();

    /// Get cascade data for shader binding.
    const std::array<CascadeData, MAX_CASCADES>& cascades() const { return cascades_; }
    u32 num_cascades() const { return config_.num_cascades; }

    /// Get the framebuffer for rendering cascade i.
    rhi::FramebufferHandle framebuffer(u32 cascade) const;

    /// Get the depth texture for cascade i.
    rhi::TextureHandle depth_texture(u32 cascade) const;

    /// Config access.
    Config& config() { return config_; }
    const Config& config() const { return config_; }

private:
    void compute_cascade_splits(float near, float far);
    Mat4 compute_light_matrix(Vec3 light_dir, const std::vector<Vec4>& frustum_corners) const;

    rhi::RHI* rhi_{nullptr};
    Config config_;
    std::array<CascadeData, MAX_CASCADES> cascades_;
    std::array<rhi::FramebufferHandle, MAX_CASCADES> framebuffers_;
    std::array<rhi::TextureHandle, MAX_CASCADES> depth_textures_;
    std::vector<float> splits_;

    // Depth rendering resources
    rhi::ShaderHandle   depth_shader_{rhi::INVALID_HANDLE};
    rhi::PipelineHandle depth_pipeline_{rhi::INVALID_HANDLE};
    u32 current_cascade_{0};
};

// ─────────────────────────────────────────────────────────────────────────────
// PointLightShadow - omnidirectional shadow for point lights
// ─────────────────────────────────────────────────────────────────────────────

class PointLightShadow {
public:
    struct Config {
        u32   resolution{512};
        float near_plane{0.1f};
        float far_plane{25.0f};
        float bias{0.005f};
    };

    void init(rhi::RHI* rhi, const Config& config);
    void shutdown();

    /// Compute the 6 face view-projection matrices for a point light.
    void update(Vec3 light_position);

    /// Begin rendering the depth pass for a specific cubemap face.
    void begin_face(u32 face, Vec3 light_position);

    /// Submit geometry for the current face.
    void submit_geometry(rhi::BufferHandle vbo, rhi::BufferHandle ibo,
                         u32 index_count, const Mat4& model);

    /// End the current face pass.
    void end_face();

    /// Get the view-projection for a cubemap face (0-5).
    const Mat4& face_matrix(u32 face) const { return face_matrices_[face]; }

    rhi::FramebufferHandle framebuffer() const { return framebuffer_; }
    rhi::TextureHandle depth_texture() const { return depth_texture_; }

    const Config& config() const { return config_; }

private:
    rhi::RHI* rhi_{nullptr};
    Config config_;
    std::array<Mat4, 6> face_matrices_;
    rhi::FramebufferHandle framebuffer_{rhi::INVALID_HANDLE};
    rhi::TextureHandle depth_texture_{rhi::INVALID_HANDLE};

    // Depth rendering resources
    rhi::ShaderHandle   depth_shader_{rhi::INVALID_HANDLE};
    rhi::PipelineHandle depth_pipeline_{rhi::INVALID_HANDLE};
};

} // namespace nexus
