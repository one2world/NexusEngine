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

    // Take the scene snapshot *through the bridge* so the one code path
    // covers every caller (menu, toolbar, shortcut, step-from-editing).
    // Without a bridge we still enter Play — useful for headless tests —
    // but Stop won't restore anything.
    bool scene_ok = true;
    if (bridge_.take_snapshot) {
        scene_ok = bridge_.take_snapshot();
        if (!scene_ok) {
            NX_WARN("Editor::play: take_snapshot failed — Stop will not restore");
        }
    }
    snapshot_.selected_entities = selection_.all();
    snapshot_.valid = scene_ok;

    play_state_ = PlayState::Playing;
    if (bridge_.on_play) bridge_.on_play();
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

    // Restore the scene *first* (so entities the selection referred to exist
    // again before we try to re-select them).
    if (snapshot_.valid && bridge_.restore_snapshot) {
        bridge_.restore_snapshot();
    }
    if (bridge_.clear_snapshot) bridge_.clear_snapshot();

    // Then restore the selection list captured at play time.
    if (snapshot_.valid) {
        selection_.clear();
        for (u32 e : snapshot_.selected_entities) {
            selection_.select(e);
        }
    }
    snapshot_ = {};

    play_state_ = PlayState::Editing;
    step_requests_ = 0;
    if (bridge_.on_stop) bridge_.on_stop();
    NX_INFO("Editor: stopped, returned to Edit mode");
}

void EditorState::step() {
    // From Editing: take snapshot via play(), then pause.  This is the path
    // that used to skip the scene snapshot entirely (data loss on Stop).
    if (play_state_ == PlayState::Editing) {
        play();
        if (play_state_ == PlayState::Playing) pause();
    } else if (play_state_ == PlayState::Playing) {
        // From Playing: pause and queue one step so the user always sees
        // the effect of the step (otherwise it would blend into running sim).
        pause();
    }
    if (play_state_ == PlayState::Paused) {
        ++step_requests_;
    }
}

bool EditorState::advance_simulation(f32 real_dt) {
    if (!bridge_.simulate) return false;
    switch (play_state_) {
        case PlayState::Editing:
            return false;
        case PlayState::Playing: {
            // fixed_dt_ == 0 opts into real-time dt.  Otherwise we run a
            // single fixed step per frame, matching Unity's Time.fixedDeltaTime
            // model (physics-bound sim shouldn't flap with display rate).
            const f32 dt = (fixed_dt_ > 0.0f) ? fixed_dt_ : real_dt;
            bridge_.simulate(dt);
            return true;
        }
        case PlayState::Paused: {
            if (step_requests_ == 0) return false;
            // Drain every queued step in one frame so rapid Step clicks are
            // honored — but each step uses the canonical fixed_dt so physics
            // behaves deterministically regardless of user click cadence.
            const f32 dt = (fixed_dt_ > 0.0f) ? fixed_dt_ : real_dt;
            u32 steps = step_requests_;
            step_requests_ = 0;
            for (u32 i = 0; i < steps; ++i) bridge_.simulate(dt);
            return true;
        }
    }
    return false;
}

void EditorState::tick(f32 dt) {
    editor_time_ += dt;
    panels_.update(dt);
}

} // namespace nexus::editor
