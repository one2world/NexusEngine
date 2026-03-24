#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include "nexus/rhi/rhi.h"
#include "nexus/renderer/camera.h"
#include <vector>

namespace nexus {

namespace anim { struct Particle; }

// ─────────────────────────────────────────────────────────────────────────────
// ParticleRenderer - renders particles as camera-facing billboards
// ─────────────────────────────────────────────────────────────────────────────

class ParticleRenderer {
public:
    void init(rhi::RHI* rhi);
    void shutdown();

    /// Render a set of particles from the camera's viewpoint.
    void render(const Camera3D& camera, const std::vector<anim::Particle>& particles);

    /// Set the texture to use for particles (optional; defaults to white).
    void set_texture(rhi::TextureHandle tex) { texture_ = tex; }

    enum class BlendMode : u8 { Alpha, Additive };
    BlendMode blend_mode{BlendMode::Alpha};

    static constexpr u32 MAX_PARTICLES = 10000;

private:
    struct ParticleVertex {
        Vec3 position;
        Vec2 uv;
        Vec4 color;
        float size;
    };

    rhi::RHI*           rhi_{nullptr};
    rhi::ShaderHandle   shader_{rhi::INVALID_HANDLE};
    rhi::PipelineHandle pipeline_alpha_{rhi::INVALID_HANDLE};
    rhi::PipelineHandle pipeline_additive_{rhi::INVALID_HANDLE};
    rhi::BufferHandle   vbo_{rhi::INVALID_HANDLE};
    rhi::TextureHandle  texture_{rhi::INVALID_HANDLE};
};

} // namespace nexus
