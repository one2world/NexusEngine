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
    EventBus events_;
};

} // namespace nexus
