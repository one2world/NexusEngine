#include "nexus/editor/editor_state.h"
#include "nexus/core/log.h"
#include <algorithm>

namespace nexus::editor {

// ── EditorSelection ─────────────────────────────────────────────────────────

void EditorSelection::select(u32 entity) {
    if (!is_selected(entity)) {
        selected_.push_back(entity);
        notify();
    }
}

void EditorSelection::deselect(u32 entity) {
    auto it = std::find(selected_.begin(), selected_.end(), entity);
    if (it != selected_.end()) {
        selected_.erase(it);
        notify();
    }
}

void EditorSelection::clear() {
    if (!selected_.empty()) {
        selected_.clear();
        notify();
    }
}

void EditorSelection::toggle(u32 entity) {
    if (is_selected(entity)) {
        deselect(entity);
    } else {
        select(entity);
    }
}

bool EditorSelection::is_selected(u32 entity) const {
    return std::find(selected_.begin(), selected_.end(), entity)
           != selected_.end();
}

void EditorSelection::notify() {
    if (on_change_) on_change_(selected_);
}

// ── EditorState ─────────────────────────────────────────────────────────────

EditorState::EditorState() = default;

void EditorState::play() {
    if (play_state_ != PlayState::Editing) return;

    // Save current scene state
    // (In real engine, this would serialize the entire scene)
    snapshot_.selected_entities = selection_.all();
    snapshot_.valid = true;

    play_state_ = PlayState::Playing;
    NX_INFO("Editor: entered Play mode");
}

void EditorState::pause() {
    if (play_state_ != PlayState::Playing) return;
    play_state_ = PlayState::Paused;
    NX_INFO("Editor: paused");
}

void EditorState::resume() {
    if (play_state_ != PlayState::Paused) return;
    play_state_ = PlayState::Playing;
    NX_INFO("Editor: resumed");
}

void EditorState::stop() {
    if (play_state_ == PlayState::Editing) return;

    // Restore scene state
    if (snapshot_.valid) {
        selection_.clear();
        for (u32 e : snapshot_.selected_entities) {
            selection_.select(e);
        }
    }

    play_state_ = PlayState::Editing;
    step_requests_ = 0;
    NX_INFO("Editor: stopped, returned to Edit mode");
}

void EditorState::step() {
    if (play_state_ == PlayState::Editing) {
        play();
        pause();
    }
    if (play_state_ == PlayState::Paused) {
        step_requests_++;
    }
}

u32 EditorState::consume_step_requests() {
    u32 count = step_requests_;
    step_requests_ = 0;
    return count;
}

void EditorState::tick(f32 dt) {
    editor_time_ += dt;
    panels_.update(dt);
}

} // namespace nexus::editor
