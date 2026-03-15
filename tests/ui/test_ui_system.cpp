#include <gtest/gtest.h>
#include "nexus/ui/ui_system.h"

using namespace nexus;
using namespace nexus::ui;

// =============================================================================
// FocusNavigator Tests
// =============================================================================

TEST(FocusNavigator, EmptyNoFocus) {
    FocusNavigator nav;
    EXPECT_EQ(nav.current(), nullptr);
}

TEST(FocusNavigator, RebuildCollectsFocusable) {
    auto root = std::make_shared<Panel>();
    auto btn1 = std::make_shared<Button>("A");
    auto btn2 = std::make_shared<Button>("B");
    auto label = std::make_shared<Label>("C");  // not focusable

    root->add_child(btn1);
    root->add_child(btn2);
    root->add_child(label);

    FocusNavigator nav;
    nav.set_root(root.get());
    nav.rebuild();

    EXPECT_EQ(nav.focusable_list().size(), 2u);
}

TEST(FocusNavigator, MoveNextCycles) {
    auto root = std::make_shared<Panel>();
    auto btn1 = std::make_shared<Button>("A");
    auto btn2 = std::make_shared<Button>("B");
    root->add_child(btn1);
    root->add_child(btn2);

    FocusNavigator nav;
    nav.set_root(root.get());
    nav.rebuild();

    nav.move_next();
    EXPECT_EQ(nav.current(), btn1.get());
    EXPECT_TRUE(btn1->focused);

    nav.move_next();
    EXPECT_EQ(nav.current(), btn2.get());
    EXPECT_TRUE(btn2->focused);
    EXPECT_FALSE(btn1->focused);

    nav.move_next();  // wraps
    EXPECT_EQ(nav.current(), btn1.get());
}

TEST(FocusNavigator, MovePrev) {
    auto root = std::make_shared<Panel>();
    auto btn1 = std::make_shared<Button>("A");
    auto btn2 = std::make_shared<Button>("B");
    root->add_child(btn1);
    root->add_child(btn2);

    FocusNavigator nav;
    nav.set_root(root.get());
    nav.rebuild();

    nav.move_prev();
    EXPECT_EQ(nav.current(), btn2.get());  // wraps to last

    nav.move_prev();
    EXPECT_EQ(nav.current(), btn1.get());
}

TEST(FocusNavigator, FocusSpecific) {
    auto root = std::make_shared<Panel>();
    auto btn1 = std::make_shared<Button>("A");
    auto btn2 = std::make_shared<Button>("B");
    root->add_child(btn1);
    root->add_child(btn2);

    FocusNavigator nav;
    nav.set_root(root.get());
    nav.rebuild();

    nav.focus(btn2.get());
    EXPECT_EQ(nav.current(), btn2.get());
    EXPECT_TRUE(btn2->focused);
}

TEST(FocusNavigator, Clear) {
    auto root = std::make_shared<Panel>();
    auto btn = std::make_shared<Button>("A");
    root->add_child(btn);

    FocusNavigator nav;
    nav.set_root(root.get());
    nav.rebuild();

    nav.move_next();
    EXPECT_NE(nav.current(), nullptr);

    nav.clear();
    EXPECT_EQ(nav.current(), nullptr);
    EXPECT_FALSE(btn->focused);
}

TEST(FocusNavigator, SkipsDisabledWidgets) {
    auto root = std::make_shared<Panel>();
    auto btn1 = std::make_shared<Button>("A");
    auto btn2 = std::make_shared<Button>("B");
    btn2->disabled = true;
    auto btn3 = std::make_shared<Button>("C");
    root->add_child(btn1);
    root->add_child(btn2);
    root->add_child(btn3);

    FocusNavigator nav;
    nav.set_root(root.get());
    nav.rebuild();

    // btn2 is disabled, so focusable list should have 2
    EXPECT_EQ(nav.focusable_list().size(), 2u);
}

// =============================================================================
// UISystem Tests
// =============================================================================

TEST(UISystem, Construction) {
    UISystem sys;
    EXPECT_NE(sys.root(), nullptr);
}

TEST(UISystem, SetScreenSize) {
    UISystem sys;
    sys.set_screen_size(1920, 1080);
    sys.update();
    // Root should fill screen
    EXPECT_FLOAT_EQ(sys.root()->computed_rect().size.x, 1920.0f);
    EXPECT_FLOAT_EQ(sys.root()->computed_rect().size.y, 1080.0f);
}

TEST(UISystem, SetTheme) {
    UISystem sys;
    UITheme theme;
    theme.primary_color = Vec4(1.0f, 0.0f, 0.0f, 1.0f);
    sys.set_theme(theme);
    EXPECT_FLOAT_EQ(sys.theme().primary_color.r, 1.0f);
}

TEST(UISystem, HitTest) {
    UISystem sys;
    sys.set_screen_size(800, 600);

    auto btn = std::make_shared<Button>("Test");
    btn->layout.width = SizeValue::px(100);
    btn->layout.height = SizeValue::px(40);
    sys.root()->add_child(btn);
    sys.update();

    // Hit inside button
    Widget* hit = sys.hit_test(Vec2(50.0f, 20.0f));
    EXPECT_EQ(hit, btn.get());

    // Hit outside
    Widget* miss = sys.hit_test(Vec2(500.0f, 500.0f));
    EXPECT_EQ(miss, nullptr);
}

