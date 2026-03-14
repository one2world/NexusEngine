#pragma once

#include <nexus/core/types.h>
#include <nexus/core/math.h>
#include <nexus/rhi/rhi.h>
#include <nexus/renderer/camera.h>
#include <array>

namespace nexus {

// ─────────────────────────────────────────────────────────────────────────────
// BatchRenderer2D – auto-batching sprite / shape renderer
// ─────────────────────────────────────────────────────────────────────────────

class BatchRenderer2D {
public:
    // ── Constants ────────────────────────────────────────────────────────
    static constexpr u32 MAX_QUADS        = 10000;
    static constexpr u32 MAX_VERTICES     = MAX_QUADS * 4;
    static constexpr u32 MAX_INDICES      = MAX_QUADS * 6;
    static constexpr u32 MAX_TEXTURE_SLOTS = 16;

    // ── Per-vertex data ─────────────────────────────────────────────────
    struct Vertex {
        Vec3  position;
        Vec4  color;
        Vec2  texcoord;
        float tex_index;
    };

    // ── Per-frame statistics ────────────────────────────────────────────
    struct Stats {
        u32 draw_calls{0};
        u32 quad_count{0};
    };

    // ── Lifecycle ───────────────────────────────────────────────────────
    void init(rhi::RHI* rhi);
    void shutdown();

    // ── Frame scope ─────────────────────────────────────────────────────
    void begin(const Camera2D& camera);
    void end();

    // ── Quad drawing ────────────────────────────────────────────────────
    void draw_quad(Vec2 position, Vec2 size, Vec4 color);
    void draw_quad(Vec2 position, Vec2 size,
                   rhi::TextureHandle texture,
                   Vec4 tint = Vec4{1.0f, 1.0f, 1.0f, 1.0f});
    void draw_quad(Vec2 position, Vec2 size, float rotation, Vec4 color);
    void draw_quad(Vec2 position, Vec2 size, float rotation,
                   rhi::TextureHandle texture,
                   Vec4 tint   = Vec4{1.0f, 1.0f, 1.0f, 1.0f},
                   Vec2 uv_min = Vec2{0.0f, 0.0f},
                   Vec2 uv_max = Vec2{1.0f, 1.0f});

    // ── Shape drawing ───────────────────────────────────────────────────
    void draw_line(Vec2 start, Vec2 end, Vec4 color, float thickness = 1.0f);
    void draw_circle(Vec2 center, float radius, Vec4 color, i32 segments = 32);
    void draw_rect(Vec2 position, Vec2 size, Vec4 color, float thickness = 1.0f);

    // ── Statistics ──────────────────────────────────────────────────────
    [[nodiscard]] const Stats& get_stats() const { return stats_; }
    void reset_stats() { stats_ = {}; }

private:
    void flush();
    void start_batch();
    float find_or_add_texture(rhi::TextureHandle texture);

    rhi::RHI* rhi_{nullptr};

    // RHI resources
    rhi::ShaderHandle   shader_{rhi::INVALID_HANDLE};
    rhi::PipelineHandle pipeline_{rhi::INVALID_HANDLE};
    rhi::BufferHandle   vbo_{rhi::INVALID_HANDLE};
    rhi::BufferHandle   ibo_{rhi::INVALID_HANDLE};
    rhi::TextureHandle  white_texture_{rhi::INVALID_HANDLE};

    // Batch state
    std::array<Vertex, MAX_VERTICES> vertices_{};
    u32 vertex_count_{0};

    std::array<rhi::TextureHandle, MAX_TEXTURE_SLOTS> texture_slots_{};
    u32 texture_slot_index_{0};

    Stats stats_{};

    // Cached quad corner positions (unit quad centred at origin)
    static constexpr Vec4 QUAD_POSITIONS[4] = {
        {-0.5f, -0.5f, 0.0f, 1.0f},
        { 0.5f, -0.5f, 0.0f, 1.0f},
        { 0.5f,  0.5f, 0.0f, 1.0f},
        {-0.5f,  0.5f, 0.0f, 1.0f}
    };
};

} // namespace nexus
