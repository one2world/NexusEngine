#pragma once

#include "nexus/core/types.h"
#include "nexus/core/timer.h"
#include "nexus/core/log.h"
#include <string>
#include <functional>

namespace nexus {

// ─────────────────────────────────────────────────────────────────────────────
// AppConfig — configuration for the Application
// ─────────────────────────────────────────────────────────────────────────────

struct AppConfig {
    std::string title = "NexusEngine";
    int width = 1280;
    int height = 720;
    bool vsync = true;
    bool fullscreen = false;
    float fixed_timestep = 1.0f / 60.0f;
    float max_frame_time = 0.25f; // cap to avoid spiral of death
};

// ─────────────────────────────────────────────────────────────────────────────
// Application — base class providing lifecycle hooks for game applications
// ─────────────────────────────────────────────────────────────────────────────

class Application {
public:
    Application() = default;
    virtual ~Application() = default;

    /// Override to provide custom configuration.
    virtual AppConfig configure() { return AppConfig{}; }

    /// Called once after engine systems are initialized.
    virtual void on_init() {}

    /// Called once per frame for game logic.
    virtual void on_update(float dt) { (void)dt; }

    /// Called at fixed timestep for physics/networking.
    virtual void on_fixed_update(float dt) { (void)dt; }

    /// Called once per frame for rendering.
    virtual void on_render() {}

    /// Called once per frame for UI overlay.
    virtual void on_ui() {}

    /// Called when the application is shutting down.
    virtual void on_shutdown() {}

    /// Called when the window is resized.
    virtual void on_resize(int width, int height) { (void)width; (void)height; }

    /// Run the main game loop. Handles timing, fixed update accumulation, and frame pacing.
    /// Returns exit code (0 = success).
    int run();

    /// Request the application to quit at the end of the frame.
    void quit() { running_ = false; }

    /// Check if the application is still running.
    bool is_running() const { return running_; }

    /// Get the frame delta time.
    float delta_time() const { return dt_; }

    /// Get total elapsed time.
    float elapsed_time() const { return elapsed_; }

    /// Get current FPS.
    float fps() const { return dt_ > 0.0f ? 1.0f / dt_ : 0.0f; }

    /// Get current frame number.
    u64 frame_count() const { return frame_count_; }

    /// Get the fixed timestep interpolation alpha for render smoothing.
    float fixed_alpha() const { return fixed_alpha_; }

protected:
    bool running_{true};
    float dt_{0.0f};
    float elapsed_{0.0f};
    u64 frame_count_{0};
    float fixed_alpha_{0.0f};
};

} // namespace nexus
