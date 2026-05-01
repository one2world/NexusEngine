#include "nexus/editor/undo_redo.h"

namespace nexus::editor {

void UndoRedoManager::execute(std::unique_ptr<Command> cmd) {
    cmd->execute();
    undo_stack_.push_back(std::move(cmd));
    redo_stack_.clear();

    // Trim oldest if exceeding max
    if (max_history_ > 0 && undo_stack_.size() > max_history_) {
        undo_stack_.pop_front();
        // Adjust saved index
        if (saved_index_ >= 0) saved_index_--;
    }
}

bool UndoRedoManager::undo() {
    if (undo_stack_.empty()) return false;

    auto cmd = std::move(undo_stack_.back());
    undo_stack_.pop_back();

    cmd->undo();
    redo_stack_.push_back(std::move(cmd));
    return true;
}

bool UndoRedoManager::redo() {
    if (redo_stack_.empty()) return false;

    auto cmd = std::move(redo_stack_.back());
    redo_stack_.pop_back();

    cmd->execute();
    undo_stack_.push_back(std::move(cmd));
    return true;
}

std::string UndoRedoManager::undo_description() const {
    return undo_stack_.empty() ? "" : undo_stack_.back()->description();
}

std::string UndoRedoManager::redo_description() const {
    return redo_stack_.empty() ? "" : redo_stack_.back()->description();
}

std::vector<std::string> UndoRedoManager::undo_history() const {
    std::vector<std::string> result;
    for (auto it = undo_stack_.rbegin(); it != undo_stack_.rend(); ++it) {
        result.push_back((*it)->description());
    }
    return result;
}

std::vector<std::string> UndoRedoManager::redo_history() const {
    std::vector<std::string> result;
    for (auto it = redo_stack_.rbegin(); it != redo_stack_.rend(); ++it) {
        result.push_back((*it)->description());
    }
    return result;
}

i32 UndoRedoManager::jump_to_undo(u32 target_undo_count) {
    // Clamp target into the valid total-history range [0, undo+redo].
    const u32 total = static_cast<u32>(undo_stack_.size() + redo_stack_.size());
    if (target_undo_count > total) target_undo_count = total;

    i32 steps = 0;
    // Too many commands applied — undo down to target.
    while (undo_stack_.size() > target_undo_count) {
        auto cmd = std::move(undo_stack_.back());
        undo_stack_.pop_back();
        cmd->undo();
        redo_stack_.push_back(std::move(cmd));
        ++steps;
    }
    // Too few — redo up to target.
    while (undo_stack_.size() < target_undo_count && !redo_stack_.empty()) {
        auto cmd = std::move(redo_stack_.back());
        redo_stack_.pop_back();
        cmd->execute();
        undo_stack_.push_back(std::move(cmd));
        --steps;
    }
    return steps;
}

void UndoRedoManager::clear() {
    undo_stack_.clear();
    redo_stack_.clear();
    saved_index_ = 0;
}

} // namespace nexus::editor
