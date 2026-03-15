#include <gtest/gtest.h>
#include "nexus/ui/widgets.h"

using namespace nexus;
using namespace nexus::ui;

// =============================================================================
// Panel Tests
// =============================================================================

TEST(Panel, TypeName) {
    Panel panel;
    EXPECT_STREQ(panel.type_name(), "Panel");
}

TEST(Panel, DrawsBackgroundWhenOpaque) {
    auto panel = std::make_shared<Panel>();
    panel->style.background_color = Vec4(0.5f, 0.5f, 0.5f, 1.0f);
    panel->layout.width = SizeValue::px(100);
    panel->layout.height = SizeValue::px(100);

    Rect available = {{0, 0}, {800, 600}};
    panel->perform_layout(available);

    std::vector<Widget::DrawCommand> commands;
    panel->collect_draw_commands(commands);
    EXPECT_GE(commands.size(), 1u);
    EXPECT_EQ(commands[0].type, Widget::DrawCommand::Type::Rect);
}

TEST(Panel, NineSliceDrawCommand) {
    auto panel = std::make_shared<Panel>();
    panel->style.background_color = Vec4(1.0f);
    panel->style.nine_slice.texture = 42;
    panel->style.nine_slice.border_left = 10;
    panel->layout.width = SizeValue::px(100);
    panel->layout.height = SizeValue::px(100);

    Rect available = {{0, 0}, {800, 600}};
    panel->perform_layout(available);

    std::vector<Widget::DrawCommand> commands;
    panel->collect_draw_commands(commands);
    EXPECT_EQ(commands[0].type, Widget::DrawCommand::Type::NineSlice);
}

// =============================================================================
// Label Tests
// =============================================================================

TEST(Label, TypeName) {
    Label label("Hello");
    EXPECT_STREQ(label.type_name(), "Label");
    EXPECT_EQ(label.text, "Hello");
}

TEST(Label, MeasureContent) {
    auto label = std::make_shared<Label>("Test");
    label->layout.width = SizeValue::auto_size();
    label->layout.height = SizeValue::auto_size();

    Rect available = {{0, 0}, {800, 600}};
    label->perform_layout(available);

    // Width should be proportional to text length
    EXPECT_GT(label->computed_rect().size.x, 0.0f);
    EXPECT_GT(label->computed_rect().size.y, 0.0f);
}

TEST(Label, DrawsText) {
    auto label = std::make_shared<Label>("Hello World");
    label->layout.width = SizeValue::px(200);
    label->layout.height = SizeValue::px(30);

    Rect available = {{0, 0}, {800, 600}};
    label->perform_layout(available);

    std::vector<Widget::DrawCommand> commands;
    label->collect_draw_commands(commands);
    EXPECT_EQ(commands.size(), 1u);
    EXPECT_EQ(commands[0].type, Widget::DrawCommand::Type::Text);
    EXPECT_EQ(commands[0].text, "Hello World");
}

TEST(Label, DisabledColor) {
    auto label = std::make_shared<Label>("Disabled");
    label->disabled = true;
    label->layout.width = SizeValue::px(200);
    label->layout.height = SizeValue::px(30);

    Rect available = {{0, 0}, {800, 600}};
    label->perform_layout(available);

    std::vector<Widget::DrawCommand> commands;
    label->collect_draw_commands(commands);
    EXPECT_EQ(commands.size(), 1u);
    // Disabled text should be gray
    EXPECT_FLOAT_EQ(commands[0].color.r, 0.5f);
}

// =============================================================================
// Button Tests
// =============================================================================

TEST(Button, TypeName) {
    Button btn("Click Me");
    EXPECT_STREQ(btn.type_name(), "Button");
    EXPECT_EQ(btn.text, "Click Me");
    EXPECT_TRUE(btn.focusable);
    EXPECT_TRUE(btn.interactive);
}

