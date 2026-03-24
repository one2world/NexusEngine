#pragma once

#include "nexus/renderer/post_process.h"
#include <array>

namespace nexus {

// ─────────────────────────────────────────────────────────────────────────────
// SSAOEffect - Screen-Space Ambient Occlusion
//
// Requires depth and normal textures from the G-buffer (deferred renderer).
// Produces a single-channel AO texture that can be multiplied into lighting.
// ─────────────────────────────────────────────────────────────────────────────

class SSAOEffect : public PostProcessEffect {
public:
    void init(rhi::RHI* rhi, u32 width, u32 height) override;
    void shutdown() override;
    void resize(u32 width, u32 height) override;
    void apply(rhi::RHI* rhi, rhi::TextureHandle input,
               rhi::FramebufferHandle dest) override;
    const std::string& name() const override {
        static std::string n = "SSAO";
        return n;
    }

    /// Set G-buffer textures needed for SSAO calculation.
    void set_gbuffer_textures(rhi::TextureHandle depth, rhi::TextureHandle normals) {
        depth_tex_ = depth;
        normal_tex_ = normals;
    }

    /// Set the camera projection matrix (needed for depth reconstruction).
    void set_projection(const Mat4& proj) { projection_ = proj; }

    // Tuning parameters
    float radius{0.5f};           // sample radius in world units
    float bias{0.025f};           // depth bias to avoid self-occlusion
    float intensity{1.5f};        // AO intensity multiplier
    u32   kernel_size{32};         // number of hemisphere samples (max 64)

private:
    void generate_kernel();
    void generate_noise_texture();

    rhi::RHI* rhi_{nullptr};
    rhi::ShaderHandle ssao_shader_{rhi::INVALID_HANDLE};
    rhi::ShaderHandle blur_shader_{rhi::INVALID_HANDLE};
    rhi::PipelineHandle ssao_pipeline_{rhi::INVALID_HANDLE};
    rhi::PipelineHandle blur_pipeline_{rhi::INVALID_HANDLE};
    rhi::BufferHandle quad_vbo_{rhi::INVALID_HANDLE};

    // SSAO FBO + texture
    rhi::FramebufferHandle ssao_fb_{rhi::INVALID_HANDLE};
    rhi::TextureHandle ssao_tex_{rhi::INVALID_HANDLE};

    // Blur FBO + texture
    rhi::FramebufferHandle blur_fb_{rhi::INVALID_HANDLE};
    rhi::TextureHandle blur_tex_{rhi::INVALID_HANDLE};

    // Noise texture (4x4 random rotations)
    rhi::TextureHandle noise_tex_{rhi::INVALID_HANDLE};

    // G-buffer inputs
    rhi::TextureHandle depth_tex_{rhi::INVALID_HANDLE};
    rhi::TextureHandle normal_tex_{rhi::INVALID_HANDLE};

    Mat4 projection_{1.0f};
    u32 width_{0}, height_{0};

    // Kernel samples (hemisphere, tangent-space)
    static constexpr u32 MAX_KERNEL_SIZE = 64;
    std::array<Vec3, MAX_KERNEL_SIZE> kernel_;
};

} // namespace nexus
