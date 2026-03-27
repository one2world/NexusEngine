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
    BatchRenderer2D() = default;
    ~BatchRenderer2D() { shutdown(); }

    // Non-copyable, movable
    BatchRenderer2D(const BatchRenderer2D&) = delete;
    BatchRenderer2D& operator=(const BatchRenderer2D&) = delete;
    BatchRenderer2D(BatchRenderer2D&& other) noexcept;
    BatchRenderer2D& operator=(BatchRenderer2D&& other) noexcept;

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

    // ── Tilemap drawing ─────────────────────────────────────────────────
    /// Draw a tilemap. tile_data is a row-major grid of tile indices (-1 = empty).
    /// atlas_texture is the tile atlas, tiles_per_row/col describe its layout.
    void draw_tilemap(const i32* tile_data, u32 map_width, u32 map_height,
                      float tile_size, Vec2 origin,
                      rhi::TextureHandle atlas_texture,
                      u32 tiles_per_row, u32 tiles_per_col,
                      Vec4 tint = Vec4{1.0f, 1.0f, 1.0f, 1.0f});

    // ── Text drawing (bitmap font) ──────────────────────────────────────
    /// Draw a single glyph quad with atlas UV coordinates.
    void draw_glyph(Vec2 position, Vec2 size,
                    rhi::TextureHandle atlas_texture,
                    Vec2 uv_min, Vec2 uv_max,
                    Vec4 color);

    // ── 2D frustum culling ──────────────────────────────────────────────
    /// Check if an AABB (center + half-size) is within the current camera bounds.
    [[nodiscard]] bool is_visible_2d(Vec2 center, Vec2 half_size) const;

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

    // Camera bounds for 2D frustum culling
    Vec2 camera_min_{0.0f};
    Vec2 camera_max_{0.0f};
    bool culling_enabled_{false};

    // Cached quad corner positions (unit quad centred at origin)
    static constexpr Vec4 QUAD_POSITIONS[4] = {
        {-0.5f, -0.5f, 0.0f, 1.0f},
        { 0.5f, -0.5f, 0.0f, 1.0f},
        { 0.5f,  0.5f, 0.0f, 1.0f},
        {-0.5f,  0.5f, 0.0f, 1.0f}
    };
};

} // namespace nexus
