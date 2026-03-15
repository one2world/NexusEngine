#include <gtest/gtest.h>
#include "nexus/ui/ui_core.h"

using namespace nexus;
using namespace nexus::ui;

// =============================================================================
// SizeValue Tests
// =============================================================================

TEST(SizeValue, FixedPixels) {
    auto sv = SizeValue::px(100.0f);
    EXPECT_EQ(sv.mode, SizeMode::Fixed);
    EXPECT_FLOAT_EQ(sv.value, 100.0f);
}

TEST(SizeValue, Percentage) {
    auto sv = SizeValue::pct(50.0f);
    EXPECT_EQ(sv.mode, SizeMode::Percent);
    EXPECT_FLOAT_EQ(sv.value, 50.0f);
}

TEST(SizeValue, AutoSize) {
    auto sv = SizeValue::auto_size();
    EXPECT_EQ(sv.mode, SizeMode::Auto);
}

TEST(SizeValue, GrowWeight) {
    auto sv = SizeValue::grow(2.0f);
    EXPECT_EQ(sv.mode, SizeMode::Grow);
    EXPECT_FLOAT_EQ(sv.value, 2.0f);
}

TEST(SizeValue, GrowDefault) {
    auto sv = SizeValue::grow();
    EXPECT_FLOAT_EQ(sv.value, 1.0f);
}

// =============================================================================
// EdgeInsets Tests
// =============================================================================

TEST(EdgeInsets, DefaultZero) {
    EdgeInsets ei;
    EXPECT_FLOAT_EQ(ei.top, 0.0f);
    EXPECT_FLOAT_EQ(ei.right, 0.0f);
    EXPECT_FLOAT_EQ(ei.bottom, 0.0f);
    EXPECT_FLOAT_EQ(ei.left, 0.0f);
}

TEST(EdgeInsets, UniformAll) {
    EdgeInsets ei(10.0f);
    EXPECT_FLOAT_EQ(ei.top, 10.0f);
    EXPECT_FLOAT_EQ(ei.right, 10.0f);
    EXPECT_FLOAT_EQ(ei.bottom, 10.0f);
    EXPECT_FLOAT_EQ(ei.left, 10.0f);
}

TEST(EdgeInsets, VerticalHorizontal) {
    EdgeInsets ei(5.0f, 10.0f);
    EXPECT_FLOAT_EQ(ei.top, 5.0f);
    EXPECT_FLOAT_EQ(ei.right, 10.0f);
    EXPECT_FLOAT_EQ(ei.bottom, 5.0f);
    EXPECT_FLOAT_EQ(ei.left, 10.0f);
    EXPECT_FLOAT_EQ(ei.horizontal(), 20.0f);
    EXPECT_FLOAT_EQ(ei.vertical(), 10.0f);
}

TEST(EdgeInsets, FourSides) {
    EdgeInsets ei(1.0f, 2.0f, 3.0f, 4.0f);
    EXPECT_FLOAT_EQ(ei.top, 1.0f);
    EXPECT_FLOAT_EQ(ei.right, 2.0f);
    EXPECT_FLOAT_EQ(ei.bottom, 3.0f);
    EXPECT_FLOAT_EQ(ei.left, 4.0f);
    EXPECT_FLOAT_EQ(ei.horizontal(), 6.0f);
    EXPECT_FLOAT_EQ(ei.vertical(), 4.0f);
}

// =============================================================================
// UIStyle Tests
// =============================================================================

TEST(UIStyle, Defaults) {
    UIStyle s;
    EXPECT_FLOAT_EQ(s.background_color.a, 0.0f);  // transparent
    EXPECT_FLOAT_EQ(s.text_color.r, 1.0f);
    EXPECT_FLOAT_EQ(s.font_size, 16.0f);
    EXPECT_FLOAT_EQ(s.opacity, 1.0f);
    EXPECT_FLOAT_EQ(s.border_width, 0.0f);
    EXPECT_FLOAT_EQ(s.corner_radius, 0.0f);
}

// =============================================================================
// UITheme Tests
// =============================================================================

TEST(UITheme, Defaults) {
    UITheme theme;
    EXPECT_FLOAT_EQ(theme.default_font_size, 16.0f);
    EXPECT_FLOAT_EQ(theme.border_width, 1.0f);
    EXPECT_FLOAT_EQ(theme.corner_radius, 4.0f);
    EXPECT_FLOAT_EQ(theme.padding, 8.0f);
    EXPECT_FLOAT_EQ(theme.spacing, 4.0f);
    EXPECT_GT(theme.primary_color.a, 0.0f);
    EXPECT_GT(theme.text_color.a, 0.0f);
}

