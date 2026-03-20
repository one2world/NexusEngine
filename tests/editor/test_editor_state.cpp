#include <gtest/gtest.h>
#include "nexus/editor/editor_state.h"

using namespace nexus;
using namespace nexus::editor;

// =============================================================================
// EditorSelection
// =============================================================================

TEST(EditorSelection, SelectDeselect) {
    EditorSelection sel;
    EXPECT_FALSE(sel.has_selection());

    sel.select(1);
    EXPECT_TRUE(sel.has_selection());
    EXPECT_TRUE(sel.is_selected(1));
    EXPECT_EQ(sel.count(), 1u);

    sel.deselect(1);
    EXPECT_FALSE(sel.has_selection());
}

TEST(EditorSelection, MultiSelect) {
    EditorSelection sel;
    sel.select(1);
    sel.select(2);
    sel.select(3);
    EXPECT_EQ(sel.count(), 3u);
    EXPECT_EQ(sel.primary(), 3u); // Last selected
}

TEST(EditorSelection, DuplicateSelect) {
    EditorSelection sel;
    sel.select(1);
    sel.select(1);
    EXPECT_EQ(sel.count(), 1u);
}

TEST(EditorSelection, Clear) {
    EditorSelection sel;
    sel.select(1);
    sel.select(2);
    sel.clear();
    EXPECT_FALSE(sel.has_selection());
    EXPECT_EQ(sel.count(), 0u);
}

TEST(EditorSelection, Toggle) {
    EditorSelection sel;
    sel.toggle(1);
    EXPECT_TRUE(sel.is_selected(1));
    sel.toggle(1);
    EXPECT_FALSE(sel.is_selected(1));
}

TEST(EditorSelection, Callback) {
    EditorSelection sel;
    int callback_count = 0;
    sel.set_on_change([&](const std::vector<u32>&) { callback_count++; });

    sel.select(1);
    EXPECT_EQ(callback_count, 1);
    sel.deselect(1);
    EXPECT_EQ(callback_count, 2);
    sel.clear(); // No change because already empty
    EXPECT_EQ(callback_count, 2);
}

TEST(EditorSelection, PrimaryEmpty) {
    EditorSelection sel;
    EXPECT_EQ(sel.primary(), 0u);
}

TEST(EditorSelection, AllSelected) {
    EditorSelection sel;
    sel.select(10);
    sel.select(20);
    auto all = sel.all();
    EXPECT_EQ(all.size(), 2u);
}

// =============================================================================
// EditorState — Play Controls
// =============================================================================

TEST(EditorState, InitialState) {
    EditorState state;
    EXPECT_TRUE(state.is_editing());
    EXPECT_FALSE(state.is_playing());
    EXPECT_FALSE(state.is_paused());
}

TEST(EditorState, Play) {
    EditorState state;
    state.play();
    EXPECT_TRUE(state.is_playing());
    EXPECT_FALSE(state.is_editing());
}

TEST(EditorState, Pause) {
    EditorState state;
    state.play();
    state.pause();
    EXPECT_TRUE(state.is_paused());
    EXPECT_FALSE(state.is_playing());
}

TEST(EditorState, Resume) {
    EditorState state;
    state.play();
    state.pause();
    state.resume();
    EXPECT_TRUE(state.is_playing());
}

TEST(EditorState, Stop) {
    EditorState state;
    state.play();
    state.stop();
    EXPECT_TRUE(state.is_editing());
}

TEST(EditorState, StopRestoresSelection) {
    EditorState state;
    state.selection().select(42);
    state.play();
    state.selection().clear();
    state.stop();
    EXPECT_TRUE(state.selection().is_selected(42));
}

TEST(EditorState, StepFromEditing) {
    EditorState state;
    state.step();
    EXPECT_TRUE(state.is_paused()); // Should enter play then pause
    EXPECT_EQ(state.consume_step_requests(), 1u);
}

TEST(EditorState, StepFromPaused) {
    EditorState state;
    state.play();
    state.pause();
    state.step();
    state.step();
    EXPECT_EQ(state.consume_step_requests(), 2u);
    EXPECT_EQ(state.consume_step_requests(), 0u); // Consumed
}

TEST(EditorState, PlayWhilePlaying) {
    EditorState state;
    state.play();
    state.play(); // Should be no-op
    EXPECT_TRUE(state.is_playing());
}

TEST(EditorState, PauseWhileEditing) {
    EditorState state;
    state.pause(); // Should be no-op
    EXPECT_TRUE(state.is_editing());
}

TEST(EditorState, StopWhileEditing) {
    EditorState state;
    state.stop(); // Should be no-op
    EXPECT_TRUE(state.is_editing());
}

// =============================================================================
// EditorState — Other
// =============================================================================

TEST(EditorState, ScenePath) {
    EditorState state;
    state.set_scene_path("scenes/level1.nxscene");
    EXPECT_EQ(state.scene_path(), "scenes/level1.nxscene");
}

TEST(EditorState, Status) {
    EditorState state;
    state.set_status("Ready");
    EXPECT_EQ(state.status(), "Ready");
}

TEST(EditorState, Tick) {
    EditorState state;
    state.tick(0.016f);
    EXPECT_NEAR(state.editor_time(), 0.016f, 0.001f);
    state.tick(0.016f);
    EXPECT_NEAR(state.editor_time(), 0.032f, 0.001f);
}

TEST(EditorState, UndoRedoAccess) {
    EditorState state;
    int val = 0;
    state.undo_redo().execute(std::make_unique<LambdaCommand>("test",
        [&](){ val = 1; }, [&](){ val = 0; }));
    EXPECT_EQ(val, 1);
    EXPECT_TRUE(state.undo_redo().can_undo());
}

TEST(EditorState, PanelAccess) {
    EditorState state;
    EXPECT_EQ(state.panels().count(), 0u);
}

TEST(EditorState, SceneSnapshot) {
    EditorState state;
    std::vector<u8> data = {1, 2, 3};
    state.set_scene_data(data);
    EXPECT_TRUE(state.snapshot().valid);
    EXPECT_EQ(state.snapshot().serialized_scene, data);
}
