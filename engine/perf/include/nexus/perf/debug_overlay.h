#pragma once

#include <nexus/core/types.h>
#include <nexus/renderer/camera.h>

#include <string>

namespace nexus {

// Forward declarations
class Profiler;
class BatchRenderer2D;
namespace ui { class BitmapFont; }
namespace rhi { class RHI; }

// ---------------------------------------------------------------------------
// DebugOverlay - renders profiling stats (FPS, frame time, draw calls)
// ---------------------------------------------------------------------------
class DebugOverlay {
public:
    DebugOverlay(Profiler* profiler, BatchRenderer2D* renderer, ui::BitmapFont* font);

    /// Render the debug overlay in screen-space.
    void render(int screen_width, int screen_height);

    /// Set the RHI texture handle for the bitmap font atlas.
    void set_font_texture(u32 texture_handle) { font_texture_ = texture_handle; }

    // --- Display toggles ---
    bool show_fps{true};
    bool show_frame_time{true};
    bool show_draw_calls{true};

    // --- Appearance ---
    Vec4 text_color{0.0f, 1.0f, 0.0f, 1.0f};       // green by default
    Vec4 background_color{0.0f, 0.0f, 0.0f, 0.6f};  // semi-transparent black
    float padding{8.0f};
    float line_spacing{4.0f};

private:
    void render_text(const std::string& text, Vec2 position);

    Profiler*         profiler_{nullptr};
    BatchRenderer2D*  renderer_{nullptr};
    ui::BitmapFont*   font_{nullptr};
    u32               font_texture_{0};
    Camera2D          screen_camera_;
};

} // namespace nexus