TEST(UITheme, CustomColors) {
    UITheme theme;
    theme.primary_color = Vec4(1.0f, 0.0f, 0.0f, 1.0f);
    theme.text_color = Vec4(0.0f, 1.0f, 0.0f, 1.0f);
    EXPECT_FLOAT_EQ(theme.primary_color.r, 1.0f);
    EXPECT_FLOAT_EQ(theme.text_color.g, 1.0f);
}

// =============================================================================
// UIEvent Tests
// =============================================================================

TEST(UIEvent, DefaultNone) {
    UIEvent event;
    EXPECT_EQ(event.type, UIEventType::None);
    EXPECT_FALSE(event.consumed);
}

TEST(UIEvent, Consume) {
    UIEvent event;
    event.type = UIEventType::Click;
    EXPECT_FALSE(event.consumed);
    event.consume();
    EXPECT_TRUE(event.consumed);
}

// =============================================================================
// LayoutParams Tests
// =============================================================================

TEST(LayoutParams, Defaults) {
    LayoutParams lp;
    EXPECT_EQ(lp.direction, Direction::Column);
    EXPECT_EQ(lp.align_items, Alignment::Stretch);
    EXPECT_EQ(lp.justify, Justify::Start);
    EXPECT_EQ(lp.overflow_x, Overflow::Visible);
    EXPECT_EQ(lp.overflow_y, Overflow::Visible);
    EXPECT_EQ(lp.anchor, Anchor::TopLeft);
    EXPECT_FALSE(lp.absolute);
}

// =============================================================================
// Widget Tree Tests
// =============================================================================

TEST(Widget, UniqueIds) {
    auto a = std::make_shared<Widget>();
    auto b = std::make_shared<Widget>();
    EXPECT_NE(a->id(), b->id());
}

TEST(Widget, AddChild) {
    auto parent = std::make_shared<Widget>();
    auto child = std::make_shared<Widget>();
    parent->add_child(child);
    EXPECT_EQ(parent->children().size(), 1u);
    EXPECT_EQ(child->parent(), parent.get());
}

TEST(Widget, RemoveChild) {
    auto parent = std::make_shared<Widget>();
    auto child1 = std::make_shared<Widget>();
    auto child2 = std::make_shared<Widget>();
    parent->add_child(child1);
    parent->add_child(child2);
    EXPECT_EQ(parent->children().size(), 2u);

    parent->remove_child(child1->id());
    EXPECT_EQ(parent->children().size(), 1u);
    EXPECT_EQ(parent->children()[0]->id(), child2->id());
}

TEST(Widget, RemoveAllChildren) {
    auto parent = std::make_shared<Widget>();
    parent->add_child(std::make_shared<Widget>());
    parent->add_child(std::make_shared<Widget>());
    parent->add_child(std::make_shared<Widget>());
    EXPECT_EQ(parent->children().size(), 3u);

    parent->remove_all_children();
    EXPECT_EQ(parent->children().size(), 0u);
}

TEST(Widget, DefaultProperties) {
    Widget w;
    EXPECT_TRUE(w.visible);
    EXPECT_TRUE(w.interactive);
    EXPECT_FALSE(w.focusable);
    EXPECT_FALSE(w.focused);
    EXPECT_FALSE(w.hovered);
    EXPECT_FALSE(w.pressed);
    EXPECT_FALSE(w.disabled);
    EXPECT_FALSE(w.world_space);
}

TEST(Widget, EventCallback) {
    auto w = std::make_shared<Widget>();
    bool clicked = false;
    w->on(UIEventType::Click, [&](const UIEvent&) { clicked = true; });

    UIEvent event;
    event.type = UIEventType::Click;
    w->dispatch_event(event);
    EXPECT_TRUE(clicked);
}

TEST(Widget, DisabledIgnoresEvents) {
    auto w = std::make_shared<Widget>();
    w->disabled = true;
    bool clicked = false;
    w->on(UIEventType::Click, [&](const UIEvent&) { clicked = true; });

    UIEvent event;
    event.type = UIEventType::Click;
    w->dispatch_event(event);
    EXPECT_FALSE(clicked);
}

