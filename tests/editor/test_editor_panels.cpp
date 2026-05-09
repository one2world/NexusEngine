#include <gtest/gtest.h>
#include "nexus/editor/editor_panels.h"
#include "nexus/editor/editor_state.h"
#include "nexus/editor/undo_redo.h"
#include "nexus/editor/component_registry.h"
#include "nexus/perf/profiler.h"
#include <filesystem>

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

// ── M34: source classification + filter chips ──────────────────────────────

TEST(ConsolePanel, SourceClassifierMatchesPrefixes) {
    EXPECT_EQ(classify_console_source("plain log"),       LogSource::Engine);
    EXPECT_EQ(classify_console_source("[Lua] result"),    LogSource::Lua);
    EXPECT_EQ(classify_console_source("[Lua print] hi"),  LogSource::LuaPrint);
    EXPECT_EQ(classify_console_source("[Script] err"),    LogSource::ScriptError);
    EXPECT_EQ(classify_console_source("[Editor] note"),   LogSource::Editor);
}

TEST(ConsolePanel, SourceClassifierLuaPrintBeforeLua) {
    // The prefix "[Lua print]" starts with "[Lua" but must classify
    // as LuaPrint not Lua — order matters in the matcher.
    EXPECT_EQ(classify_console_source("[Lua print] x"), LogSource::LuaPrint);
    EXPECT_EQ(classify_console_source("[Lua] x"),       LogSource::Lua);
}

TEST(ConsolePanel, AddMessageAutoTagsSourceFromPrefix) {
    ConsolePanel cp;
    cp.add_message("[Lua] hello",       LogLevel::Info);
    cp.add_message("[Lua print] world", LogLevel::Info);
    cp.add_message("[Script] boom",     LogLevel::Error);
    cp.add_message("plain engine log",  LogLevel::Info);

    ASSERT_EQ(cp.message_count(), 4u);
    EXPECT_EQ(cp.messages()[0].source, LogSource::Lua);
    EXPECT_EQ(cp.messages()[1].source, LogSource::LuaPrint);
    EXPECT_EQ(cp.messages()[2].source, LogSource::ScriptError);
    EXPECT_EQ(cp.messages()[3].source, LogSource::Engine);
}

TEST(ConsolePanel, ExplicitSourceOverridesPrefix) {
    // The explicit overload trusts the caller — text prefix is ignored.
    ConsolePanel cp;
    cp.add_message("plain text", LogLevel::Info, LogSource::Lua);
    EXPECT_EQ(cp.messages().front().source, LogSource::Lua);
}

TEST(ConsolePanel, SourceFilterDefaultsAllShown) {
    ConsolePanel cp;
    EXPECT_TRUE(cp.is_source_shown(LogSource::Engine));
    EXPECT_TRUE(cp.is_source_shown(LogSource::Lua));
    EXPECT_TRUE(cp.is_source_shown(LogSource::LuaPrint));
    EXPECT_TRUE(cp.is_source_shown(LogSource::ScriptError));
    EXPECT_TRUE(cp.is_source_shown(LogSource::Editor));
}

TEST(ConsolePanel, SourceFilterToggle) {
    ConsolePanel cp;
    cp.set_source_filter(LogSource::Lua, false);
    EXPECT_FALSE(cp.is_source_shown(LogSource::Lua));
    EXPECT_TRUE(cp.is_source_shown(LogSource::LuaPrint));  // unaffected
    cp.set_source_filter(LogSource::Lua, true);
    EXPECT_TRUE(cp.is_source_shown(LogSource::Lua));
}

TEST(ConsolePanel, SourceCountIncrementsPerInsertion) {
    ConsolePanel cp;
    cp.add_message("a", LogLevel::Info, LogSource::Lua);
    cp.add_message("b", LogLevel::Info, LogSource::Lua);
    cp.add_message("c", LogLevel::Info, LogSource::LuaPrint);

    EXPECT_EQ(cp.source_count(LogSource::Lua),         2u);
    EXPECT_EQ(cp.source_count(LogSource::LuaPrint),    1u);
    EXPECT_EQ(cp.source_count(LogSource::Engine),      0u);
    EXPECT_EQ(cp.source_count(LogSource::ScriptError), 0u);
}

TEST(ConsolePanel, SourceCountIncrementsOnCollapsedRepeat) {
    // Collapse mode folds repeats but the per-source counter tracks
    // *occurrences*, not unique entries (mirrors per-level counters).
    ConsolePanel cp;
    cp.set_collapse_mode(true);
    cp.add_message("dup", LogLevel::Info, LogSource::Lua);
    cp.add_message("dup", LogLevel::Info, LogSource::Lua);
    cp.add_message("dup", LogLevel::Info, LogSource::Lua);

    EXPECT_EQ(cp.message_count(), 1u);
    EXPECT_EQ(cp.messages().front().count, 3u);
    EXPECT_EQ(cp.source_count(LogSource::Lua), 3u);
}

TEST(ConsolePanel, ClearResetsSourceCounts) {
    ConsolePanel cp;
    cp.add_message("a", LogLevel::Info, LogSource::Lua);
    cp.add_message("b", LogLevel::Info, LogSource::ScriptError);
    cp.clear();

    EXPECT_EQ(cp.source_count(LogSource::Lua),         0u);
    EXPECT_EQ(cp.source_count(LogSource::ScriptError), 0u);
}

TEST(ConsolePanel, SourceCountDecrementsOnPrune) {
    ConsolePanel cp;
    cp.set_max_messages(2);
    cp.add_message("1", LogLevel::Info, LogSource::Lua);       // pruned
    cp.add_message("2", LogLevel::Info, LogSource::Lua);
    cp.add_message("3", LogLevel::Info, LogSource::LuaPrint);

    EXPECT_EQ(cp.message_count(), 2u);
    EXPECT_EQ(cp.source_count(LogSource::Lua),      1u);
    EXPECT_EQ(cp.source_count(LogSource::LuaPrint), 1u);
}

TEST(ConsolePanel, CollapseRequiresSameSourceForFold) {
    // Same text + level but different source must NOT fold — sources
    // are part of the dedup identity.  Otherwise a Lua print collision
    // would silently steal the count of an engine log.
    ConsolePanel cp;
    cp.set_collapse_mode(true);
    cp.add_message("ping", LogLevel::Info, LogSource::Engine);
    cp.add_message("ping", LogLevel::Info, LogSource::LuaPrint);

    EXPECT_EQ(cp.message_count(), 2u);
    EXPECT_EQ(cp.source_count(LogSource::Engine),   1u);
    EXPECT_EQ(cp.source_count(LogSource::LuaPrint), 1u);
}

// ── M36: jump-to-source on script-error rows ───────────────────────────────

TEST(ConsolePanel, JumpHandlerDefaultsUnregistered) {
    ConsolePanel cp;
    EXPECT_FALSE(cp.has_jump_handler());
}

TEST(ConsolePanel, JumpRequestNoOpWhenIndexOutOfRange) {
    ConsolePanel cp;
    bool fired = false;
    cp.set_on_jump_to_source([&](const std::string&, u32) { fired = true; });
    EXPECT_FALSE(cp.request_jump_at(0));
    EXPECT_FALSE(cp.request_jump_at(99));
    EXPECT_FALSE(fired);
}

TEST(ConsolePanel, JumpRequestNoOpWhenRowHasNoTarget) {
    // Engine logs and Lua print rows don't carry file/line.
    ConsolePanel cp;
    cp.set_on_jump_to_source([](const std::string&, u32) {});
    cp.add_message("plain log", LogLevel::Info);  // no target
    EXPECT_FALSE(cp.request_jump_at(0));
}

TEST(ConsolePanel, JumpRequestNoOpWhenHandlerUnregistered) {
    ConsolePanel cp;
    cp.add_message("[Script] x.lua:5 — boom", LogLevel::Error,
                   LogSource::ScriptError, "x.lua", 5);
    // Even though the row has a target, no handler ⇒ silent no-op.
    EXPECT_FALSE(cp.request_jump_at(0));
}