TEST(Button, ClickCallback) {
    auto btn = std::make_shared<Button>("OK");
    bool clicked = false;
    btn->on_click = [&](const UIEvent&) { clicked = true; };

    btn->layout.width = SizeValue::px(100);
    btn->layout.height = SizeValue::px(40);
    Rect available = {{0, 0}, {800, 600}};
    btn->perform_layout(available);

    UIEvent event;
    event.type = UIEventType::Click;
    btn->dispatch_event(event);
    EXPECT_TRUE(clicked);
    EXPECT_TRUE(event.consumed);
}

TEST(Button, DisabledNoClick) {
    auto btn = std::make_shared<Button>("OK");
    btn->disabled = true;
    bool clicked = false;
    btn->on_click = [&](const UIEvent&) { clicked = true; };

    UIEvent event;
    event.type = UIEventType::Click;
    btn->dispatch_event(event);
    EXPECT_FALSE(clicked);
}

TEST(Button, DrawCommandsCount) {
    auto btn = std::make_shared<Button>("Test");
    btn->layout.width = SizeValue::px(100);
    btn->layout.height = SizeValue::px(40);

    Rect available = {{0, 0}, {800, 600}};
    btn->perform_layout(available);

    std::vector<Widget::DrawCommand> commands;
    btn->collect_draw_commands(commands);
    EXPECT_EQ(commands.size(), 2u);  // background + text
}

TEST(Button, HoveredStyle) {
    auto btn = std::make_shared<Button>("Test");
    btn->hovered = true;
    btn->layout.width = SizeValue::px(100);
    btn->layout.height = SizeValue::px(40);

    Rect available = {{0, 0}, {800, 600}};
    btn->perform_layout(available);

    std::vector<Widget::DrawCommand> commands;
    btn->collect_draw_commands(commands);
    // Hovered background should be brighter
    EXPECT_GT(commands[0].color.r, 0.3f);
}

// =============================================================================
// Slider Tests
// =============================================================================

TEST(Slider, TypeName) {
    Slider slider;
    EXPECT_STREQ(slider.type_name(), "Slider");
}

TEST(Slider, DefaultValues) {
    Slider slider;
    EXPECT_FLOAT_EQ(slider.value, 0.0f);
    EXPECT_FLOAT_EQ(slider.min_value, 0.0f);
    EXPECT_FLOAT_EQ(slider.max_value, 1.0f);
    EXPECT_FLOAT_EQ(slider.step, 0.0f);
    EXPECT_FALSE(slider.vertical);
    EXPECT_TRUE(slider.focusable);
}

TEST(Slider, SetValueClamps) {
    Slider slider;
    slider.min_value = 0.0f;
    slider.max_value = 10.0f;

    slider.set_value(5.0f);
    EXPECT_FLOAT_EQ(slider.value, 5.0f);

    slider.set_value(-1.0f);
    EXPECT_FLOAT_EQ(slider.value, 0.0f);

    slider.set_value(15.0f);
    EXPECT_FLOAT_EQ(slider.value, 10.0f);
}

TEST(Slider, SetValueWithStep) {
    Slider slider;
    slider.min_value = 0.0f;
    slider.max_value = 10.0f;
    slider.step = 2.5f;

    slider.set_value(3.3f);
    EXPECT_NEAR(slider.value, 2.5f, 0.01f);

    slider.set_value(4.0f);
    EXPECT_NEAR(slider.value, 5.0f, 0.01f);
}

TEST(Slider, Normalized) {
    Slider slider;
    slider.min_value = 0.0f;
    slider.max_value = 100.0f;
    slider.set_value(50.0f);
    EXPECT_NEAR(slider.normalized(), 0.5f, 0.001f);

    slider.set_value(0.0f);
    EXPECT_NEAR(slider.normalized(), 0.0f, 0.001f);

    slider.set_value(100.0f);
    EXPECT_NEAR(slider.normalized(), 1.0f, 0.001f);
}