TEST(Widget, InvisibleIgnoresEvents) {
    auto w = std::make_shared<Widget>();
    w->visible = false;
    bool clicked = false;
    w->on(UIEventType::Click, [&](const UIEvent&) { clicked = true; });

    UIEvent event;
    event.type = UIEventType::Click;
    w->dispatch_event(event);
    EXPECT_FALSE(clicked);
}

// =============================================================================
// Layout Tests
// =============================================================================

TEST(Layout, FixedSize) {
    auto w = std::make_shared<Widget>();
    w->layout.width = SizeValue::px(200.0f);
    w->layout.height = SizeValue::px(100.0f);

    Rect available = {{0, 0}, {800, 600}};
    w->perform_layout(available);

    EXPECT_FLOAT_EQ(w->computed_rect().size.x, 200.0f);
    EXPECT_FLOAT_EQ(w->computed_rect().size.y, 100.0f);
}

TEST(Layout, PercentSize) {
    auto w = std::make_shared<Widget>();
    w->layout.width = SizeValue::pct(50.0f);
    w->layout.height = SizeValue::pct(25.0f);

    Rect available = {{0, 0}, {800, 600}};
    w->perform_layout(available);

    EXPECT_FLOAT_EQ(w->computed_rect().size.x, 400.0f);
    EXPECT_FLOAT_EQ(w->computed_rect().size.y, 150.0f);
}

TEST(Layout, ColumnChildren) {
    auto parent = std::make_shared<Widget>();
    parent->layout.width = SizeValue::px(200.0f);
    parent->layout.height = SizeValue::px(300.0f);
    parent->layout.direction = Direction::Column;

    auto child1 = std::make_shared<Widget>();
    child1->layout.width = SizeValue::pct(100);
    child1->layout.height = SizeValue::px(50.0f);

    auto child2 = std::make_shared<Widget>();
    child2->layout.width = SizeValue::pct(100);
    child2->layout.height = SizeValue::px(50.0f);

    parent->add_child(child1);
    parent->add_child(child2);

    Rect available = {{0, 0}, {800, 600}};
    parent->perform_layout(available);

    EXPECT_FLOAT_EQ(child1->computed_rect().position.y, 0.0f);
    EXPECT_FLOAT_EQ(child1->computed_rect().size.y, 50.0f);
    EXPECT_FLOAT_EQ(child2->computed_rect().position.y, 50.0f);
    EXPECT_FLOAT_EQ(child2->computed_rect().size.y, 50.0f);
}

TEST(Layout, RowChildren) {
    auto parent = std::make_shared<Widget>();
    parent->layout.width = SizeValue::px(400.0f);
    parent->layout.height = SizeValue::px(100.0f);
    parent->layout.direction = Direction::Row;

    auto child1 = std::make_shared<Widget>();
    child1->layout.width = SizeValue::px(100.0f);
    child1->layout.height = SizeValue::px(50.0f);

    auto child2 = std::make_shared<Widget>();
    child2->layout.width = SizeValue::px(150.0f);
    child2->layout.height = SizeValue::px(50.0f);

    parent->add_child(child1);
    parent->add_child(child2);

    Rect available = {{0, 0}, {800, 600}};
    parent->perform_layout(available);

    EXPECT_FLOAT_EQ(child1->computed_rect().position.x, 0.0f);
    EXPECT_FLOAT_EQ(child1->computed_rect().size.x, 100.0f);
    EXPECT_FLOAT_EQ(child2->computed_rect().position.x, 100.0f);
    EXPECT_FLOAT_EQ(child2->computed_rect().size.x, 150.0f);
}

TEST(Layout, GrowDistribution) {
    auto parent = std::make_shared<Widget>();
    parent->layout.width = SizeValue::px(300.0f);
    parent->layout.height = SizeValue::px(100.0f);
    parent->layout.direction = Direction::Row;

    auto child1 = std::make_shared<Widget>();
    child1->layout.width = SizeValue::grow(1);
    child1->layout.height = SizeValue::px(50.0f);

    auto child2 = std::make_shared<Widget>();
    child2->layout.width = SizeValue::grow(2);
    child2->layout.height = SizeValue::px(50.0f);

    parent->add_child(child1);
    parent->add_child(child2);

    Rect available = {{0, 0}, {800, 600}};
    parent->perform_layout(available);

    EXPECT_NEAR(child1->computed_rect().size.x, 100.0f, 1.0f);
    EXPECT_NEAR(child2->computed_rect().size.x, 200.0f, 1.0f);
}