TEST(ConsolePanel, JumpRequestFiresHandlerWithFileAndLine) {
    ConsolePanel cp;
    std::string captured_file;
    u32 captured_line = 0;
    cp.set_on_jump_to_source(
        [&](const std::string& f, u32 l) {
            captured_file = f;
            captured_line = l;
        });

    cp.add_message("[Script] scripts/main.lua:42 — bad call", LogLevel::Error,
                   LogSource::ScriptError, "scripts/main.lua", 42);

    EXPECT_TRUE(cp.request_jump_at(0));
    EXPECT_EQ(captured_file, "scripts/main.lua");
    EXPECT_EQ(captured_line, 42u);
}

TEST(ConsolePanel, JumpTargetSurvivesPrune) {
    // The (file,line) pair must survive the same prune the message
    // text survives — otherwise a long Console session would silently
    // strip script-error click targets.
    ConsolePanel cp;
    cp.set_max_messages(2);
    cp.set_on_jump_to_source([](const std::string&, u32) {});
    cp.add_message("filler", LogLevel::Info);  // pruned
    cp.add_message("[Script] a.lua:1 — e", LogLevel::Error,
                   LogSource::ScriptError, "a.lua", 1);
    cp.add_message("[Script] b.lua:7 — e", LogLevel::Error,
                   LogSource::ScriptError, "b.lua", 7);

    ASSERT_EQ(cp.message_count(), 2u);
    EXPECT_EQ(cp.messages()[0].source_file, "a.lua");
    EXPECT_EQ(cp.messages()[0].source_line, 1u);
    EXPECT_EQ(cp.messages()[1].source_file, "b.lua");
    EXPECT_EQ(cp.messages()[1].source_line, 7u);
}

TEST(ConsolePanel, JumpTargetIgnoredWhenLineIsZero) {
    // A ScriptError without a known line (line == 0) is non-clickable
    // even when source_file is set — UX rule: don't open the file at
    // an unknown spot.
    ConsolePanel cp;
    bool fired = false;
    cp.set_on_jump_to_source([&](const std::string&, u32) { fired = true; });
    cp.add_message("[Script] x.lua — no line info", LogLevel::Error,
                   LogSource::ScriptError, "x.lua", 0);
    EXPECT_FALSE(cp.request_jump_at(0));
    EXPECT_FALSE(fired);
}

