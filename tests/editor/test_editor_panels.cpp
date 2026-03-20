#include <gtest/gtest.h>
#include "nexus/editor/editor_panels.h"

using namespace nexus;
using namespace nexus::editor;

// =============================================================================
// ViewportPanel
// =============================================================================

TEST(ViewportPanel, DefaultGizmo) {
    ViewportPanel vp;
    EXPECT_EQ(vp.gizmo_mode(), GizmoMode::Translate);
    EXPECT_EQ(vp.gizmo_space(), GizmoSpace::World);
}

TEST(ViewportPanel, SetGizmoMode) {
    ViewportPanel vp;
    vp.set_gizmo_mode(GizmoMode::Rotate);
    EXPECT_EQ(vp.gizmo_mode(), GizmoMode::Rotate);
    vp.set_gizmo_mode(GizmoMode::Scale);
    EXPECT_EQ(vp.gizmo_mode(), GizmoMode::Scale);
}

TEST(ViewportPanel, ToggleGizmoSpace) {
    ViewportPanel vp;
    EXPECT_EQ(vp.gizmo_space(), GizmoSpace::World);
    vp.toggle_gizmo_space();
    EXPECT_EQ(vp.gizmo_space(), GizmoSpace::Local);
    vp.toggle_gizmo_space();
    EXPECT_EQ(vp.gizmo_space(), GizmoSpace::World);
}

TEST(ViewportPanel, Grid) {
    ViewportPanel vp;
    EXPECT_TRUE(vp.is_grid_visible());
    vp.set_grid_visible(false);
    EXPECT_FALSE(vp.is_grid_visible());
}

TEST(ViewportPanel, ViewportSize) {
    ViewportPanel vp;
    vp.set_viewport_size(1920, 1080);
    EXPECT_EQ(vp.viewport_width(), 1920u);
    EXPECT_EQ(vp.viewport_height(), 1080u);
}

TEST(ViewportPanel, TypeId) {
    ViewportPanel vp;
    EXPECT_STREQ(vp.type_id(), "ViewportPanel");
}

// =============================================================================
// HierarchyPanel
// =============================================================================

TEST(HierarchyPanel, Selection) {
    HierarchyPanel hp;
    EXPECT_FALSE(hp.has_selection());

    hp.set_selected_entity(42);
    EXPECT_TRUE(hp.has_selection());
    EXPECT_EQ(hp.selected_entity(), 42u);

    hp.clear_selection();
    EXPECT_FALSE(hp.has_selection());
}

TEST(HierarchyPanel, MultiSelection) {
    HierarchyPanel hp;
    hp.add_to_selection(1);
    hp.add_to_selection(2);
    hp.add_to_selection(3);
    EXPECT_EQ(hp.multi_selection().size(), 3u);
    EXPECT_TRUE(hp.is_multi_selected(2));
    EXPECT_FALSE(hp.is_multi_selected(99));

    hp.remove_from_selection(2);
    EXPECT_EQ(hp.multi_selection().size(), 2u);
    EXPECT_FALSE(hp.is_multi_selected(2));
}

TEST(HierarchyPanel, DuplicateMultiSelect) {
    HierarchyPanel hp;
    hp.add_to_selection(1);
    hp.add_to_selection(1);
    EXPECT_EQ(hp.multi_selection().size(), 1u);
}

TEST(HierarchyPanel, ClearMultiSelection) {
    HierarchyPanel hp;
    hp.add_to_selection(1);
    hp.clear_multi_selection();
    EXPECT_TRUE(hp.multi_selection().empty());
}

TEST(HierarchyPanel, Filter) {
    HierarchyPanel hp;
    hp.set_filter("player");
    EXPECT_EQ(hp.filter(), "player");
}

TEST(HierarchyPanel, TypeId) {
    HierarchyPanel hp;
    EXPECT_STREQ(hp.type_id(), "HierarchyPanel");
}

// =============================================================================
// InspectorPanel
// =============================================================================

TEST(InspectorPanel, Target) {
    InspectorPanel ip;
    EXPECT_FALSE(ip.has_target());

    ip.set_target_entity(10);
    EXPECT_TRUE(ip.has_target());
    EXPECT_EQ(ip.target_entity(), 10u);

    ip.clear_target();
    EXPECT_FALSE(ip.has_target());
}

TEST(InspectorPanel, Locked) {
    InspectorPanel ip;
    EXPECT_FALSE(ip.is_locked());
    ip.set_locked(true);
    EXPECT_TRUE(ip.is_locked());
}

TEST(InspectorPanel, PendingEdits) {
    InspectorPanel ip;
    PropertyEdit edit;
    edit.component = "Transform";
    edit.property = "position.x";
    edit.old_value = "0";
    edit.new_value = "10";
    ip.push_edit(edit);

    auto edits = ip.drain_edits();
    EXPECT_EQ(edits.size(), 1u);
    EXPECT_EQ(edits[0].component, "Transform");

    // Drain should clear
    auto empty = ip.drain_edits();
    EXPECT_TRUE(empty.empty());
}