TEST(UISystem, MouseHoverEvents) {
    UISystem sys;
    sys.set_screen_size(800, 600);

    auto btn = std::make_shared<Button>("Test");
    btn->layout.width = SizeValue::px(100);
    btn->layout.height = SizeValue::px(40);
    sys.root()->add_child(btn);
    sys.update();

    bool entered = false;
    btn->on(UIEventType::MouseEnter, [&](const UIEvent&) { entered = true; });

    sys.process_mouse_move(Vec2(50.0f, 20.0f));
    EXPECT_TRUE(entered);
    EXPECT_TRUE(btn->hovered);
}

TEST(UISystem, MouseClick) {
    UISystem sys;
    sys.set_screen_size(800, 600);

    auto btn = std::make_shared<Button>("Test");
    btn->layout.width = SizeValue::px(100);
    btn->layout.height = SizeValue::px(40);
    bool clicked = false;
    btn->on_click = [&](const UIEvent&) { clicked = true; };
    sys.root()->add_child(btn);
    sys.update();

    sys.process_mouse_move(Vec2(50.0f, 20.0f));
    sys.process_mouse_button(true);  // down
    sys.process_mouse_button(false); // up → click
    EXPECT_TRUE(clicked);
}

TEST(UISystem, NavigateNextPrev) {
    UISystem sys;
    sys.set_screen_size(800, 600);

    auto btn1 = std::make_shared<Button>("A");
    btn1->layout.width = SizeValue::px(100);
    btn1->layout.height = SizeValue::px(40);

    auto btn2 = std::make_shared<Button>("B");
    btn2->layout.width = SizeValue::px(100);
    btn2->layout.height = SizeValue::px(40);

    sys.root()->add_child(btn1);
    sys.root()->add_child(btn2);
    sys.update();

    sys.navigate_next();
    EXPECT_EQ(sys.focus().current(), btn1.get());

    sys.navigate_next();
    EXPECT_EQ(sys.focus().current(), btn2.get());

    sys.navigate_prev();
    EXPECT_EQ(sys.focus().current(), btn1.get());
}

TEST(UISystem, NavigateActivate) {
    UISystem sys;
    sys.set_screen_size(800, 600);

    auto btn = std::make_shared<Button>("OK");
    btn->layout.width = SizeValue::px(100);
    btn->layout.height = SizeValue::px(40);
    bool clicked = false;
    btn->on_click = [&](const UIEvent&) { clicked = true; };
    sys.root()->add_child(btn);
    sys.update();

    sys.navigate_next();
    sys.navigate_activate();
    EXPECT_TRUE(clicked);
}

TEST(UISystem, TextInputViaSystem) {
    UISystem sys;
    sys.set_screen_size(800, 600);

    auto input = std::make_shared<TextInput>();
    input->layout.width = SizeValue::px(200);
    input->layout.height = SizeValue::px(30);
    sys.root()->add_child(input);
    sys.update();

    // Focus the input
    sys.navigate_next();
    EXPECT_EQ(sys.focus().current(), input.get());

    // Type some text
    sys.process_text_input('H');
    sys.process_text_input('i');
    EXPECT_EQ(input->text, "Hi");
}

TEST(UISystem, DrawCommandsCollected) {
    UISystem sys;
    sys.set_screen_size(800, 600);

    auto btn = std::make_shared<Button>("Test");
    btn->layout.width = SizeValue::px(100);
    btn->layout.height = SizeValue::px(40);
    sys.root()->add_child(btn);
    sys.update();

    const auto& cmds = sys.draw_commands();
    EXPECT_GE(cmds.size(), 2u);  // at least background + text
}

TEST(UISystem, ScrollProcessing) {
    UISystem sys;
    sys.set_screen_size(800, 600);

    auto sv = std::make_shared<ScrollView>();
    sv->content_size = Vec2(200, 1000);
    sv->layout.width = SizeValue::px(200);
    sv->layout.height = SizeValue::px(200);
    sys.root()->add_child(sv);
    sys.update();

    sys.process_mouse_move(Vec2(100.0f, 100.0f));
    sys.process_scroll(-3.0f);
    EXPECT_GT(sv->scroll_offset.y, 0.0f);
}

TEST(UISystem, MouseLeaveOnMove) {
    UISystem sys;
    sys.set_screen_size(800, 600);

    auto btn = std::make_shared<Button>("Test");
    btn->layout.width = SizeValue::px(100);
    btn->layout.height = SizeValue::px(40);
    bool left = false;
    btn->on(UIEventType::MouseLeave, [&](const UIEvent&) { left = true; });
    sys.root()->add_child(btn);
    sys.update();

    sys.process_mouse_move(Vec2(50.0f, 20.0f));  // enter
    EXPECT_TRUE(btn->hovered);

    sys.process_mouse_move(Vec2(500.0f, 500.0f));  // leave
    EXPECT_TRUE(left);
    EXPECT_FALSE(btn->hovered);
}
