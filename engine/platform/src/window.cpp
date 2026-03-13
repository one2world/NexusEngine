#include "nexus/platform/window.h"
#include "nexus/core/log.h"

#include <GLFW/glfw3.h>
#include <stdexcept>

namespace nexus {

Window::Window(const WindowConfig& config)
    : width_(config.width), height_(config.height), vsync_(config.vsync) {

    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    GLFWmonitor* monitor = config.fullscreen ? glfwGetPrimaryMonitor() : nullptr;
    window_ = glfwCreateWindow(width_, height_, config.title.c_str(), monitor, nullptr);

    if (!window_) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(window_);
    glfwSetWindowUserPointer(window_, this);
    set_vsync(vsync_);
    setup_callbacks();

    NX_INFO("Window created: {}x{} '{}'", width_, height_, config.title);
}

Window::~Window() {
    if (window_) {
        glfwDestroyWindow(window_);
    }
    glfwTerminate();
}

void Window::poll_events() {
    glfwPollEvents();
}

void Window::swap_buffers() {
    glfwSwapBuffers(window_);
}

bool Window::should_close() const {
    return glfwWindowShouldClose(window_);
}

void Window::set_title(const std::string& title) {
    glfwSetWindowTitle(window_, title.c_str());
}

void Window::set_vsync(bool enabled) {
    vsync_ = enabled;
    glfwSwapInterval(enabled ? 1 : 0);
}

void Window::setup_callbacks() {
    glfwSetFramebufferSizeCallback(window_, [](GLFWwindow* w, int width, int height) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
        self->width_ = width;
        self->height_ = height;
        self->events_.publish(WindowResizeEvent{width, height});
    });

    glfwSetWindowCloseCallback(window_, [](GLFWwindow* w) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
        self->events_.publish(WindowCloseEvent{});
    });

    glfwSetKeyCallback(window_, [](GLFWwindow* w, int key, int scancode, int action, int mods) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
        self->events_.publish(KeyEvent{key, scancode, action, mods});
    });

    glfwSetCursorPosCallback(window_, [](GLFWwindow* w, double x, double y) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
        self->events_.publish(MouseMoveEvent{x, y});
    });

    glfwSetMouseButtonCallback(window_, [](GLFWwindow* w, int button, int action, int mods) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
        self->events_.publish(MouseButtonEvent{button, action, mods});
    });

    glfwSetScrollCallback(window_, [](GLFWwindow* w, double xoffset, double yoffset) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
        self->events_.publish(MouseScrollEvent{xoffset, yoffset});
    });
}

} // namespace nexus
