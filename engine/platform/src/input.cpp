#include "nexus/platform/input.h"
#include <GLFW/glfw3.h>

namespace nexus {

namespace {
// GLFW key codes are sparse (Space=32, A=65, F1=290, …).  Calling
// glfwGetKey() with any unmapped index spams "invalid key" GLFW errors
// every frame, so poll only the codes the public Key enum actually
// exposes.
constexpr int kPolledKeys[] = {
    32, 39, 44, 45, 46, 47,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57,
    59, 61,
    65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77,
    78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90,
    91, 92, 93, 96,
    256, 257, 258, 259, 260, 261,
    262, 263, 264, 265,
    266, 267, 268, 269,
    280, 281, 282, 283, 284,
    290, 291, 292, 293, 294, 295, 296, 297, 298, 299, 300, 301,
    340, 341, 342, 343,
    344, 345, 346, 347,
    348,
};
} // namespace

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

    for (int code : kPolledKeys) {
        s_keys[static_cast<size_t>(code)] = glfwGetKey(s_window, code) == GLFW_PRESS;
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
