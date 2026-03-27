#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include "nexus/renderer/batch_renderer_2d.h"
#include "nexus/renderer/camera.h"
#include <vector>

namespace nexus {

// ─────────────────────────────────────────────────────────────────────────────
// TilemapRenderer — renders a 2D tilemap using BatchRenderer2D
// ─────────────────────────────────────────────────────────────────────────────

class TilemapRenderer {
public:
    struct TilemapData {
        u32 width{0};
        u32 height{0};
        float tile_size{1.0f};
        rhi::TextureHandle texture{rhi::INVALID_HANDLE};
        u32 tiles_per_row{16};
        u32 tiles_per_col{16};
        const i32* tiles{nullptr};
        Vec2 offset{0.0f, 0.0f};
        Vec4 tint{1.0f, 1.0f, 1.0f, 1.0f};
    };

    /// Render a tilemap, only drawing tiles visible to the camera.
    static void render(BatchRenderer2D& renderer, const Camera2D& camera,
                       Vec2 screen_size, const TilemapData& tilemap);
};

} // namespace nexus
