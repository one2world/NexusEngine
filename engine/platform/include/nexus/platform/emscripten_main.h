#pragma once

/// Emscripten main loop integration for NexusEngine.
///
/// On web builds, the browser controls the frame cadence via
/// requestAnimationFrame. This header provides a portable main-loop
/// abstraction that works on both desktop (polling loop) and
/// Emscripten (callback-based loop).
///
/// Usage:
///   nexus::platform::MainLoop loop;
///   loop.set_update([&](float dt) { game.update(dt); });
///   loop.set_render([&]()         { game.render(); });
///   loop.run(); // blocks on desktop, returns immediately on Emscripten

#include <nexus/core/types.h>
#include <functional>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

namespace nexus::platform {

class MainLoop {
public:
    using UpdateFn = std::function<void(f32 dt)>;
    using RenderFn = std::function<void()>;

    MainLoop() = default;
    ~MainLoop() = default;

    /// Set the update callback (called once per frame with delta time).
    void set_update(UpdateFn fn) { update_fn_ = std::move(fn); }

    /// Set the render callback (called once per frame after update).
    void set_render(RenderFn fn) { render_fn_ = std::move(fn); }

    /// Set target FPS (0 = unlimited / vsync). Default: 0.
    void set_target_fps(u32 fps) { target_fps_ = fps; }

    /// Whether the loop should keep running.
    void request_stop() { running_ = false; }
    [[nodiscard]] bool is_running() const { return running_; }

    /// Start the main loop.
    /// On desktop: blocks until request_stop() is called.
    /// On Emscripten: schedules the loop via emscripten_set_main_loop and returns.
    void run();

    /// Execute a single frame (called internally or for testing).
    void tick();

    /// Get current FPS measurement.
    [[nodiscard]] f32 fps() const { return fps_; }

    /// Get total elapsed time.
    [[nodiscard]] f64 elapsed() const { return elapsed_; }

    /// Get frame count.
    [[nodiscard]] u64 frame_count() const { return frame_count_; }

private:
    UpdateFn update_fn_;
    RenderFn render_fn_;
    u32  target_fps_{0};
    bool running_{true};
    f32  fps_{0.0f};
    f64  elapsed_{0.0};
    f64  last_time_{0.0};
    u64  frame_count_{0};

#ifdef __EMSCRIPTEN__
    static void emscripten_loop_callback(void* arg);
#endif
};

/// Query the canvas size (Emscripten only; returns window size on desktop).
struct CanvasSize {
    i32 width{800};
    i32 height{600};
};
CanvasSize get_canvas_size();

/// Set the canvas size (Emscripten only; no-op on desktop).
void set_canvas_size(i32 width, i32 height);

} // namespace nexus::platform
