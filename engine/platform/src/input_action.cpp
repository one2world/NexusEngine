#include "nexus/platform/input_action.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include <cstring>

namespace nexus {

// ── Gamepad ─────────────────────────────────────────────────────────────────

float Gamepad::deadzone = 0.15f;
Gamepad::State Gamepad::s_states[MAX_GAMEPADS]{};

void Gamepad::update() {
    for (i32 id = 0; id < MAX_GAMEPADS; ++id) {
        auto& state = s_states[id];
        std::memcpy(state.prev_buttons, state.buttons, sizeof(state.buttons));

        GLFWgamepadstate gp;
        if (glfwGetGamepadState(GLFW_JOYSTICK_1 + id, &gp)) {
            state.connected = true;
            for (i32 b = 0; b < MAX_BUTTONS && b <= GLFW_GAMEPAD_BUTTON_LAST; ++b) {
                state.buttons[b] = gp.buttons[b] == GLFW_PRESS;
            }
            for (i32 a = 0; a < MAX_AXES && a <= GLFW_GAMEPAD_AXIS_LAST; ++a) {
                float v = gp.axes[a];
                // Apply deadzone
                if (std::abs(v) < deadzone) v = 0.0f;
                state.axes[a] = v;
            }
        } else {
            state.connected = false;
            std::memset(state.buttons, 0, sizeof(state.buttons));
            std::memset(state.axes, 0, sizeof(state.axes));
        }
    }
}

bool Gamepad::is_connected(i32 id) {
    return id >= 0 && id < MAX_GAMEPADS && s_states[id].connected;
}

bool Gamepad::button_down(GamepadButton btn, i32 id) {
    if (id < 0 || id >= MAX_GAMEPADS) return false;
    i32 b = static_cast<i32>(btn);
    return b >= 0 && b < MAX_BUTTONS && s_states[id].buttons[b];
}

bool Gamepad::button_pressed(GamepadButton btn, i32 id) {
    if (id < 0 || id >= MAX_GAMEPADS) return false;
    i32 b = static_cast<i32>(btn);
    return b >= 0 && b < MAX_BUTTONS &&
           s_states[id].buttons[b] && !s_states[id].prev_buttons[b];
}

bool Gamepad::button_released(GamepadButton btn, i32 id) {
    if (id < 0 || id >= MAX_GAMEPADS) return false;
    i32 b = static_cast<i32>(btn);
    return b >= 0 && b < MAX_BUTTONS &&
           !s_states[id].buttons[b] && s_states[id].prev_buttons[b];
}

float Gamepad::axis_value(GamepadAxis axis, i32 id) {
    if (id < 0 || id >= MAX_GAMEPADS) return 0.0f;
    i32 a = static_cast<i32>(axis);
    return (a >= 0 && a < MAX_AXES) ? s_states[id].axes[a] : 0.0f;
}

// ── InputMap ────────────────────────────────────────────────────────────────

void InputMap::add_action(const std::string& name, InputBinding binding) {
    actions_[name].push_back(binding);
}

void InputMap::remove_action(const std::string& name) {
    actions_.erase(name);
}

bool InputMap::is_action_down(const std::string& name) const {
    auto it = actions_.find(name);
    if (it == actions_.end()) return false;
    for (const auto& b : it->second) {
        switch (b.type) {
            case InputBinding::KeyBinding:
                if (Input::key_down(b.key)) return true;
                break;
            case InputBinding::MouseBinding:
                if (Input::mouse_down(b.mouse_button)) return true;
                break;
            case InputBinding::GamepadButtonBinding:
                if (Gamepad::button_down(b.gamepad_button, b.gamepad_id)) return true;
                break;
            case InputBinding::GamepadAxisBinding:
                if (std::abs(Gamepad::axis_value(b.gamepad_axis, b.gamepad_id)) > 0.5f) return true;
                break;
        }
    }
    return false;
}

bool InputMap::is_action_pressed(const std::string& name) const {
    auto it = actions_.find(name);
    if (it == actions_.end()) return false;
    for (const auto& b : it->second) {
        switch (b.type) {
            case InputBinding::KeyBinding:
                if (Input::key_pressed(b.key)) return true;
                break;
            case InputBinding::MouseBinding:
                if (Input::mouse_pressed(b.mouse_button)) return true;
                break;
            case InputBinding::GamepadButtonBinding:
                if (Gamepad::button_pressed(b.gamepad_button, b.gamepad_id)) return true;
                break;
            case InputBinding::GamepadAxisBinding:
                break; // axes don't have "pressed" semantics
        }
    }
    return false;
}

bool InputMap::is_action_released(const std::string& name) const {
    auto it = actions_.find(name);
    if (it == actions_.end()) return false;
    for (const auto& b : it->second) {
        switch (b.type) {
            case InputBinding::KeyBinding:
                if (Input::key_released(b.key)) return true;
                break;
            case InputBinding::MouseBinding:
                if (Input::mouse_released(b.mouse_button)) return true;
                break;
            case InputBinding::GamepadButtonBinding:
                if (Gamepad::button_released(b.gamepad_button, b.gamepad_id)) return true;
                break;
            case InputBinding::GamepadAxisBinding:
                break;
        }
    }
    return false;
}

float InputMap::get_action_strength(const std::string& name) const {
    auto it = actions_.find(name);
    if (it == actions_.end()) return 0.0f;
    float result = 0.0f;
    for (const auto& b : it->second) {
        float v = 0.0f;
        switch (b.type) {
            case InputBinding::KeyBinding:
                v = Input::key_down(b.key) ? 1.0f : 0.0f;
                break;
            case InputBinding::MouseBinding:
                v = Input::mouse_down(b.mouse_button) ? 1.0f : 0.0f;
                break;
            case InputBinding::GamepadButtonBinding:
                v = Gamepad::button_down(b.gamepad_button, b.gamepad_id) ? 1.0f : 0.0f;
                break;
            case InputBinding::GamepadAxisBinding:
                v = Gamepad::axis_value(b.gamepad_axis, b.gamepad_id);
                break;
        }
        v *= b.scale;
        // Keep the value with largest absolute magnitude
        if (std::abs(v) > std::abs(result)) result = v;
    }
    return result;
}

} // namespace nexus
