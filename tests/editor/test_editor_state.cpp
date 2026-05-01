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
    EXPECT_EQ(state.pending_steps(), 1u);
}

TEST(EditorState, StepFromPaused) {
    EditorState state;
    state.play();
    state.pause();
    state.step();
    state.step();
    EXPECT_EQ(state.pending_steps(), 2u);
}

// =============================================================================
// EditorState — SceneBridge
// =============================================================================

namespace {
struct BridgeFixture {
    EditorState state;
    int snapshot_calls = 0;
    int restore_calls = 0;
    int clear_calls = 0;
    int simulate_calls = 0;
    int play_hooks = 0;
    int stop_hooks = 0;
    f32 last_dt = 0.0f;

    BridgeFixture() {
        SceneBridge b;
        b.take_snapshot    = [this]() { ++snapshot_calls; return true; };
        b.restore_snapshot = [this]() { ++restore_calls;  return true; };
        b.clear_snapshot   = [this]() { ++clear_calls;    };
        b.simulate         = [this](f32 dt) { ++simulate_calls; last_dt = dt; };
        b.on_play          = [this]() { ++play_hooks; };
        b.on_stop          = [this]() { ++stop_hooks; };
        state.set_scene_bridge(std::move(b));
    }
};
}

TEST(EditorStateBridge, PlayCallsTakeSnapshotAndOnPlay) {
    BridgeFixture fx;
    fx.state.play();
    EXPECT_EQ(fx.snapshot_calls, 1);
    EXPECT_EQ(fx.play_hooks, 1);
    EXPECT_TRUE(fx.state.is_playing());
}

TEST(EditorStateBridge, StopCallsRestoreThenClear) {
    BridgeFixture fx;
    fx.state.play();
    fx.state.stop();
    EXPECT_EQ(fx.restore_calls, 1);
    EXPECT_EQ(fx.clear_calls, 1);
    EXPECT_EQ(fx.stop_hooks, 1);
    EXPECT_TRUE(fx.state.is_editing());
}

TEST(EditorStateBridge, StepFromEditingTakesSnapshotOncePauses) {
    BridgeFixture fx;
    fx.state.step();
    EXPECT_EQ(fx.snapshot_calls, 1);   // <-- this was the data-loss bug
    EXPECT_TRUE(fx.state.is_paused());
    EXPECT_EQ(fx.state.pending_steps(), 1u);
}

TEST(EditorStateBridge, AdvanceWhilePlayingUsesFixedDt) {
    BridgeFixture fx;
    fx.state.set_fixed_dt(1.0f / 50.0f);
    fx.state.play();
    EXPECT_TRUE(fx.state.advance_simulation(0.123f));
    EXPECT_EQ(fx.simulate_calls, 1);
    EXPECT_FLOAT_EQ(fx.last_dt, 1.0f / 50.0f);
}

TEST(EditorStateBridge, AdvanceWhilePausedDrainsSteps) {
    BridgeFixture fx;
    fx.state.play();
    fx.state.pause();
    fx.state.step();
    fx.state.step();
    fx.state.step();
    EXPECT_TRUE(fx.state.advance_simulation(0.0f));
    EXPECT_EQ(fx.simulate_calls, 3);
    EXPECT_EQ(fx.state.pending_steps(), 0u);
    // Subsequent advance with no pending steps does nothing.
    EXPECT_FALSE(fx.state.advance_simulation(0.016f));
    EXPECT_EQ(fx.simulate_calls, 3);
}

TEST(EditorStateBridge, AdvanceWhileEditingIsNoop) {
    BridgeFixture fx;
    EXPECT_FALSE(fx.state.advance_simulation(0.016f));
    EXPECT_EQ(fx.simulate_calls, 0);
}

TEST(EditorStateBridge, StopRestoresSelectionFromSnapshot) {
    BridgeFixture fx;
    fx.state.selection().select(7);
    fx.state.selection().select(13);
    fx.state.play();
    fx.state.selection().clear();   // simulate Play-mode wiping selection
    fx.state.stop();
    EXPECT_TRUE(fx.state.selection().is_selected(7));
    EXPECT_TRUE(fx.state.selection().is_selected(13));
}

TEST(EditorStateBridge, RealDtWhenFixedDtZero) {
    BridgeFixture fx;
    fx.state.set_fixed_dt(0.0f);
    fx.state.play();
    fx.state.advance_simulation(0.04f);
    EXPECT_FLOAT_EQ(fx.last_dt, 0.04f);
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
