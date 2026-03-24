#include <gtest/gtest.h>
#include <nexus/platform/input_action.h>

namespace nexus::tests {

TEST(InputBinding, CreateFromKey) {
    auto b = InputBinding::from_key(Key::Space);
    EXPECT_EQ(b.type, InputBinding::KeyBinding);
    EXPECT_EQ(b.key, Key::Space);
    EXPECT_FLOAT_EQ(b.scale, 1.0f);
}

TEST(InputBinding, CreateFromMouse) {
    auto b = InputBinding::from_mouse(MouseButton::Left);
    EXPECT_EQ(b.type, InputBinding::MouseBinding);
    EXPECT_EQ(b.mouse_button, MouseButton::Left);
}

TEST(InputBinding, CreateFromGamepadButton) {
    auto b = InputBinding::from_gamepad_button(GamepadButton::A, 1.0f, 0);
    EXPECT_EQ(b.type, InputBinding::GamepadButtonBinding);
    EXPECT_EQ(b.gamepad_button, GamepadButton::A);
    EXPECT_EQ(b.gamepad_id, 0);
}

TEST(InputBinding, CreateFromGamepadAxis) {
    auto b = InputBinding::from_gamepad_axis(GamepadAxis::LeftX, -1.0f, 1);
    EXPECT_EQ(b.type, InputBinding::GamepadAxisBinding);
    EXPECT_EQ(b.gamepad_axis, GamepadAxis::LeftX);
    EXPECT_FLOAT_EQ(b.scale, -1.0f);
    EXPECT_EQ(b.gamepad_id, 1);
}

TEST(InputMap, AddAndRemoveAction) {
    InputMap map;
    map.add_action("jump", InputBinding::from_key(Key::Space));
    map.add_action("jump", InputBinding::from_gamepad_button(GamepadButton::A));

    // Can't easily test is_action_down without GLFW input, but we can test
    // that remove doesn't crash
    map.remove_action("jump");

    // Querying a removed action should return false/0
    EXPECT_FALSE(map.is_action_down("jump"));
    EXPECT_FLOAT_EQ(map.get_action_strength("jump"), 0.0f);
}

TEST(InputMap, NonexistentActionReturnsFalse) {
    InputMap map;
    EXPECT_FALSE(map.is_action_down("nonexistent"));
    EXPECT_FALSE(map.is_action_pressed("nonexistent"));
    EXPECT_FALSE(map.is_action_released("nonexistent"));
    EXPECT_FLOAT_EQ(map.get_action_strength("nonexistent"), 0.0f);
}

TEST(GamepadEnums, ButtonRange) {
    EXPECT_EQ(static_cast<i32>(GamepadButton::A), 0);
    EXPECT_GT(static_cast<i32>(GamepadButton::MaxButtons), 10);
}

TEST(GamepadEnums, AxisRange) {
    EXPECT_EQ(static_cast<i32>(GamepadAxis::LeftX), 0);
    EXPECT_GT(static_cast<i32>(GamepadAxis::MaxAxes), 4);
}

} // namespace nexus::tests
