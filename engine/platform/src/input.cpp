#include "nexus/platform/input.h"
#include <GLFW/glfw3.h>

namespace nexus {

GLFWwindow* Input::s_window = nullptr;
std::array<bool, static_cast<size_t>(Key::MaxKeys)> Input::s_keys{};
std::array<bool, static_cast<size_t>(Key::MaxKeys)> Input::s_prev_keys{};
std::array<bool, static_cast<size_t>(MouseButton::MaxButtons)> Input::s_buttons{};
std::array<bool, static_cast<size_t>(MouseButton::MaxButtons)> Input::s_prev_buttons{};
Vec2 Input::s_mouse_pos{0.0f};
Vec2 Input::s_prev_mouse_pos{0.0f};
float Input::s_scroll_delta = 0.0f;

void Input::init(GLFWwindow* window) {
    s_window = window;
    s_keys.fill(false);
    s_prev_keys.fill(false);
    s_buttons.fill(false);
    s_prev_buttons.fill(false);
}

void Input::update() {
    s_prev_keys = s_keys;
    s_prev_buttons = s_buttons;
    s_prev_mouse_pos = s_mouse_pos;
    s_scroll_delta = 0.0f;

    for (int i = 0; i < static_cast<int>(Key::MaxKeys); ++i) {
        s_keys[static_cast<size_t>(i)] = glfwGetKey(s_window, i) == GLFW_PRESS;
    }

    for (int i = 0; i < static_cast<int>(MouseButton::MaxButtons); ++i) {
        s_buttons[static_cast<size_t>(i)] = glfwGetMouseButton(s_window, i) == GLFW_PRESS;
    }

    double mx, my;
    glfwGetCursorPos(s_window, &mx, &my);
    s_mouse_pos = Vec2(static_cast<float>(mx), static_cast<float>(my));
}

bool Input::key_down(Key key)     { return s_keys[static_cast<size_t>(key)]; }
bool Input::key_pressed(Key key)  { return s_keys[static_cast<size_t>(key)] && !s_prev_keys[static_cast<size_t>(key)]; }
bool Input::key_released(Key key) { return !s_keys[static_cast<size_t>(key)] && s_prev_keys[static_cast<size_t>(key)]; }

bool Input::mouse_down(MouseButton btn)     { return s_buttons[static_cast<size_t>(btn)]; }
bool Input::mouse_pressed(MouseButton btn)  { return s_buttons[static_cast<size_t>(btn)] && !s_prev_buttons[static_cast<size_t>(btn)]; }
bool Input::mouse_released(MouseButton btn) { return !s_buttons[static_cast<size_t>(btn)] && s_prev_buttons[static_cast<size_t>(btn)]; }

Vec2 Input::mouse_position() { return s_mouse_pos; }
Vec2 Input::mouse_delta()    { return s_mouse_pos - s_prev_mouse_pos; }
float Input::scroll_delta()  { return s_scroll_delta; }

} // namespace nexus