TEST(Slider, DrawCommandsCount) {
    auto slider = std::make_shared<Slider>();
    slider->layout.width = SizeValue::px(120);
    slider->layout.height = SizeValue::px(24);

    Rect available = {{0, 0}, {800, 600}};
    slider->perform_layout(available);

    std::vector<Widget::DrawCommand> commands;
    slider->collect_draw_commands(commands);
    EXPECT_EQ(commands.size(), 3u);  // track + fill + thumb
}

// =============================================================================
// TextInput Tests
// =============================================================================

TEST(TextInput, TypeName) {
    TextInput input;
    EXPECT_STREQ(input.type_name(), "TextInput");
    EXPECT_TRUE(input.focusable);
}

TEST(TextInput, InsertChar) {
    TextInput input;
    input.insert_char('H');
    input.insert_char('i');
    EXPECT_EQ(input.text, "Hi");
    EXPECT_EQ(input.cursor(), 2u);
}

TEST(TextInput, DeleteBackward) {
    TextInput input;
    input.text = "Hello";
    input.set_cursor(5);
    input.delete_backward();
    EXPECT_EQ(input.text, "Hell");
    EXPECT_EQ(input.cursor(), 4u);
}

TEST(TextInput, DeleteForward) {
    TextInput input;
    input.text = "Hello";
    input.set_cursor(0);
    input.delete_forward();
    EXPECT_EQ(input.text, "ello");
    EXPECT_EQ(input.cursor(), 0u);
}

TEST(TextInput, SelectAll) {
    TextInput input;
    input.text = "Hello";
    input.select_all();
    EXPECT_TRUE(input.has_selection());
    EXPECT_EQ(input.selection_start(), 0u);
    EXPECT_EQ(input.selection_end(), 5u);
}

TEST(TextInput, DeleteSelection) {
    TextInput input;
    input.text = "Hello";
    input.select_all();
    input.delete_backward();
    EXPECT_EQ(input.text, "");
    EXPECT_EQ(input.cursor(), 0u);
    EXPECT_FALSE(input.has_selection());
}

TEST(TextInput, InsertReplacesSelection) {
    TextInput input;
    input.text = "Hello";
    input.select_all();
    input.insert_char('X');
    EXPECT_EQ(input.text, "X");
    EXPECT_EQ(input.cursor(), 1u);
}

TEST(TextInput, MaxLength) {
    TextInput input;
    input.max_length = 5;
    input.text = "ABCDE";
    input.set_cursor(5);
    input.insert_char('F');
    EXPECT_EQ(input.text, "ABCDE");  // should not grow
}

TEST(TextInput, SetCursorClamps) {
    TextInput input;
    input.text = "Hi";
    input.set_cursor(100);
    EXPECT_EQ(input.cursor(), 2u);
}

TEST(TextInput, PasswordDraws) {
    auto input = std::make_shared<TextInput>();
    input->text = "secret";
    input->password = true;
    input->focused = true;
    input->layout.width = SizeValue::px(200);
    input->layout.height = SizeValue::px(30);

    Rect available = {{0, 0}, {800, 600}};
    input->perform_layout(available);

    std::vector<Widget::DrawCommand> commands;
    input->collect_draw_commands(commands);
    // Find the text command
    bool found_masked = false;
    for (auto& cmd : commands) {
        if (cmd.type == Widget::DrawCommand::Type::Text) {
            EXPECT_EQ(cmd.text, "******");
            found_masked = true;
        }
    }
    EXPECT_TRUE(found_masked);
}

// =============================================================================
// Image Tests
// =============================================================================

TEST(ImageWidget, TypeName) {
    Image img;
    EXPECT_STREQ(img.type_name(), "Image");
}

TEST(ImageWidget, DefaultTint) {
    Image img;
    EXPECT_FLOAT_EQ(img.tint.r, 1.0f);
    EXPECT_FLOAT_EQ(img.tint.a, 1.0f);
}

