#include <gtest/gtest.h>
#include "nexus/editor/panel.h"

using namespace nexus;
using namespace nexus::editor;

// Concrete test panel
class TestPanel : public Panel {
public:
    TestPanel(const std::string& title) : Panel(title) {}
    void on_render() override { render_count++; }
    const char* type_id() const override { return "TestPanel"; }

    int render_count{0};
};

// =============================================================================
// Panel base
// =============================================================================

TEST(Panel, TitleAndVisibility) {
    TestPanel p("Test");
    EXPECT_EQ(p.title(), "Test");
    EXPECT_TRUE(p.is_visible());
}

TEST(Panel, ToggleVisibility) {
    TestPanel p("Test");
    p.toggle_visible();
    EXPECT_FALSE(p.is_visible());
    p.toggle_visible();
    EXPECT_TRUE(p.is_visible());
}

TEST(Panel, Focus) {
    TestPanel p("Test");
    EXPECT_FALSE(p.is_focused());
    p.set_focused(true);
    EXPECT_TRUE(p.is_focused());
}

TEST(Panel, TypeId) {
    TestPanel p("Test");
    EXPECT_STREQ(p.type_id(), "TestPanel");
}

// =============================================================================
// DockSpace
// =============================================================================

TEST(DockSpace, InitialState) {
    DockSpace ds;
    EXPECT_EQ(ds.node_count(), 1u); // Root node
}

TEST(DockSpace, DockPanel) {
    DockSpace ds;
    ds.dock("Viewport", DockPosition::Center);
    EXPECT_EQ(ds.get_position("Viewport"), DockPosition::Center);
}

TEST(DockSpace, DockMultiplePanels) {
    DockSpace ds;
    ds.dock("Viewport", DockPosition::Center);
    ds.dock("Hierarchy", DockPosition::Left);
    ds.dock("Inspector", DockPosition::Right);

    EXPECT_EQ(ds.get_position("Viewport"), DockPosition::Center);
    EXPECT_EQ(ds.get_position("Hierarchy"), DockPosition::Left);
    EXPECT_EQ(ds.get_position("Inspector"), DockPosition::Right);
}

TEST(DockSpace, UndockPanel) {
    DockSpace ds;
    ds.dock("Viewport", DockPosition::Center);
    ds.undock("Viewport");
    EXPECT_EQ(ds.get_position("Viewport"), DockPosition::Float);
}

TEST(DockSpace, PanelsAtPosition) {
    DockSpace ds;
    ds.dock("A", DockPosition::Left);
    ds.dock("B", DockPosition::Left);
    ds.dock("C", DockPosition::Right);

    auto left = ds.panels_at(DockPosition::Left);
    EXPECT_EQ(left.size(), 2u);
    EXPECT_EQ(ds.panels_at(DockPosition::Right).size(), 1u);
    EXPECT_EQ(ds.panels_at(DockPosition::Bottom).size(), 0u);
}

TEST(DockSpace, UnknownPanelIsFloat) {
    DockSpace ds;
    EXPECT_EQ(ds.get_position("nonexistent"), DockPosition::Float);
}

// =============================================================================
// PanelManager
// =============================================================================

TEST(PanelManager, AddPanel) {
    PanelManager mgr;
    mgr.add_panel(std::make_unique<TestPanel>("A"));
    mgr.add_panel(std::make_unique<TestPanel>("B"));
    EXPECT_EQ(mgr.count(), 2u);
}

TEST(PanelManager, FindPanel) {
    PanelManager mgr;
    mgr.add_panel(std::make_unique<TestPanel>("Viewport"));
    EXPECT_NE(mgr.find("Viewport"), nullptr);
    EXPECT_EQ(mgr.find("Missing"), nullptr);
}

TEST(PanelManager, FindTyped) {
    PanelManager mgr;
    mgr.add_panel(std::make_unique<TestPanel>("A"));
    auto* p = mgr.find_typed<TestPanel>("A");
    ASSERT_NE(p, nullptr);
    EXPECT_STREQ(p->type_id(), "TestPanel");
}

TEST(PanelManager, RemovePanel) {
    PanelManager mgr;
    mgr.add_panel(std::make_unique<TestPanel>("A"));
    mgr.add_panel(std::make_unique<TestPanel>("B"));
    mgr.remove_panel("A");
    EXPECT_EQ(mgr.count(), 1u);
    EXPECT_EQ(mgr.find("A"), nullptr);
    EXPECT_NE(mgr.find("B"), nullptr);
}

TEST(PanelManager, RenderVisibleOnly) {
    PanelManager mgr;
    auto a = std::make_unique<TestPanel>("A");
    auto b = std::make_unique<TestPanel>("B");
    b->set_visible(false);

    auto* pa = a.get();
    auto* pb = b.get();

    mgr.add_panel(std::move(a));
    mgr.add_panel(std::move(b));
    mgr.render();

    EXPECT_EQ(pa->render_count, 1);
    EXPECT_EQ(pb->render_count, 0);
}

TEST(PanelManager, Focus) {
    PanelManager mgr;
    mgr.add_panel(std::make_unique<TestPanel>("A"));
    mgr.add_panel(std::make_unique<TestPanel>("B"));

    mgr.focus("A");
    EXPECT_TRUE(mgr.find("A")->is_focused());
    EXPECT_FALSE(mgr.find("B")->is_focused());

    mgr.focus("B");
    EXPECT_FALSE(mgr.find("A")->is_focused());
    EXPECT_TRUE(mgr.find("B")->is_focused());
}

TEST(PanelManager, FocusedPanel) {
    PanelManager mgr;
    mgr.add_panel(std::make_unique<TestPanel>("A"));
    EXPECT_EQ(mgr.focused(), nullptr);

    mgr.focus("A");
    EXPECT_NE(mgr.focused(), nullptr);
    EXPECT_EQ(mgr.focused()->title(), "A");
}
