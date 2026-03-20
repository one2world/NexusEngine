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
// EditorState — central editor state management
// ─────────────────────────────────────────────────────────────────────────────

class EditorState {
public:
    EditorState();

    // ── Play controls ──────────────────────────────────────────────────

    PlayState play_state() const { return play_state_; }

    /// Enter play mode (saves scene snapshot).
    void play();

    /// Pause the running game.
    void pause();

    /// Resume from pause.
    void resume();

    /// Stop play mode (restores scene snapshot).
    void stop();

    /// Step one frame while paused.
    void step();

    bool is_playing() const { return play_state_ == PlayState::Playing; }
    bool is_paused() const { return play_state_ == PlayState::Paused; }
    bool is_editing() const { return play_state_ == PlayState::Editing; }

    /// Number of step requests (consumed by the runtime loop).
    u32 consume_step_requests();

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
    std::string scene_path_;
    SceneSnapshot snapshot_;
    EditorSelection selection_;
    UndoRedoManager undo_redo_;
    PanelManager panels_;
    std::string status_message_;
    f32 editor_time_{0.0f};
};

} // namespace nexus::editor
