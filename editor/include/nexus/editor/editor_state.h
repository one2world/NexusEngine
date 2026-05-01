#pragma once

#include "nexus/core/types.h"
#include "nexus/editor/undo_redo.h"
#include "nexus/editor/panel.h"
#include <string>
#include <vector>
#include <functional>

namespace nexus::editor {

// ─────────────────────────────────────────────────────────────────────────────
// PlayState — editor play/pause/step controls
// ─────────────────────────────────────────────────────────────────────────────

enum class PlayState : u8 {
    Editing,    // Normal editor mode
    Playing,    // Game is running
    Paused,     // Game is paused (can step)
};

// ─────────────────────────────────────────────────────────────────────────────
// EditorSelection — centralized selection state
// ─────────────────────────────────────────────────────────────────────────────

class EditorSelection {
public:
    EditorSelection() = default;

    void select(u32 entity);
    void deselect(u32 entity);
    void clear();
    void toggle(u32 entity);

    bool is_selected(u32 entity) const;
    bool has_selection() const { return !selected_.empty(); }
    u32 count() const { return static_cast<u32>(selected_.size()); }
    u32 primary() const { return selected_.empty() ? 0 : selected_.back(); }
    const std::vector<u32>& all() const { return selected_; }

    using Callback = std::function<void(const std::vector<u32>&)>;
    void set_on_change(Callback cb) { on_change_ = std::move(cb); }

private:
    void notify();

    std::vector<u32> selected_;
    Callback on_change_;
};

// ─────────────────────────────────────────────────────────────────────────────
// SceneSnapshot — captured editor state before play mode
// ─────────────────────────────────────────────────────────────────────────────

struct SceneSnapshot {
    std::vector<u8> serialized_scene;
    std::vector<u32> selected_entities;
    bool valid{false};
};

// ─────────────────────────────────────────────────────────────────────────────
// SceneBridge — inversion-of-control callbacks so EditorState owns the full
// play/step lifecycle without depending on Scene or SceneSerializer directly.
// The host wires these once at startup, and EditorState uses them to snapshot,
// restore, and simulate — eliminating the "external snapshot + internal state"
// two-sources-of-truth bug that made Step lose data when fired from Editing.
// ─────────────────────────────────────────────────────────────────────────────

struct SceneBridge {
    /// Capture a full scene snapshot.  Return true on success.
    std::function<bool()> take_snapshot;
    /// Restore the most recent snapshot.  Return true on success.
    std::function<bool()> restore_snapshot;
    /// Drop the current snapshot (freeing memory, no restore after this).
    std::function<void()> clear_snapshot;
    /// Run one simulation step with the given dt.
    std::function<void(f32)> simulate;
    /// Emitted when the editor transitions into Play.  Hosts use this to e.g.
    /// clear the console when the user entered play and "Clear on Play" is on.
    std::function<void()> on_play;
    /// Emitted when the editor leaves Play and is back in Editing.
    std::function<void()> on_stop;
};

// ─────────────────────────────────────────────────────────────────────────────
// EditorState — central editor state management
// ─────────────────────────────────────────────────────────────────────────────

class EditorState {
public:
    EditorState();

    // ── Play controls ──────────────────────────────────────────────────

    PlayState play_state() const { return play_state_; }

    /// Host wires scene snapshot/simulate/lifecycle hooks here.  All four are
    /// optional, but omitting `take_snapshot`/`restore_snapshot` turns Play
    /// into a destructive operation and is expected only in tests.
    void set_scene_bridge(SceneBridge bridge) { bridge_ = std::move(bridge); }

    /// Enter play mode (takes scene snapshot via bridge).  Safe from any
    /// state: if we're already Playing or Paused it's a no-op.
    void play();

    /// Pause the running game.
    void pause();

    /// Resume from pause.
    void resume();

    /// Stop play mode (restores scene snapshot via bridge).
    void stop();

    /// Step one simulation frame.  If currently Editing, enters Play-Paused
    /// (with snapshot) then steps once.  If currently Playing, pauses and
    /// queues one step.  If Paused, queues one step.
    void step();

    /// Advance the simulation.  Call once per frame from the host loop after
    /// tick().  Internally handles Play (continuous), Paused (no sim),
    /// Paused-with-pending-steps (fixed-dt simulation for each queued step).
    /// Returns true if the scene was simulated this frame.
    bool advance_simulation(f32 real_dt);

    /// Fixed simulation dt used for step() and configurable for physics-bound
    /// play modes.  Unity's default is 1/50 for physics; we use 1/60 to match
    /// common display refresh.  Set to 0 to use real-time dt during Playing.
    f32 fixed_dt() const { return fixed_dt_; }
    void set_fixed_dt(f32 dt) { fixed_dt_ = dt; }

    bool is_playing() const { return play_state_ == PlayState::Playing; }
    bool is_paused() const { return play_state_ == PlayState::Paused; }
    bool is_editing() const { return play_state_ == PlayState::Editing; }

    /// Pending step count — read by tests and the status bar; users
    /// generally don't need to call this directly (advance_simulation does).
    u32 pending_steps() const { return step_requests_; }

    // ── Scene management ───────────────────────────────────────────────

    void set_scene_path(const std::string& path) { scene_path_ = path; }
    const std::string& scene_path() const { return scene_path_; }

    void set_scene_data(const std::vector<u8>& data) { snapshot_.serialized_scene = data; snapshot_.valid = true; }
    const SceneSnapshot& snapshot() const { return snapshot_; }

    // ── Subsystems ─────────────────────────────────────────────────────

    EditorSelection& selection() { return selection_; }
    const EditorSelection& selection() const { return selection_; }

    UndoRedoManager& undo_redo() { return undo_redo_; }
    const UndoRedoManager& undo_redo() const { return undo_redo_; }

    PanelManager& panels() { return panels_; }
    const PanelManager& panels() const { return panels_; }

    // ── Status ─────────────────────────────────────────────────────────

    void set_status(const std::string& msg) { status_message_ = msg; }
    const std::string& status() const { return status_message_; }

    f32 editor_time() const { return editor_time_; }
    void tick(f32 dt);

private:
    PlayState play_state_{PlayState::Editing};
    u32 step_requests_{0};
    f32 fixed_dt_{1.0f / 60.0f};
    SceneBridge bridge_;
    std::string scene_path_;
    SceneSnapshot snapshot_;
    EditorSelection selection_;
    UndoRedoManager undo_redo_;
    PanelManager panels_;
    std::string status_message_;
    f32 editor_time_{0.0f};
};

} // namespace nexus::editor