TEST(ConsolePanel, JumpHandlerReplaceableLatestWins) {
    ConsolePanel cp;
    cp.add_message("[Script] f:1 — e", LogLevel::Error,
                   LogSource::ScriptError, "f", 1);

    int first = 0;
    cp.set_on_jump_to_source([&](const std::string&, u32) { ++first; });

    int second = 0;
    cp.set_on_jump_to_source([&](const std::string&, u32) { ++second; });

    EXPECT_TRUE(cp.request_jump_at(0));
    EXPECT_EQ(first, 0);
    EXPECT_EQ(second, 1);
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

// =============================================================================
// UndoHistoryPanel
// =============================================================================

TEST(UndoHistoryPanel, TypeId) {
    UndoHistoryPanel p;
    EXPECT_STREQ(p.type_id(), "UndoHistoryPanel");
    EXPECT_EQ(p.title(), "Undo History");
}

TEST(UndoHistoryPanel, BindManager) {
    UndoHistoryPanel p;
    UndoRedoManager mgr;
    p.bind_undo_manager(&mgr);
    // No render call — panel is headless in tests (no ImGui context).
    SUCCEED();
}

// =============================================================================
// ProfilerPanel
// =============================================================================

TEST(ProfilerPanel, TypeId) {
    ProfilerPanel p;
    EXPECT_STREQ(p.type_id(), "ProfilerPanel");
    EXPECT_EQ(p.title(), "Profiler");
}

TEST(ProfilerPanel, BindProfiler) {
    ProfilerPanel p;
    nexus::Profiler prof;
    p.bind_profiler(&prof);
    EXPECT_FALSE(p.is_paused());
    p.set_paused(true);
    EXPECT_TRUE(p.is_paused());
}

// =============================================================================
// ConsolePanel — collapse, timestamps, clear-on-play, error-pause
// =============================================================================

TEST(ConsolePanel, AddsMessage) {
    ConsolePanel p;
    p.add_message("hello", LogLevel::Info);
    ASSERT_EQ(p.message_count(), 1u);
    EXPECT_EQ(p.messages()[0].text, "hello");
    EXPECT_EQ(p.messages()[0].count, 1u);
}

TEST(ConsolePanel, SeverityCounts) {
    ConsolePanel p;
    p.add_message("a", LogLevel::Info);
    p.add_message("b", LogLevel::Warning);
    p.add_message("c", LogLevel::Error);
    p.add_message("d", LogLevel::Debug);
    EXPECT_EQ(p.info_count(),    1u);
    EXPECT_EQ(p.warning_count(), 1u);
    EXPECT_EQ(p.error_count(),   1u);
    EXPECT_EQ(p.debug_count(),   1u);
}

TEST(ConsolePanel, CollapseFoldsAdjacentDuplicates) {
    ConsolePanel p;
    p.set_collapse_mode(true);
    p.add_message("repeated", LogLevel::Warning);
    p.add_message("repeated", LogLevel::Warning);
    p.add_message("repeated", LogLevel::Warning);
    ASSERT_EQ(p.message_count(), 1u);
    EXPECT_EQ(p.messages()[0].count, 3u);
    // Severity counter increments per occurrence, not per row.
    EXPECT_EQ(p.warning_count(), 3u);
}

TEST(ConsolePanel, CollapseDoesNotFoldNonAdjacent) {
    ConsolePanel p;
    p.set_collapse_mode(true);
    p.add_message("a", LogLevel::Info);
    p.add_message("b", LogLevel::Info);
    p.add_message("a", LogLevel::Info);
    EXPECT_EQ(p.message_count(), 3u);
}

TEST(ConsolePanel, CollapseOffNeverFolds) {
    ConsolePanel p;
    EXPECT_FALSE(p.collapse_mode());
    p.add_message("dup", LogLevel::Info);
    p.add_message("dup", LogLevel::Info);
    EXPECT_EQ(p.message_count(), 2u);
}

TEST(ConsolePanel, ClearResetsCounters) {
    ConsolePanel p;
    p.add_message("x", LogLevel::Error);
    p.add_message("y", LogLevel::Warning);
    p.clear();
    EXPECT_EQ(p.message_count(), 0u);
    EXPECT_EQ(p.error_count(),   0u);
    EXPECT_EQ(p.warning_count(), 0u);
}

TEST(ConsolePanel, OnEnterPlayClearsWhenEnabled) {
    ConsolePanel p;
    p.set_clear_on_play(true);
    p.add_message("stale", LogLevel::Info);
    p.on_enter_play();
    EXPECT_EQ(p.message_count(), 0u);
}

TEST(ConsolePanel, OnEnterPlayKeepsWhenDisabled) {
    ConsolePanel p;
    p.set_clear_on_play(false);
    p.add_message("stale", LogLevel::Info);
    p.on_enter_play();
    EXPECT_EQ(p.message_count(), 1u);
}

TEST(ConsolePanel, ErrorPauseRequestSetOnce) {
    ConsolePanel p;
    p.set_error_pause(true);
    p.add_message("oops", LogLevel::Error);
    EXPECT_TRUE(p.consume_error_pause_request());
    EXPECT_FALSE(p.consume_error_pause_request());
}

TEST(ConsolePanel, ErrorPauseIgnoredWhenDisabled) {
    ConsolePanel p;
    p.set_error_pause(false);
    p.add_message("oops", LogLevel::Error);
    EXPECT_FALSE(p.consume_error_pause_request());
}

TEST(ConsolePanel, MaxMessagesPrunesOldest) {
    ConsolePanel p;
    p.set_max_messages(3);
    p.add_message("a", LogLevel::Info);
    p.add_message("b", LogLevel::Info);
    p.add_message("c", LogLevel::Info);
    p.add_message("d", LogLevel::Info);
    EXPECT_EQ(p.message_count(), 3u);
    EXPECT_EQ(p.messages()[0].text, "b");
    EXPECT_EQ(p.messages()[2].text, "d");
}

TEST(ConsolePanel, MaxMessagesPrunePreservesSeverityCounts) {
    // When collapsed entries with count>1 are pruned, the severity counter
    // must drop by the merged count, not by 1.
    ConsolePanel p;
    p.set_max_messages(2);
    p.set_collapse_mode(true);
    // Row 1 = "x" warning x3 (one entry, count=3).
    p.add_message("x", LogLevel::Warning);
    p.add_message("x", LogLevel::Warning);
    p.add_message("x", LogLevel::Warning);
    // Row 2 = "y" info.
    p.add_message("y", LogLevel::Info);
    // Row 3 = "z" error — pushes row 1 out, removing 3 warning-counter ticks.
    p.add_message("z", LogLevel::Error);
    EXPECT_EQ(p.message_count(),  2u);
    EXPECT_EQ(p.warning_count(),  0u);
    EXPECT_EQ(p.info_count(),     1u);
    EXPECT_EQ(p.error_count(),    1u);
}

// =============================================================================
// AssetBrowserPanel — root cap + path semantics
// =============================================================================

TEST(AssetBrowserPanel, NavigateUpStopsAtRoot) {
    namespace fs = std::filesystem;
    AssetBrowserPanel ab;
    auto root = fs::temp_directory_path() / "nexus-ab-test";
    fs::create_directories(root / "sub");
    // navigate_to resolves through weakly_canonical, so on macOS the result
    // can read as `/private/var/...` even when the input is `/var/...`.
    // Compare against the same canonical form rather than the literal input.
    std::error_code ec;
    const auto canonical_root = fs::weakly_canonical(root, ec);
    ab.set_root_path(root.string());
    ab.navigate_to((root / "sub").string());
    ab.navigate_up();
    EXPECT_EQ(fs::path(ab.current_path()), canonical_root);
    // Climbing past root must not escape the project.
    ab.navigate_up();
    EXPECT_EQ(fs::path(ab.current_path()), canonical_root);
    fs::remove_all(root);
}

TEST(AssetBrowserPanel, NavigationDirtyFlagFiresOncePerChange) {
    AssetBrowserPanel ab;
    EXPECT_TRUE(ab.consume_navigation_dirty()) << "initial nav is dirty";
    EXPECT_FALSE(ab.consume_navigation_dirty());
    ab.navigate_to("/tmp");
    EXPECT_TRUE(ab.consume_navigation_dirty());
    EXPECT_FALSE(ab.consume_navigation_dirty());
}

TEST(AssetBrowserPanel, RecursiveSearchToggle) {
    AssetBrowserPanel ab;
    EXPECT_FALSE(ab.recursive_search());
    ab.set_recursive_search(true);
    EXPECT_TRUE(ab.recursive_search());
}

// =============================================================================
// HierarchyPanel — selection routing through bound EditorSelection
// =============================================================================

TEST(HierarchyPanelSelection, RoutesThroughBoundSelection) {
    HierarchyPanel hp;
    EditorSelection sel;
    hp.bind_selection(&sel);
    hp.set_selected_entity(7);
    EXPECT_TRUE(sel.is_selected(7));
    EXPECT_EQ(sel.primary(), 7u);
    EXPECT_EQ(hp.selected_entity(), 7u);
    EXPECT_TRUE(hp.has_selection());
}

TEST(HierarchyPanelSelection, ClearReachesBoundSelection) {
    HierarchyPanel hp;
    EditorSelection sel;
    hp.bind_selection(&sel);
    hp.set_selected_entity(3);
    hp.clear_selection();
    EXPECT_FALSE(sel.has_selection());
    EXPECT_FALSE(hp.has_selection());
}

TEST(HierarchyPanelSelection, MultiSelectViaBound) {
    HierarchyPanel hp;
    EditorSelection sel;
    hp.bind_selection(&sel);
    hp.add_to_selection(1);
    hp.add_to_selection(2);
    hp.add_to_selection(3);
    EXPECT_EQ(sel.count(), 3u);
    EXPECT_TRUE(hp.is_multi_selected(2));
    hp.remove_from_selection(2);
    EXPECT_FALSE(hp.is_multi_selected(2));
    EXPECT_EQ(sel.count(), 2u);
}

TEST(HierarchyPanelSelection, FallbackWhenUnbound) {
    HierarchyPanel hp;
    hp.set_selected_entity(42);
    EXPECT_EQ(hp.selected_entity(), 42u);
    EXPECT_TRUE(hp.has_selection());
    hp.clear_selection();
    EXPECT_FALSE(hp.has_selection());
}

TEST(HierarchyPanelSelection, BoundSelectionVisibleToOtherReader) {
    HierarchyPanel hp;
    EditorSelection sel;
    hp.bind_selection(&sel);
    sel.select(99);                          // external mutation
    EXPECT_EQ(hp.selected_entity(), 99u);    // panel sees it
    EXPECT_TRUE(hp.has_selection());
}

TEST(HierarchyPanelRename, RenamingState) {
    HierarchyPanel hp;
    EXPECT_FALSE(hp.is_renaming());
    EXPECT_EQ(hp.renaming_entity(), 0u);
}

TEST(HierarchyPanelAssetDrop, CallbackWiringStoresCallback) {
    HierarchyPanel hp;
    bool called = false;
    std::string saw_path;
    u32 saw_entity = 0;
    hp.set_on_asset_drop(
        [&](const std::string& p, u32 e) {
            called = true;
            saw_path = p;
            saw_entity = e;
        });
    // Direct callback exposure — we don't have an ImGui context to simulate
    // the actual drop, but binding alone exercises the storage path so
    // future render-path changes that drop the callback are caught.
    EXPECT_FALSE(called);
    (void)saw_path;
    (void)saw_entity;
}

// =============================================================================
// InspectorPanel — multi-target broadcast
// =============================================================================

TEST(InspectorPanelMulti, EditTargetsSinglePrimary) {
    InspectorPanel ip;
    ip.set_target_entity(7);
    auto t = ip.current_edit_targets();
    ASSERT_EQ(t.size(), 1u);
    EXPECT_EQ(t[0], 7u);
}

TEST(InspectorPanelMulti, EditTargetsBroadcastsWhenSelectionMatches) {
    InspectorPanel ip;
    EditorSelection sel;
    ip.bind_selection(&sel);
    sel.select(1);
    sel.select(2);
    sel.select(3);
    ip.set_target_entity(2);            // primary among the selection
    auto t = ip.current_edit_targets();
    EXPECT_EQ(t.size(), 3u);
    EXPECT_NE(std::find(t.begin(), t.end(), 1u), t.end());
    EXPECT_NE(std::find(t.begin(), t.end(), 2u), t.end());
    EXPECT_NE(std::find(t.begin(), t.end(), 3u), t.end());
}

TEST(InspectorPanelMulti, EditTargetsFallbackWhenLockedTargetOutsideSelection) {
    InspectorPanel ip;
    EditorSelection sel;
    ip.bind_selection(&sel);
    sel.select(1);
    sel.select(2);
    ip.set_target_entity(99);           // pinned to entity not in selection
    auto t = ip.current_edit_targets();
    EXPECT_EQ(t.size(), 1u);
    EXPECT_EQ(t[0], 99u);
}

TEST(InspectorPanelMulti, EditTargetsEmptyWhenNoTarget) {
    InspectorPanel ip;
    auto t = ip.current_edit_targets();
    EXPECT_TRUE(t.empty());
}

TEST(InspectorPanelMulti, EditTargetsSingleWhenSelectionHasOne) {
    InspectorPanel ip;
    EditorSelection sel;
    ip.bind_selection(&sel);
    sel.select(5);
    ip.set_target_entity(5);
    auto t = ip.current_edit_targets();
    EXPECT_EQ(t.size(), 1u);
    EXPECT_EQ(t[0], 5u);
}

// =============================================================================
// ViewportPanel — asset drop callback wiring
// =============================================================================

TEST(ViewportPanelAssetDrop, CallbackBindable) {
    ViewportPanel vp;
    bool called = false;
    vp.set_on_asset_drop([&](const std::string&) { called = true; });
    EXPECT_FALSE(called);
}

// =============================================================================
// HierarchyPanel — undoable lock/visibility binding
// =============================================================================

TEST(HierarchyPanelUndo, CanBindUndoManager) {
    HierarchyPanel hp;
    UndoRedoManager mgr;
    hp.bind_undo_manager(&mgr);
    EXPECT_FALSE(mgr.can_undo());
}

// =============================================================================
// AssetBrowserPanel — thumbnail registration
// =============================================================================

TEST(AssetBrowserThumbnail, SetAndQuery) {
    AssetBrowserPanel ab;
    EXPECT_EQ(ab.thumbnail("nonexistent"), 0u);
    ab.set_thumbnail("textures/player.png", 0xDEADBEEFull);
    EXPECT_EQ(ab.thumbnail("textures/player.png"), 0xDEADBEEFull);
    EXPECT_EQ(ab.thumbnail_count(), 1u);
}

TEST(AssetBrowserThumbnail, ZeroIdRemovesEntry) {
    AssetBrowserPanel ab;
    ab.set_thumbnail("a.png", 42);
    ab.set_thumbnail("b.png", 99);
    EXPECT_EQ(ab.thumbnail_count(), 2u);
    ab.set_thumbnail("a.png", 0);
    EXPECT_EQ(ab.thumbnail_count(), 1u);
    EXPECT_EQ(ab.thumbnail("a.png"), 0u);
    EXPECT_EQ(ab.thumbnail("b.png"), 99u);
}

TEST(AssetBrowserThumbnail, OverwritesExistingBinding) {
    AssetBrowserPanel ab;
    ab.set_thumbnail("x", 1);
    ab.set_thumbnail("x", 2);
    EXPECT_EQ(ab.thumbnail("x"), 2u);
    EXPECT_EQ(ab.thumbnail_count(), 1u);
}

// =============================================================================
// AssetBrowserPanel — project sidebar (Favorites / virtual collections)
// =============================================================================

TEST(AssetBrowserSidebar, DefaultFilterIsNone) {
    AssetBrowserPanel ab;
    EXPECT_EQ(ab.current_filter(), AssetBrowserPanel::SidebarFilter::None);
}

TEST(AssetBrowserSidebar, SetFilterMarksDirtyAndFiresCallback) {
    AssetBrowserPanel ab;
    int fired = 0;
    AssetBrowserPanel::SidebarFilter received =
        AssetBrowserPanel::SidebarFilter::None;
    ab.set_on_filter_request([&](AssetBrowserPanel::SidebarFilter f) {
        ++fired;
        received = f;
    });
    // Drain the constructor's initial dirty bit so we measure the toggle.
    (void)ab.consume_navigation_dirty();

    ab.set_filter(AssetBrowserPanel::SidebarFilter::AllMaterials);
    EXPECT_EQ(ab.current_filter(),
              AssetBrowserPanel::SidebarFilter::AllMaterials);
    EXPECT_TRUE(ab.consume_navigation_dirty());
    EXPECT_EQ(fired, 1);
    EXPECT_EQ(received, AssetBrowserPanel::SidebarFilter::AllMaterials);
}

TEST(AssetBrowserSidebar, ReclickingActiveFilterRefiresCallback) {
    AssetBrowserPanel ab;
    int fired = 0;
    ab.set_on_filter_request([&](AssetBrowserPanel::SidebarFilter) { ++fired; });
    ab.set_filter(AssetBrowserPanel::SidebarFilter::AllModels);
    EXPECT_EQ(fired, 1);
    // Re-clicking the same active filter is a refresh request — fires again.
    ab.set_filter(AssetBrowserPanel::SidebarFilter::AllModels);
    EXPECT_EQ(fired, 2);
}

TEST(AssetBrowserSidebar, NoneFilterDoesNotFireCallback) {
    AssetBrowserPanel ab;
    int fired = 0;
    ab.set_on_filter_request([&](AssetBrowserPanel::SidebarFilter) { ++fired; });
    ab.set_filter(AssetBrowserPanel::SidebarFilter::AllPrefabs);
    EXPECT_EQ(fired, 1);
    // Switching back to None means "browse current_path_" — host scans via
    // the existing folder populator, no extra callback.
    ab.set_filter(AssetBrowserPanel::SidebarFilter::None);
    EXPECT_EQ(fired, 1);
    EXPECT_EQ(ab.current_filter(), AssetBrowserPanel::SidebarFilter::None);
}

TEST(AssetBrowserSidebar, NavigateToClearsFilter) {
    AssetBrowserPanel ab;
    ab.set_filter(AssetBrowserPanel::SidebarFilter::Favorites);
    EXPECT_EQ(ab.current_filter(),
              AssetBrowserPanel::SidebarFilter::Favorites);
    ab.navigate_to("/tmp");
    // Returning to a folder must clear any virtual collection so the host's
    // next rescan reads from the folder again.
    EXPECT_EQ(ab.current_filter(), AssetBrowserPanel::SidebarFilter::None);
}

TEST(AssetBrowserFavorites, AddRemoveAndQuery) {
    AssetBrowserPanel ab;
    EXPECT_EQ(ab.favorite_count(), 0u);
    EXPECT_FALSE(ab.is_favorite("/proj/Materials/wood.mat"));

    ab.add_favorite("/proj/Materials/wood.mat");
    ab.add_favorite("/proj/Models/cube.obj");
    EXPECT_EQ(ab.favorite_count(), 2u);
    EXPECT_TRUE(ab.is_favorite("/proj/Materials/wood.mat"));
    EXPECT_TRUE(ab.is_favorite("/proj/Models/cube.obj"));

    // Adding a duplicate is a no-op.
    ab.add_favorite("/proj/Materials/wood.mat");
    EXPECT_EQ(ab.favorite_count(), 2u);

    ab.remove_favorite("/proj/Materials/wood.mat");
    EXPECT_EQ(ab.favorite_count(), 1u);
    EXPECT_FALSE(ab.is_favorite("/proj/Materials/wood.mat"));

    // Removing an absent path is also a no-op.
    ab.remove_favorite("/proj/Nonexistent.png");
    EXPECT_EQ(ab.favorite_count(), 1u);
}

// =============================================================================
// InspectorPanel — ComponentRegistry binding
// =============================================================================
//
// The popup itself can't be exercised headlessly (no ImGui context), but the
// API contract — bind / unbind / accessor — is testable and guards against
// regressions where a future refactor drops the slot.

TEST(InspectorPanelAddComponent, RegistryBindIsRoundTrippable) {
    InspectorPanel ip;
    EXPECT_EQ(ip.component_registry(), nullptr);
    ComponentRegistry reg;
    register_builtin_components(reg);
    ip.bind_component_registry(&reg);
    EXPECT_EQ(ip.component_registry(), &reg);
    ip.bind_component_registry(nullptr);
    EXPECT_EQ(ip.component_registry(), nullptr);
}

// =============================================================================
// InspectorPanel — asset path resolvers (M13)
// =============================================================================
//
// The inspector renders raw asset ids (material_id, clip_id, etc.) alongside
// a human-readable filename when the host project has bound a path resolver.
// The popup itself isn't testable headlessly, but the bind/unbind/lookup
// contract is — these tests guard against regressions where a future
// refactor drops the slot or wraps callbacks in a way that would crash on
// a missing resolver.

TEST(InspectorPanelAssetResolvers, DefaultResolversAreEmpty) {
    InspectorPanel ip;
    EXPECT_FALSE(ip.asset_path_resolvers().material);
    EXPECT_FALSE(ip.asset_path_resolvers().animation);
    EXPECT_FALSE(ip.asset_path_resolvers().audio);
    EXPECT_FALSE(ip.asset_path_resolvers().prefab);
}

TEST(InspectorPanelAssetResolvers, BindRoundTripsAndCallbacksFire) {
    InspectorPanel ip;
    InspectorPanel::AssetPathResolvers r;
    r.material  = [](u32 id) { return id == 1u ? std::string{"mat.mat"}     : std::string{}; };
    r.animation = [](u32 id) { return id == 2u ? std::string{"a.anim"}      : std::string{}; };
    r.audio     = [](u32 id) { return id == 3u ? std::string{"clip.wav"}    : std::string{}; };
    r.prefab    = [](u32 id) { return id == 4u ? std::string{"hero.prefab"} : std::string{}; };
    ip.bind_asset_path_resolvers(std::move(r));

    const auto& got = ip.asset_path_resolvers();
    ASSERT_TRUE(got.material);
    ASSERT_TRUE(got.animation);
    ASSERT_TRUE(got.audio);
    ASSERT_TRUE(got.prefab);

    EXPECT_EQ(got.material(1u),  "mat.mat");
    EXPECT_EQ(got.material(99u), "");
    EXPECT_EQ(got.animation(2u), "a.anim");
    EXPECT_EQ(got.audio(3u),     "clip.wav");
    EXPECT_EQ(got.prefab(4u),    "hero.prefab");
}

TEST(InspectorPanelAssetResolvers, BindCanReplaceIndividualResolvers) {
    InspectorPanel ip;
    InspectorPanel::AssetPathResolvers r1;
    r1.material = [](u32) { return std::string{"first"}; };
    ip.bind_asset_path_resolvers(std::move(r1));
    EXPECT_EQ(ip.asset_path_resolvers().material(0u), "first");

    InspectorPanel::AssetPathResolvers r2;
    r2.material = [](u32) { return std::string{"second"}; };
    ip.bind_asset_path_resolvers(std::move(r2));
    EXPECT_EQ(ip.asset_path_resolvers().material(0u), "second");
    // The previous animation slot should be gone — bind replaces wholesale.
    EXPECT_FALSE(ip.asset_path_resolvers().animation);
}

// =============================================================================
// AssetBrowserPanel — visual contract for non-image asset families (M14)
// =============================================================================

TEST(AssetBrowserVisuals, IconForKnownExtensions) {
    EXPECT_STREQ(AssetBrowserPanel::default_icon_for(".mat"),    "[MAT]");
    EXPECT_STREQ(AssetBrowserPanel::default_icon_for(".material"),"[MAT]");
    EXPECT_STREQ(AssetBrowserPanel::default_icon_for(".prefab"), "[PFB]");
    EXPECT_STREQ(AssetBrowserPanel::default_icon_for(".nexusprefab"), "[PFB]");
    EXPECT_STREQ(AssetBrowserPanel::default_icon_for(".anim"),   "[ANIM]");
    EXPECT_STREQ(AssetBrowserPanel::default_icon_for(".png"),    "[IMG]");
    EXPECT_STREQ(AssetBrowserPanel::default_icon_for(".jpeg"),   "[IMG]");
    EXPECT_STREQ(AssetBrowserPanel::default_icon_for(".obj"),    "[MESH]");
    EXPECT_STREQ(AssetBrowserPanel::default_icon_for(".fbx"),    "[MESH]");
    EXPECT_STREQ(AssetBrowserPanel::default_icon_for(".wav"),    "[SFX]");
}

TEST(AssetBrowserVisuals, IconFallbackForUnknown) {
    EXPECT_STREQ(AssetBrowserPanel::default_icon_for(".xyz"), "[F]");
    EXPECT_STREQ(AssetBrowserPanel::default_icon_for(""),     "[F]");
}

TEST(AssetBrowserVisuals, TileColorsAreUniqueAcrossFamilies) {
    // Each major family should map to a distinct color so users can
    // distinguish at a glance.
    const u32 mat    = AssetBrowserPanel::default_tile_color_for(".mat");
    const u32 prefab = AssetBrowserPanel::default_tile_color_for(".prefab");
    const u32 anim   = AssetBrowserPanel::default_tile_color_for(".anim");
    const u32 img    = AssetBrowserPanel::default_tile_color_for(".png");
    const u32 mesh   = AssetBrowserPanel::default_tile_color_for(".obj");
    const u32 audio  = AssetBrowserPanel::default_tile_color_for(".wav");
    const u32 dflt   = AssetBrowserPanel::default_tile_color_for(".xyz");

    EXPECT_NE(mat, prefab);
    EXPECT_NE(prefab, anim);
    EXPECT_NE(anim, img);
    EXPECT_NE(img, mesh);
    EXPECT_NE(mesh, audio);
    EXPECT_NE(audio, dflt);
    EXPECT_NE(mat, dflt);
}

TEST(AssetBrowserVisuals, TileColorAliasesShareSameColor) {
    // .mat and .material map to the same family → same tile color.
    EXPECT_EQ(AssetBrowserPanel::default_tile_color_for(".mat"),
              AssetBrowserPanel::default_tile_color_for(".material"));
    EXPECT_EQ(AssetBrowserPanel::default_tile_color_for(".prefab"),
              AssetBrowserPanel::default_tile_color_for(".nexusprefab"));
    EXPECT_EQ(AssetBrowserPanel::default_tile_color_for(".jpg"),
              AssetBrowserPanel::default_tile_color_for(".jpeg"));
}

// =============================================================================
// AnimationPanel — dopesheet helpers (M15)
// =============================================================================

#include "nexus/animation/animation_clip.h"

TEST(AnimationPanel, TimeToXLinearMapping) {
    // Maps t in [0, duration] linearly onto [view_min, view_max].
    EXPECT_FLOAT_EQ(AnimationPanel::time_to_x(0.0f, 100.0f, 500.0f, 2.0f), 100.0f);
    EXPECT_FLOAT_EQ(AnimationPanel::time_to_x(2.0f, 100.0f, 500.0f, 2.0f), 500.0f);
    EXPECT_FLOAT_EQ(AnimationPanel::time_to_x(1.0f, 100.0f, 500.0f, 2.0f), 300.0f);
}

TEST(AnimationPanel, TimeToXClampsOutOfRange) {
    EXPECT_FLOAT_EQ(AnimationPanel::time_to_x(-1.0f, 100.0f, 500.0f, 2.0f), 100.0f);
    EXPECT_FLOAT_EQ(AnimationPanel::time_to_x(99.0f, 100.0f, 500.0f, 2.0f), 500.0f);
}

TEST(AnimationPanel, TimeToXZeroDurationPinsToStart) {
    EXPECT_FLOAT_EQ(AnimationPanel::time_to_x(1.0f, 100.0f, 500.0f, 0.0f), 100.0f);
}

TEST(AnimationPanel, XToTimeIsInverseOfTimeToX) {
    const f32 t = 0.7f;
    const f32 x = AnimationPanel::time_to_x(t, 100.0f, 500.0f, 2.0f);
    EXPECT_NEAR(AnimationPanel::x_to_time(x, 100.0f, 500.0f, 2.0f), t, 1e-4f);
}

TEST(AnimationPanel, XToTimeClampsAndZeroDurationReturnsZero) {
    EXPECT_FLOAT_EQ(AnimationPanel::x_to_time(50.0f,  100.0f, 500.0f, 2.0f), 0.0f);
    EXPECT_FLOAT_EQ(AnimationPanel::x_to_time(700.0f, 100.0f, 500.0f, 2.0f), 2.0f);
    EXPECT_FLOAT_EQ(AnimationPanel::x_to_time(300.0f, 100.0f, 500.0f, 0.0f), 0.0f);
}

TEST(AnimationPanel, ClipTotalKeysCountsAcrossChannelsAndComponents) {
    nexus::anim::AnimationClip clip("test", 1.0f);
    nexus::anim::BoneChannel ch;
    ch.bone_index = 0;
    ch.positions.push_back({0.0f, Vec3(0)});
    ch.positions.push_back({1.0f, Vec3(1)});
    ch.rotations.push_back({0.5f, Quat(1, 0, 0, 0)});
    clip.add_channel(std::move(ch));

    nexus::anim::BoneChannel ch2;
    ch2.bone_index = 1;
    ch2.scales.push_back({0.0f, Vec3(1)});
    ch2.scales.push_back({1.0f, Vec3(2)});
    clip.add_channel(std::move(ch2));

    EXPECT_EQ(AnimationPanel::clip_total_keys(clip), 5u);
}

TEST(AnimationPanel, EmptyClipHasZeroKeys) {
    nexus::anim::AnimationClip clip("empty", 0.0f);
    EXPECT_EQ(AnimationPanel::clip_total_keys(clip), 0u);
}

TEST(AnimationPanel, PlayheadAndZoomClampToValidRanges) {
    AnimationPanel p;
    EXPECT_FLOAT_EQ(p.playhead(), 0.0f);
    p.set_playhead(-5.0f);
    EXPECT_FLOAT_EQ(p.playhead(), 0.0f);   // clamps to 0
    p.set_playhead(2.5f);
    EXPECT_FLOAT_EQ(p.playhead(), 2.5f);

    EXPECT_FLOAT_EQ(p.zoom(), 120.0f);     // default
    p.set_zoom(0.1f);
    EXPECT_FLOAT_EQ(p.zoom(), 16.0f);      // clamps to min
    p.set_zoom(99999.0f);
    EXPECT_FLOAT_EQ(p.zoom(), 4096.0f);    // clamps to max
    p.set_zoom(256.0f);
    EXPECT_FLOAT_EQ(p.zoom(), 256.0f);
}

TEST(AnimationPanel, ClipIdAndResolverBindings) {
    AnimationPanel p;
    EXPECT_EQ(p.clip_id(), 0u);
    EXPECT_FALSE(p.has_clip_resolver());
    p.set_clip_id(12345);
    EXPECT_EQ(p.clip_id(), 12345u);
    p.set_clip_resolver([](u32) { return nullptr; });
    EXPECT_TRUE(p.has_clip_resolver());
    EXPECT_STREQ(p.type_id(), "AnimationPanel");
}

// =============================================================================
// AnimationPanel — edit-mode bindings (M23)
// =============================================================================

TEST(AnimationPanel, MutableResolverBindIsRoundTrippable) {
    AnimationPanel p;
    EXPECT_FALSE(p.has_mutable_clip_resolver());
    p.set_mutable_clip_resolver(
        [](u32) -> nexus::anim::AnimationClip* { return nullptr; });
    EXPECT_TRUE(p.has_mutable_clip_resolver());
    p.set_mutable_clip_resolver({});
    EXPECT_FALSE(p.has_mutable_clip_resolver());
}

TEST(AnimationPanel, ActiveBoneClampsAndDefaults) {
    AnimationPanel p;
    EXPECT_EQ(p.active_bone(), 0);
    p.set_active_bone(5);
    EXPECT_EQ(p.active_bone(), 5);
    p.set_active_bone(-3);
    EXPECT_EQ(p.active_bone(), 0);  // clamps to 0
}


// =============================================================================
// LuaConsolePanel (M22) — runner binding + history accounting
// =============================================================================

TEST(LuaConsolePanel, DefaultStateIsEmpty) {
    LuaConsolePanel p;
    EXPECT_FALSE(p.has_runner());
    EXPECT_TRUE(p.source().empty());
    EXPECT_TRUE(p.history().empty());
    EXPECT_STREQ(p.type_id(), "LuaConsolePanel");
}

TEST(LuaConsolePanel, RunWithoutRunnerProducesSyntheticError) {
    LuaConsolePanel p;
    auto r = p.run_source("anything");
    EXPECT_FALSE(r.ok);
    EXPECT_FALSE(r.error.empty());
    ASSERT_EQ(p.history().size(), 1u);
    EXPECT_FALSE(p.history().front().ok);
}

TEST(LuaConsolePanel, RunDispatchesToRunnerAndAppendsHistory) {
    LuaConsolePanel p;
    int call_count = 0;
    std::string captured;
    p.set_runner([&](const std::string& src) {
        ++call_count;
        captured = src;
        LuaConsolePanel::RunResult r;
        r.ok = true;
        r.output = "42";
        return r;
    });
    EXPECT_TRUE(p.has_runner());

    p.set_source("return 42");
    auto r = p.run_now();
    EXPECT_TRUE(r.ok);
    EXPECT_EQ(call_count, 1);
    EXPECT_EQ(captured, "return 42");
    ASSERT_EQ(p.history().size(), 1u);
    EXPECT_TRUE(p.history().front().ok);
    EXPECT_NE(p.history().front().text.find("OK"), std::string::npos);
    EXPECT_NE(p.history().front().text.find("42"), std::string::npos);
}

TEST(LuaConsolePanel, RunnerFailureIsLoggedRed) {
    LuaConsolePanel p;
    p.set_runner([](const std::string&) {
        LuaConsolePanel::RunResult r;
        r.ok    = false;
        r.error = "syntax error: line 3";
        return r;
    });
    auto r = p.run_source("foo bar");
    EXPECT_FALSE(r.ok);
    ASSERT_EQ(p.history().size(), 1u);
    EXPECT_FALSE(p.history().front().ok);
    EXPECT_NE(p.history().front().text.find("ERROR"), std::string::npos);
    EXPECT_NE(p.history().front().text.find("syntax error"),
              std::string::npos);
}

TEST(LuaConsolePanel, HistoryCappedAtMaxSize) {
    LuaConsolePanel p;
    p.set_runner([](const std::string&) {
        LuaConsolePanel::RunResult r; r.ok = true; return r;
    });
    // Run kHistoryCap+10 times; the panel should drop the oldest 10.
    for (u32 i = 0; i < LuaConsolePanel::kHistoryCap + 10u; ++i) {
        p.run_source("noop");
    }
    EXPECT_EQ(p.history().size(), LuaConsolePanel::kHistoryCap);
}

TEST(LuaConsolePanel, ClearHistoryWipesEverything) {
    LuaConsolePanel p;
    p.set_runner([](const std::string&) {
        LuaConsolePanel::RunResult r; r.ok = true; return r;
    });
    p.run_source("foo");
    p.run_source("bar");
    EXPECT_EQ(p.history().size(), 2u);
    p.clear_history();
    EXPECT_TRUE(p.history().empty());
}

TEST(LuaConsolePanel, SetSourceTruncatesAtBufferCap) {
    LuaConsolePanel p;
    std::string oversized(LuaConsolePanel::kBufferCap * 2, 'a');
    p.set_source(oversized);
    EXPECT_EQ(p.source().size(), LuaConsolePanel::kBufferCap);
}

// ── External log forwarder (M29) ────────────────────────────────────────────

TEST(LuaConsolePanel, OnLogBindIsRoundTrippable) {
    LuaConsolePanel p;
    EXPECT_FALSE(p.has_on_log());
    p.set_on_log([](bool, const std::string&) {});
    EXPECT_TRUE(p.has_on_log());
    p.set_on_log({});
    EXPECT_FALSE(p.has_on_log());
}

TEST(LuaConsolePanel, OnLogFiresOncePerRunWithCorrectOk) {
    LuaConsolePanel p;
    p.set_runner([](const std::string& src) {
        LuaConsolePanel::RunResult r;
        if (src == "good") {
            r.ok = true;
            r.output = "out-good";
        } else {
            r.ok = false;
            r.error = "err-bad";
        }
        return r;
    });

    std::vector<std::pair<bool, std::string>> seen;
    p.set_on_log([&](bool ok, const std::string& msg) {
        seen.emplace_back(ok, msg);
    });

    p.run_source("good");
    p.run_source("bad");
    ASSERT_EQ(seen.size(), 2u);
    EXPECT_TRUE(seen[0].first);
    EXPECT_NE(seen[0].second.find("out-good"), std::string::npos);
    EXPECT_FALSE(seen[1].first);
    EXPECT_NE(seen[1].second.find("err-bad"), std::string::npos);
}

TEST(LuaConsolePanel, OnLogFiresEvenWhenRunnerIsUnbound) {
    // Synthetic-error path must still notify external sinks so the
    // user sees the missing-runner message in Console even when they
    // never opened the Lua panel.
    LuaConsolePanel p;
    int fired = 0;
    bool last_ok = true;
    p.set_on_log([&](bool ok, const std::string&) {
        ++fired;
        last_ok = ok;
    });
    p.run_source("any");
    EXPECT_EQ(fired, 1);
    EXPECT_FALSE(last_ok);
}

// =============================================================================
// RuntimeStatsPanel (M28) — snapshot + supplier wiring
// =============================================================================

TEST(RuntimeStatsPanel, DefaultSnapshotIsZero) {
    RuntimeStatsPanel p;
    const auto& s = p.snapshot();
    EXPECT_EQ(s.animator_components, 0u);
    EXPECT_EQ(s.animator_playing,    0u);
    EXPECT_EQ(s.particle_emitters,   0u);
    EXPECT_EQ(s.particles_alive,     0u);
    EXPECT_FLOAT_EQ(s.fps,           0.0f);
    EXPECT_FLOAT_EQ(s.frame_ms,      0.0f);
    EXPECT_EQ(s.entity_count,        0u);
    EXPECT_FALSE(p.has_supplier());
    EXPECT_STREQ(p.type_id(), "RuntimeStatsPanel");
}

TEST(RuntimeStatsPanel, SetSnapshotRoundTrips) {
    RuntimeStatsPanel p;
    RuntimeStatsPanel::Snapshot s;
    s.animator_components = 5;
    s.animator_playing    = 3;
    s.particle_emitters   = 2;
    s.particles_alive     = 200;
    s.fps                 = 60.0f;
    s.frame_ms            = 16.67f;
    s.entity_count        = 42;
    p.set_snapshot(s);

    const auto& got = p.snapshot();
    EXPECT_EQ(got.animator_components, 5u);
    EXPECT_EQ(got.animator_playing,    3u);
    EXPECT_EQ(got.particle_emitters,   2u);
    EXPECT_EQ(got.particles_alive,     200u);
    EXPECT_FLOAT_EQ(got.fps,           60.0f);
    EXPECT_NEAR(got.frame_ms,         16.67f, 1e-3f);
    EXPECT_EQ(got.entity_count,        42u);
}

TEST(RuntimeStatsPanel, SupplierBindIsRoundTrippable) {
    RuntimeStatsPanel p;
    EXPECT_FALSE(p.has_supplier());
    p.set_supplier([] {
        RuntimeStatsPanel::Snapshot s;
        s.fps = 99.0f;
        return s;
    });
    EXPECT_TRUE(p.has_supplier());
    p.set_supplier({});
    EXPECT_FALSE(p.has_supplier());
}

TEST(AnimationPanel, KeySelectionRoundTripsAndClearable) {
    AnimationPanel p;
    EXPECT_EQ(p.selected_key_kind(), AnimationPanel::KeyKind::None);
    EXPECT_EQ(p.selected_key_index(), -1);

    p.select_key(AnimationPanel::KeyKind::Position, 2);
    EXPECT_EQ(p.selected_key_kind(),  AnimationPanel::KeyKind::Position);
    EXPECT_EQ(p.selected_key_index(), 2);

    p.select_key(AnimationPanel::KeyKind::Rotation, 7);
    EXPECT_EQ(p.selected_key_kind(),  AnimationPanel::KeyKind::Rotation);
    EXPECT_EQ(p.selected_key_index(), 7);

    p.clear_key_selection();
    EXPECT_EQ(p.selected_key_kind(),  AnimationPanel::KeyKind::None);
    EXPECT_EQ(p.selected_key_index(), -1);
}

TEST(AssetBrowserVisuals, MaterialColorSamplerBindIsIntrospectable) {
    AssetBrowserPanel ab;
    EXPECT_FALSE(ab.has_material_color_sampler());
    bool fired = false;
    ab.set_material_color_sampler(
        [&fired](const std::string& path, f32 rgba[4]) {
            fired = true;
            (void)path;
            rgba[0] = 0.1f; rgba[1] = 0.2f; rgba[2] = 0.3f; rgba[3] = 1.0f;
            return true;
        });
    EXPECT_TRUE(ab.has_material_color_sampler());
    // Clearing via empty std::function is fine.
    ab.set_material_color_sampler({});
    EXPECT_FALSE(ab.has_material_color_sampler());
    (void)fired;  // sampler is invoked from on_render which we can't run here
}

TEST(InspectorPanelAssetResolvers, EmptyBindClearsAllSlots) {
    InspectorPanel ip;
    InspectorPanel::AssetPathResolvers r;
    r.material = [](u32) { return std::string{"x"}; };
    ip.bind_asset_path_resolvers(std::move(r));
    EXPECT_TRUE(ip.asset_path_resolvers().material);

    ip.bind_asset_path_resolvers({});
    EXPECT_FALSE(ip.asset_path_resolvers().material);
    EXPECT_FALSE(ip.asset_path_resolvers().animation);
    EXPECT_FALSE(ip.asset_path_resolvers().audio);
    EXPECT_FALSE(ip.asset_path_resolvers().prefab);
}

// =============================================================================
// ViewportPanel — Game view toolbar state + letterbox math
// =============================================================================

TEST(ViewportPanelGameView, AspectRatioValueMapping) {
    EXPECT_FLOAT_EQ(ViewportPanel::aspect_ratio_value(GameAspect::Free),  0.0f);
    EXPECT_NEAR(ViewportPanel::aspect_ratio_value(GameAspect::R16x9),
                16.0f / 9.0f, 1e-6f);
    EXPECT_NEAR(ViewportPanel::aspect_ratio_value(GameAspect::R16x10),
                16.0f / 10.0f, 1e-6f);
    EXPECT_NEAR(ViewportPanel::aspect_ratio_value(GameAspect::R4x3),
                4.0f / 3.0f, 1e-6f);
    EXPECT_FLOAT_EQ(ViewportPanel::aspect_ratio_value(GameAspect::R1x1), 1.0f);
}

TEST(ViewportPanelGameView, LetterboxFreeReturnsAvailableSize) {
    f32 w = 0, h = 0;
    ViewportPanel::compute_letterbox(800.0f, 600.0f, GameAspect::Free, 1.0f, w, h);
    EXPECT_FLOAT_EQ(w, 800.0f);
    EXPECT_FLOAT_EQ(h, 600.0f);
}

TEST(ViewportPanelGameView, LetterboxWideAvail16x9FitsHeight) {
    // Available 1920x600, 16:9 — width 1066.66 fits because width is the
    // limiting axis (avail_w/avail_h = 3.2 > 1.78).
    f32 w = 0, h = 0;
    ViewportPanel::compute_letterbox(1920.0f, 600.0f, GameAspect::R16x9,
                                     1.0f, w, h);
    EXPECT_NEAR(w, 600.0f * 16.0f / 9.0f, 0.5f);
    EXPECT_FLOAT_EQ(h, 600.0f);
}

TEST(ViewportPanelGameView, LetterboxTallAvail16x9FitsWidth) {
    f32 w = 0, h = 0;
    ViewportPanel::compute_letterbox(640.0f, 1280.0f, GameAspect::R16x9,
                                     1.0f, w, h);
    EXPECT_FLOAT_EQ(w, 640.0f);
    EXPECT_NEAR(h, 640.0f * 9.0f / 16.0f, 0.5f);
}

TEST(ViewportPanelGameView, LetterboxScaleClampedToAvailable) {
    // scale 4x must NOT exceed the dock area — the rendered image may zoom
    // but the visible region is bounded by avail_w/avail_h.
    f32 w = 0, h = 0;
    ViewportPanel::compute_letterbox(800.0f, 600.0f, GameAspect::R16x9,
                                     4.0f, w, h);
    EXPECT_LE(w, 800.0f + 0.5f);
    EXPECT_LE(h, 600.0f + 0.5f);
}

TEST(ViewportPanelGameView, LetterboxZeroAvailableYieldsZero) {
    f32 w = 1, h = 1;
    ViewportPanel::compute_letterbox(0.0f, 600.0f, GameAspect::R16x9,
                                     1.0f, w, h);
    EXPECT_FLOAT_EQ(w, 0.0f);
    EXPECT_FLOAT_EQ(h, 0.0f);
}

TEST(ViewportPanelGameView, ScaleClampedTo1To5Range) {
    ViewportPanel vp("Game", ViewportCameraMode::GameView);
    vp.set_game_scale(0.1f);   // below min
    EXPECT_FLOAT_EQ(vp.game_scale(), 1.0f);
    vp.set_game_scale(99.0f);  // above max
    EXPECT_FLOAT_EQ(vp.game_scale(), 5.0f);
    vp.set_game_scale(2.5f);
    EXPECT_FLOAT_EQ(vp.game_scale(), 2.5f);
}

TEST(ViewportPanelGameView, DisplayIndexClampedTo0To3) {
    ViewportPanel vp("Game", ViewportCameraMode::GameView);
    EXPECT_EQ(vp.display_index(), 0u);
    vp.set_display_index(7);
    EXPECT_EQ(vp.display_index(), 3u);
    vp.set_display_index(2);
    EXPECT_EQ(vp.display_index(), 2u);
}

TEST(ViewportPanelGameView, GameAspectAndStatsToggleAreSettable) {
    ViewportPanel vp("Game", ViewportCameraMode::GameView);
    EXPECT_EQ(vp.game_aspect(), GameAspect::Free);
    EXPECT_FALSE(vp.show_stats());
    EXPECT_FALSE(vp.show_game_gizmos());

    vp.set_game_aspect(GameAspect::R16x9);
    vp.set_show_stats(true);
    vp.set_show_game_gizmos(true);
    EXPECT_EQ(vp.game_aspect(), GameAspect::R16x9);
    EXPECT_TRUE(vp.show_stats());
    EXPECT_TRUE(vp.show_game_gizmos());
}

// =============================================================================
// ViewportPanel — SceneView toolbar (shading / 2D / gizmos / camera presets)
// =============================================================================

TEST(ViewportPanelSceneView, ShadingModeIsRoundTrippable) {
    ViewportPanel vp("Scene", ViewportCameraMode::SceneView);
    EXPECT_EQ(vp.shading_mode(), SceneShadingMode::Shaded);
    vp.set_shading_mode(SceneShadingMode::Wireframe);
    EXPECT_EQ(vp.shading_mode(), SceneShadingMode::Wireframe);
    vp.set_shading_mode(SceneShadingMode::ShadedWireframe);
    EXPECT_EQ(vp.shading_mode(), SceneShadingMode::ShadedWireframe);
}

TEST(ViewportPanelSceneView, View2DAndSceneGizmosAreRoundTrippable) {
    ViewportPanel vp("Scene", ViewportCameraMode::SceneView);
    EXPECT_FALSE(vp.view_2d());
    EXPECT_TRUE(vp.show_scene_gizmos());
    vp.set_view_2d(true);
    vp.set_show_scene_gizmos(false);
    EXPECT_TRUE(vp.view_2d());
    EXPECT_FALSE(vp.show_scene_gizmos());
}

TEST(ViewportPanelSceneView, PresetToYawPitchTopLooksDown) {
    f32 y = 0, p = 0;
    ASSERT_TRUE(ViewportPanel::preset_to_yaw_pitch(SceneCameraPreset::Top, y, p));
    EXPECT_LT(p, -85.0f);  // looking nearly straight down
    EXPECT_GT(p, -90.5f);  // but never exactly -90 to avoid gimbal lock
}

TEST(ViewportPanelSceneView, PresetToYawPitchFrontIsZeroPitch) {
    f32 y = 1, p = 1;
    ASSERT_TRUE(ViewportPanel::preset_to_yaw_pitch(SceneCameraPreset::Front, y, p));
    EXPECT_NEAR(p, 0.0f, 1e-3f);
}

TEST(ViewportPanelSceneView, PresetToYawPitchFreeReturnsFalse) {
    f32 y = 99, p = 99;
    EXPECT_FALSE(ViewportPanel::preset_to_yaw_pitch(SceneCameraPreset::Free, y, p));
    // Sentinel values must remain unchanged when the function returns false.
    EXPECT_FLOAT_EQ(y, 99.0f);
    EXPECT_FLOAT_EQ(p, 99.0f);
}

TEST(ViewportPanelSceneView, ApplyCameraPresetSnapsAngles) {
    ViewportPanel vp("Scene", ViewportCameraMode::SceneView);
    vp.editor_cam_yaw_deg()   = 200.0f;
    vp.editor_cam_pitch_deg() = -45.0f;
    vp.apply_camera_preset(SceneCameraPreset::Iso);
    EXPECT_NEAR(vp.editor_cam_yaw_deg(),    45.0f, 1e-3f);
    EXPECT_NEAR(vp.editor_cam_pitch_deg(), -25.0f, 1e-3f);
}

TEST(ViewportPanelSceneView, ApplyCameraPresetFreeIsNoop) {
    ViewportPanel vp("Scene", ViewportCameraMode::SceneView);
    vp.editor_cam_yaw_deg()   = 12.5f;
    vp.editor_cam_pitch_deg() = -7.5f;
    vp.apply_camera_preset(SceneCameraPreset::Free);
    EXPECT_FLOAT_EQ(vp.editor_cam_yaw_deg(),   12.5f);
    EXPECT_FLOAT_EQ(vp.editor_cam_pitch_deg(), -7.5f);
}

TEST(ViewportPanelSceneView, AllPresetsExceptFreeReturnTrue) {
    f32 y = 0, p = 0;
    EXPECT_TRUE(ViewportPanel::preset_to_yaw_pitch(SceneCameraPreset::Top,    y, p));
    EXPECT_TRUE(ViewportPanel::preset_to_yaw_pitch(SceneCameraPreset::Bottom, y, p));
    EXPECT_TRUE(ViewportPanel::preset_to_yaw_pitch(SceneCameraPreset::Front,  y, p));
    EXPECT_TRUE(ViewportPanel::preset_to_yaw_pitch(SceneCameraPreset::Back,   y, p));
    EXPECT_TRUE(ViewportPanel::preset_to_yaw_pitch(SceneCameraPreset::Left,   y, p));
    EXPECT_TRUE(ViewportPanel::preset_to_yaw_pitch(SceneCameraPreset::Right,  y, p));
    EXPECT_TRUE(ViewportPanel::preset_to_yaw_pitch(SceneCameraPreset::Iso,    y, p));
}

TEST(ViewportPanelGameView, RenderStatsRoundTripThroughAccessor) {
    ViewportPanel vp("Game", ViewportCameraMode::GameView);
    EXPECT_EQ(vp.last_render_stats().draw_calls, 0u);

    ViewportRenderStats s;
    s.draw_calls = 12;
    s.mesh_draw_calls = 8;
    s.sprite_draw_calls = 4;
    s.triangles = 4096;
    s.vertices  = 8192;
    s.set_pass_calls = 3;
    vp.set_last_render_stats(s);
    const auto& got = vp.last_render_stats();
    EXPECT_EQ(got.draw_calls, 12u);
    EXPECT_EQ(got.mesh_draw_calls, 8u);
    EXPECT_EQ(got.sprite_draw_calls, 4u);
    EXPECT_EQ(got.triangles, 4096u);
    EXPECT_EQ(got.vertices, 8192u);
    EXPECT_EQ(got.set_pass_calls, 3u);
}