TEST(ImageWidget, NoDrawWithoutTexture) {
    auto img = std::make_shared<Image>();
    img->layout.width = SizeValue::px(64);
    img->layout.height = SizeValue::px(64);

    Rect available = {{0, 0}, {800, 600}};
    img->perform_layout(available);

    std::vector<Widget::DrawCommand> commands;
    img->collect_draw_commands(commands);
    EXPECT_TRUE(commands.empty());
}

TEST(ImageWidget, DrawsWithTexture) {
    auto img = std::make_shared<Image>();
    img->texture = 42;
    img->layout.width = SizeValue::px(64);
    img->layout.height = SizeValue::px(64);

    Rect available = {{0, 0}, {800, 600}};
    img->perform_layout(available);

    std::vector<Widget::DrawCommand> commands;
    img->collect_draw_commands(commands);
    EXPECT_EQ(commands.size(), 1u);
    EXPECT_EQ(commands[0].type, Widget::DrawCommand::Type::Image);
    EXPECT_EQ(commands[0].texture, 42u);
}

// =============================================================================
// ProgressBar Tests
// =============================================================================

TEST(ProgressBar, TypeName) {
    ProgressBar pb;
    EXPECT_STREQ(pb.type_name(), "ProgressBar");
}

TEST(ProgressBar, DefaultValues) {
    ProgressBar pb;
    EXPECT_FLOAT_EQ(pb.value, 0.0f);
    EXPECT_TRUE(pb.show_text);
}

TEST(ProgressBar, DrawCommands) {
    auto pb = std::make_shared<ProgressBar>();
    pb->value = 0.5f;
    pb->layout.width = SizeValue::px(200);
    pb->layout.height = SizeValue::px(24);

    Rect available = {{0, 0}, {800, 600}};
    pb->perform_layout(available);

    std::vector<Widget::DrawCommand> commands;
    pb->collect_draw_commands(commands);
    EXPECT_EQ(commands.size(), 3u);  // background + fill + text
}

TEST(ProgressBar, ZeroValueNoFill) {
    auto pb = std::make_shared<ProgressBar>();
    pb->value = 0.0f;
    pb->show_text = false;
    pb->layout.width = SizeValue::px(200);
    pb->layout.height = SizeValue::px(24);

    Rect available = {{0, 0}, {800, 600}};
    pb->perform_layout(available);

    std::vector<Widget::DrawCommand> commands;
    pb->collect_draw_commands(commands);
    EXPECT_EQ(commands.size(), 1u);  // background only
}

// =============================================================================
// ScrollView Tests
// =============================================================================

TEST(ScrollView, TypeName) {
    ScrollView sv;
    EXPECT_STREQ(sv.type_name(), "ScrollView");
}

TEST(ScrollView, DefaultValues) {
    ScrollView sv;
    EXPECT_FALSE(sv.scroll_x);
    EXPECT_TRUE(sv.scroll_y);
    EXPECT_FLOAT_EQ(sv.scroll_speed, 20.0f);
}

TEST(ScrollView, ScrollToTop) {
    ScrollView sv;
    sv.scroll_offset.y = 100.0f;
    sv.scroll_to_top();
    EXPECT_FLOAT_EQ(sv.scroll_offset.y, 0.0f);
}

TEST(ScrollView, ScrollEvent) {
    auto sv = std::make_shared<ScrollView>();
    sv->content_size = Vec2(200, 500);
    sv->layout.width = SizeValue::px(200);
    sv->layout.height = SizeValue::px(200);

    Rect available = {{0, 0}, {800, 600}};
    sv->perform_layout(available);

    UIEvent event;
    event.type = UIEventType::Scroll;
    event.scroll_delta = -2.0f;
    sv->dispatch_event(event);
    EXPECT_GT(sv->scroll_offset.y, 0.0f);
    EXPECT_TRUE(event.consumed);
}
