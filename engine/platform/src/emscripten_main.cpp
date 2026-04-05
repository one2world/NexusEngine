#include "nexus/platform/emscripten_main.h"
#include "nexus/core/log.h"
#include <chrono>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

namespace nexus::platform {

// ── Time helpers ────────────────────────────────────────────────────────────

static f64 get_time_seconds() {
#ifdef __EMSCRIPTEN__
    return emscripten_get_now() / 1000.0;
#else
    using Clock = std::chrono::high_resolution_clock;
    static auto start = Clock::now();
    auto now = Clock::now();
    return std::chrono::duration<f64>(now - start).count();
#endif
}

// ── MainLoop ────────────────────────────────────────────────────────────────

void MainLoop::tick() {
    f64 now = get_time_seconds();
    f32 dt = (last_time_ > 0.0) ? static_cast<f32>(now - last_time_) : (1.0f / 60.0f);
    last_time_ = now;

    // Clamp dt to prevent spiral of death
    if (dt > 0.25f) dt = 0.25f;

    elapsed_ += static_cast<f64>(dt);
    ++frame_count_;

    // FPS calculation (exponential moving average)
    f32 instant_fps = (dt > 0.0001f) ? (1.0f / dt) : 0.0f;
    fps_ = fps_ * 0.95f + instant_fps * 0.05f;

    if (update_fn_) update_fn_(dt);
    if (render_fn_) render_fn_();
}

void MainLoop::run() {
    last_time_ = get_time_seconds();

#ifdef __EMSCRIPTEN__
    // On Emscripten, use the browser's requestAnimationFrame loop
    i32 fps = (target_fps_ > 0) ? static_cast<i32>(target_fps_) : 0;
    emscripten_set_main_loop_arg(emscripten_loop_callback, this, fps, 0);
    NX_INFO("Emscripten main loop started (target FPS: {})",
            target_fps_ > 0 ? target_fps_ : 0);
#else
    // Desktop: blocking loop
    NX_INFO("Desktop main loop started");
    while (running_) {
        tick();

        // Simple frame limiting for desktop
        if (target_fps_ > 0) {
            f64 frame_time = 1.0 / static_cast<f64>(target_fps_);
            f64 now = get_time_seconds();
            f64 sleep_time = frame_time - (now - last_time_);
            if (sleep_time > 0.001) {
                // Busy-wait for precision (sub-ms)
                while (get_time_seconds() - now < sleep_time) {}
            }
        }
    }
    NX_INFO("Main loop stopped after {} frames ({:.1f}s)",
            frame_count_, elapsed_);
#endif
}

#ifdef __EMSCRIPTEN__
void MainLoop::emscripten_loop_callback(void* arg) {
    auto* loop = static_cast<MainLoop*>(arg);
    if (!loop->running_) {
        emscripten_cancel_main_loop();
        return;
    }
    loop->tick();
}
#endif

// ── Canvas helpers ──────────────────────────────────────────────────────────

CanvasSize get_canvas_size() {
    CanvasSize size;
#ifdef __EMSCRIPTEN__
    emscripten_get_canvas_element_size("#canvas", &size.width, &size.height);
#else
    size.width = 800;
    size.height = 600;
#endif
    return size;
}

void set_canvas_size(i32 width, i32 height) {
#ifdef __EMSCRIPTEN__
    emscripten_set_canvas_element_size("#canvas", width, height);
#else
    (void)width; (void)height;
#endif
}

} // namespace nexus::platform