TEST(InspectorPanel, TypeId) {
    InspectorPanel ip;
    EXPECT_STREQ(ip.type_id(), "InspectorPanel");
}

// =============================================================================
// ConsolePanel
// =============================================================================

TEST(ConsolePanel, AddMessages) {
    ConsolePanel cp;
    cp.add_message("Hello", LogLevel::Info);
    cp.add_message("Warning!", LogLevel::Warning);
    EXPECT_EQ(cp.message_count(), 2u);
}

TEST(ConsolePanel, Clear) {
    ConsolePanel cp;
    cp.add_message("Hello");
    cp.clear();
    EXPECT_EQ(cp.message_count(), 0u);
}

TEST(ConsolePanel, LevelFilter) {
    ConsolePanel cp;
    EXPECT_TRUE(cp.is_level_shown(LogLevel::Info));
    cp.set_level_filter(LogLevel::Info, false);
    EXPECT_FALSE(cp.is_level_shown(LogLevel::Info));
    EXPECT_TRUE(cp.is_level_shown(LogLevel::Error));
}

TEST(ConsolePanel, MaxMessages) {
    ConsolePanel cp;
    cp.set_max_messages(3);
    cp.add_message("1");
    cp.add_message("2");
    cp.add_message("3");
    cp.add_message("4"); // Should prune oldest
    EXPECT_EQ(cp.message_count(), 3u);
    EXPECT_EQ(cp.messages().front().text, "2");
}

TEST(ConsolePanel, AutoScroll) {
    ConsolePanel cp;
    EXPECT_TRUE(cp.auto_scroll());
    cp.set_auto_scroll(false);
    EXPECT_FALSE(cp.auto_scroll());
}

TEST(ConsolePanel, TypeId) {
    ConsolePanel cp;
    EXPECT_STREQ(cp.type_id(), "ConsolePanel");
}

// =============================================================================
// AssetBrowserPanel
// =============================================================================

TEST(AssetBrowserPanel, Navigation) {
    AssetBrowserPanel ab;
    ab.navigate_to("assets");
    EXPECT_EQ(ab.current_path(), "assets");

    ab.navigate_to("assets/textures");
    EXPECT_EQ(ab.current_path(), "assets/textures");
    EXPECT_EQ(ab.history().size(), 2u);
}

TEST(AssetBrowserPanel, NavigateUp) {
    AssetBrowserPanel ab;
    ab.navigate_to("assets/textures/player");
    ab.navigate_up();
    EXPECT_EQ(ab.current_path(), "assets/textures");
}

TEST(AssetBrowserPanel, BackForward) {
    AssetBrowserPanel ab;
    ab.navigate_to("a");
    ab.navigate_to("b");
    ab.navigate_to("c");

    EXPECT_TRUE(ab.can_go_back());
    ab.go_back();
    EXPECT_EQ(ab.current_path(), "b");

    ab.go_back();
    EXPECT_EQ(ab.current_path(), "a");

    EXPECT_TRUE(ab.can_go_forward());
    ab.go_forward();
    EXPECT_EQ(ab.current_path(), "b");
}

TEST(AssetBrowserPanel, NavigateAfterBack) {
    AssetBrowserPanel ab;
    ab.navigate_to("a");
    ab.navigate_to("b");
    ab.go_back();
    ab.navigate_to("c"); // Should truncate forward history

    EXPECT_FALSE(ab.can_go_forward());
    EXPECT_EQ(ab.current_path(), "c");
}

TEST(AssetBrowserPanel, ViewMode) {
    AssetBrowserPanel ab;
    EXPECT_EQ(ab.view_mode(), AssetBrowserPanel::ViewMode::Grid);
    ab.set_view_mode(AssetBrowserPanel::ViewMode::List);
    EXPECT_EQ(ab.view_mode(), AssetBrowserPanel::ViewMode::List);
}

TEST(AssetBrowserPanel, ThumbnailSize) {
    AssetBrowserPanel ab;
    EXPECT_EQ(ab.thumbnail_size(), 64u);
    ab.set_thumbnail_size(128);
    EXPECT_EQ(ab.thumbnail_size(), 128u);
}

TEST(AssetBrowserPanel, Search) {
    AssetBrowserPanel ab;
    ab.set_search("player");
    EXPECT_EQ(ab.search(), "player");
}

TEST(AssetBrowserPanel, Entries) {
    AssetBrowserPanel ab;
    std::vector<AssetBrowserEntry> entries;
    entries.push_back({"texture.png", "assets/texture.png", ".png", false, 1024});
    entries.push_back({"models", "assets/models", "", true, 0});
    ab.set_entries(std::move(entries));
    EXPECT_EQ(ab.entries().size(), 2u);
}

TEST(AssetBrowserPanel, TypeId) {
    AssetBrowserPanel ab;
    EXPECT_STREQ(ab.type_id(), "AssetBrowserPanel");
}
