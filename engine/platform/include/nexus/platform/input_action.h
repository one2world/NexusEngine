#pragma once

#include "nexus/core/types.h"
#include "nexus/platform/input.h"
#include <string>
#include <vector>
#include <unordered_map>

struct GLFWwindow;

namespace nexus {

// ─────────────────────────────────────────────────────────────────────────────
// Gamepad enums
// ─────────────────────────────────────────────────────────────────────────────

enum class GamepadButton : i32 {
    A = 0, B, X, Y,
    LeftBumper, RightBumper,
    Back, Start, Guide,
    LeftThumb, RightThumb,
    DPadUp, DPadRight, DPadDown, DPadLeft,
    MaxButtons
};

enum class GamepadAxis : i32 {
    LeftX = 0, LeftY,
    RightX, RightY,
    LeftTrigger, RightTrigger,
    MaxAxes
};

// ─────────────────────────────────────────────────────────────────────────────
// Gamepad - polls GLFW joystick/gamepad state
// ─────────────────────────────────────────────────────────────────────────────

class Gamepad {
public:
    static void update();

    [[nodiscard]] static bool is_connected(i32 id = 0);
    [[nodiscard]] static bool button_down(GamepadButton btn, i32 id = 0);
    [[nodiscard]] static bool button_pressed(GamepadButton btn, i32 id = 0);
    [[nodiscard]] static bool button_released(GamepadButton btn, i32 id = 0);
    [[nodiscard]] static float axis_value(GamepadAxis axis, i32 id = 0);

    static float deadzone;  // default 0.15

private:
    static constexpr i32 MAX_GAMEPADS = 4;
    static constexpr i32 MAX_BUTTONS = static_cast<i32>(GamepadButton::MaxButtons);
    static constexpr i32 MAX_AXES = static_cast<i32>(GamepadAxis::MaxAxes);

    struct State {
        bool connected{false};
        bool buttons[MAX_BUTTONS]{};
        bool prev_buttons[MAX_BUTTONS]{};
        float axes[MAX_AXES]{};
    };

    static State s_states[MAX_GAMEPADS];
};

// ─────────────────────────────────────────────────────────────────────────────
// InputBinding - a single key/button/axis that maps to an action
// ─────────────────────────────────────────────────────────────────────────────

struct InputBinding {
    enum Type : u8 { KeyBinding, MouseBinding, GamepadButtonBinding, GamepadAxisBinding };

    Type type;
    union {
        Key key;
        MouseButton mouse_button;
        GamepadButton gamepad_button;
        GamepadAxis gamepad_axis;
    };
    float scale{1.0f};         // for axis: multiplier (e.g., -1 for inverted)
    i32   gamepad_id{0};       // which gamepad

    static InputBinding from_key(Key k, float s = 1.0f) {
        InputBinding b; b.type = KeyBinding; b.key = k; b.scale = s; return b;
    }
    static InputBinding from_mouse(MouseButton m, float s = 1.0f) {
        InputBinding b; b.type = MouseBinding; b.mouse_button = m; b.scale = s; return b;
    }
    static InputBinding from_gamepad_button(GamepadButton g, float s = 1.0f, i32 id = 0) {
        InputBinding b; b.type = GamepadButtonBinding; b.gamepad_button = g; b.scale = s; b.gamepad_id = id; return b;
    }
    static InputBinding from_gamepad_axis(GamepadAxis a, float s = 1.0f, i32 id = 0) {
        InputBinding b; b.type = GamepadAxisBinding; b.gamepad_axis = a; b.scale = s; b.gamepad_id = id; return b;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// InputMap - maps named actions to one or more bindings
// ─────────────────────────────────────────────────────────────────────────────

class InputMap {
public:
    /// Add a binding to an action.
    void add_action(const std::string& name, InputBinding binding);

    /// Remove all bindings for an action.
    void remove_action(const std::string& name);

    /// Is any binding for this action currently held?
    [[nodiscard]] bool is_action_down(const std::string& name) const;

    /// Was any binding for this action just pressed this frame?
    [[nodiscard]] bool is_action_pressed(const std::string& name) const;

    /// Was any binding for this action just released this frame?
    [[nodiscard]] bool is_action_released(const std::string& name) const;

    /// Get the axis strength for an action (-1..1 for axis, 0/1 for buttons).
    [[nodiscard]] float get_action_strength(const std::string& name) const;

private:
    std::unordered_map<std::string, std::vector<InputBinding>> actions_;
};

} // namespace nexus
