#pragma once

#include "nexus/renderer/post_process.h"
#include <array>

namespace nexus {

// ─────────────────────────────────────────────────────────────────────────────
// TAAEffect — Temporal Anti-Aliasing using sub-pixel jitter + reprojection
// ─────────────────────────────────────────────────────────────────────────────

class TAAEffect : public PostProcessEffect {
public:
    void init(rhi::RHI* rhi, u32 width, u32 height) override;
    void shutdown() override;
    void resize(u32 width, u32 height) override;
    void apply(rhi::RHI* rhi, rhi::TextureHandle input,
               rhi::FramebufferHandle dest) override;
    const std::string& name() const override { static std::string n = "TAA"; return n; }

    /// Get the current frame's jitter offset in NDC [-1, 1].
    /// Apply this to the projection matrix before rendering geometry.
    [[nodiscard]] Vec2 get_jitter_ndc() const { return jitter_ndc_; }

    /// Get jitter in pixel units.
    [[nodiscard]] Vec2 get_jitter_pixels() const { return jitter_pixels_; }

    /// Advance to the next frame's jitter. Call once per frame before rendering.
    void advance_frame();

    /// Set the motion vectors texture (screen-space velocity per pixel).
    void set_motion_vectors(rhi::TextureHandle motion_tex) { motion_vectors_ = motion_tex; }

    /// Set the depth texture (for disocclusion detection).
    void set_depth_texture(rhi::TextureHandle depth_tex) { depth_texture_ = depth_tex; }

    /// Feedback blend factor (higher = more temporal stability, lower = less ghosting).
    float feedback_factor{0.9f};

    /// Enable/disable neighborhood clamping (reduces ghosting).
    bool clamp_history{true};

private:
    rhi::ShaderHandle shader_{rhi::INVALID_HANDLE};
    rhi::PipelineHandle pipeline_{rhi::INVALID_HANDLE};
    rhi::BufferHandle quad_vbo_{rhi::INVALID_HANDLE};

    // Double-buffered history
    rhi::FramebufferHandle history_fb_[2] = {rhi::INVALID_HANDLE, rhi::INVALID_HANDLE};
    rhi::TextureHandle history_tex_[2] = {rhi::INVALID_HANDLE, rhi::INVALID_HANDLE};
    u32 history_index_{0};

    // Motion vectors and depth
    rhi::TextureHandle motion_vectors_{rhi::INVALID_HANDLE};
    rhi::TextureHandle depth_texture_{rhi::INVALID_HANDLE};

    // Jitter state
    u32 frame_index_{0};
    Vec2 jitter_ndc_{0.0f};
    Vec2 jitter_pixels_{0.0f};
    u32 width_{0}, height_{0};

    // Halton sequence for sub-pixel jitter
    static constexpr u32 JITTER_SEQUENCE_LENGTH = 16;
    static float halton(u32 index, u32 base);
    Vec2 compute_jitter(u32 frame) const;
};

} // namespace nexus