TEST(Layout, Padding) {
    auto parent = std::make_shared<Widget>();
    parent->layout.width = SizeValue::px(200.0f);
    parent->layout.height = SizeValue::px(200.0f);
    parent->layout.padding = EdgeInsets(20.0f);

    auto child = std::make_shared<Widget>();
    child->layout.width = SizeValue::px(50.0f);
    child->layout.height = SizeValue::px(50.0f);

    parent->add_child(child);

    Rect available = {{0, 0}, {800, 600}};
    parent->perform_layout(available);

    EXPECT_FLOAT_EQ(child->computed_rect().position.x, 20.0f);
    EXPECT_FLOAT_EQ(child->computed_rect().position.y, 20.0f);
}

TEST(Layout, JustifyCenter) {
    auto parent = std::make_shared<Widget>();
    parent->layout.width = SizeValue::px(200.0f);
    parent->layout.height = SizeValue::px(200.0f);
    parent->layout.direction = Direction::Column;
    parent->layout.justify = Justify::Center;

    auto child = std::make_shared<Widget>();
    child->layout.width = SizeValue::px(50.0f);
    child->layout.height = SizeValue::px(50.0f);

    parent->add_child(child);

    Rect available = {{0, 0}, {800, 600}};
    parent->perform_layout(available);

    // Child should be vertically centered
    EXPECT_NEAR(child->computed_rect().position.y, 75.0f, 1.0f);
}

TEST(Layout, Spacing) {
    auto parent = std::make_shared<Widget>();
    parent->layout.width = SizeValue::px(400.0f);
    parent->layout.height = SizeValue::px(200.0f);
    parent->layout.direction = Direction::Column;
    parent->layout.spacing = 10.0f;

    auto child1 = std::make_shared<Widget>();
    child1->layout.width = SizeValue::px(50.0f);
    child1->layout.height = SizeValue::px(30.0f);

    auto child2 = std::make_shared<Widget>();
    child2->layout.width = SizeValue::px(50.0f);
    child2->layout.height = SizeValue::px(30.0f);

    parent->add_child(child1);
    parent->add_child(child2);

    Rect available = {{0, 0}, {800, 600}};
    parent->perform_layout(available);

    EXPECT_FLOAT_EQ(child1->computed_rect().position.y, 0.0f);
    EXPECT_FLOAT_EQ(child2->computed_rect().position.y, 40.0f); // 30 + 10 spacing
}

TEST(Layout, AbsolutePositioning) {
    auto parent = std::make_shared<Widget>();
    parent->layout.width = SizeValue::px(400.0f);
    parent->layout.height = SizeValue::px(300.0f);

    auto child = std::make_shared<Widget>();
    child->layout.absolute = true;
    child->layout.anchor = Anchor::Center;
    child->layout.width = SizeValue::px(100.0f);
    child->layout.height = SizeValue::px(50.0f);

    parent->add_child(child);

    Rect available = {{0, 0}, {400, 300}};
    parent->perform_layout(available);

    EXPECT_NEAR(child->computed_rect().position.x, 150.0f, 1.0f);
    EXPECT_NEAR(child->computed_rect().position.y, 125.0f, 1.0f);
}

// =============================================================================
// Draw Commands Tests
// =============================================================================

TEST(Widget, EmptyDrawCommands) {
    Widget w;
    std::vector<Widget::DrawCommand> commands;
    w.collect_draw_commands(commands);
    EXPECT_TRUE(commands.empty());
}

TEST(Widget, InvisibleNoDrawCommands) {
    Widget w;
    w.visible = false;
    std::vector<Widget::DrawCommand> commands;
    w.collect_draw_commands(commands);
    EXPECT_TRUE(commands.empty());
}

// =============================================================================
// NineSlice Tests
// =============================================================================

TEST(NineSlice, Defaults) {
    NineSlice ns;
    EXPECT_EQ(ns.texture, UI_INVALID_HANDLE);
    EXPECT_FLOAT_EQ(ns.border_left, 0.0f);
    EXPECT_FLOAT_EQ(ns.border_right, 0.0f);
    EXPECT_FLOAT_EQ(ns.border_top, 0.0f);
    EXPECT_FLOAT_EQ(ns.border_bottom, 0.0f);
}
