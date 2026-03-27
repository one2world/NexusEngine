#pragma once

#include "nexus/core/types.h"
#include "nexus/core/event.h"
#include <string>

struct GLFWwindow;

namespace nexus {

struct WindowConfig {
    std::string title = "NexusEngine";
    int width = 1280;
    int height = 720;
    bool vsync = true;
    bool resizable = true;
    bool fullscreen = false;
};

class Window {
public:
    explicit Window(const WindowConfig& config);
    ~Window();

    NEXUS_NON_COPYABLE(Window)
    NEXUS_NON_MOVABLE(Window)

    void poll_events();
    void swap_buffers();
    [[nodiscard]] bool should_close() const;
    void set_title(const std::string& title);
    void set_vsync(bool enabled);

    /// Toggle between fullscreen and windowed mode at runtime.
    void set_fullscreen(bool fullscreen);

    /// Resize the window (windowed mode only).
    void set_size(int width, int height);

    /// Set cursor visibility/lock mode.
    enum class CursorMode : int { Normal = 0, Hidden, Locked };
    void set_cursor_mode(CursorMode mode);
    [[nodiscard]] CursorMode cursor_mode() const { return cursor_mode_; }

    /// Minimize/maximize the window.
    void minimize();
    void maximize();
    void restore();

    /// Set window position.
    void set_position(int x, int y);
    void get_position(int& x, int& y) const;

    [[nodiscard]] bool is_fullscreen() const { return fullscreen_; }
    [[nodiscard]] bool is_minimized() const;
    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }
    [[nodiscard]] float aspect_ratio() const {
        return height_ > 0 ? static_cast<float>(width_) / static_cast<float>(height_) : 1.0f;
    }
    [[nodiscard]] GLFWwindow* native_handle() const { return window_; }
    [[nodiscard]] EventBus& events() { return events_; }

private:
    void setup_callbacks();

    GLFWwindow* window_ = nullptr;
    int width_;
    int height_;
    bool vsync_;
    bool fullscreen_{false};
    CursorMode cursor_mode_{CursorMode::Normal};
    // Saved windowed state for fullscreen toggle
    int windowed_x_{0};
    int windowed_y_{0};
    int windowed_width_{0};
    int windowed_height_{0};
    EventBus events_;
};

} // namespace nexus
