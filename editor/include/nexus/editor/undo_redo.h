#pragma once

#include "nexus/core/types.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <deque>

namespace nexus::editor {

// ─────────────────────────────────────────────────────────────────────────────
// Command — base class for undoable operations
// ─────────────────────────────────────────────────────────────────────────────

class Command {
public:
    virtual ~Command() = default;

    /// Execute the command.
    virtual void execute() = 0;

    /// Undo the command.
    virtual void undo() = 0;

    /// Description for display.
    virtual std::string description() const = 0;

    /// Can this command be merged with another of the same type?
    virtual bool merge_with(const Command&) { return false; }

    /// Unique type identifier.
    virtual const char* type_id() const = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// Common commands
// ─────────────────────────────────────────────────────────────────────────────

/// Generic lambda-based command
class LambdaCommand : public Command {
public:
    using DoFn = std::function<void()>;
    using UndoFn = std::function<void()>;

    LambdaCommand(std::string desc, DoFn do_fn, UndoFn undo_fn)
        : desc_(std::move(desc)), do_fn_(std::move(do_fn)),
          undo_fn_(std::move(undo_fn)) {}

    void execute() override { if (do_fn_) do_fn_(); }
    void undo() override { if (undo_fn_) undo_fn_(); }
    std::string description() const override { return desc_; }
    const char* type_id() const override { return "LambdaCommand"; }

private:
    std::string desc_;
    DoFn do_fn_;
    UndoFn undo_fn_;
};

/// Compound command (groups multiple commands as one undo step)
class CompoundCommand : public Command {
public:
    explicit CompoundCommand(std::string desc) : desc_(std::move(desc)) {}

    void add(std::unique_ptr<Command> cmd) { commands_.push_back(std::move(cmd)); }

    void execute() override {
        for (auto& cmd : commands_) cmd->execute();
    }

    void undo() override {
        for (auto it = commands_.rbegin(); it != commands_.rend(); ++it) {
            (*it)->undo();
        }
    }

    std::string description() const override { return desc_; }
    const char* type_id() const override { return "CompoundCommand"; }
    u32 sub_command_count() const { return static_cast<u32>(commands_.size()); }

private:
    std::string desc_;
    std::vector<std::unique_ptr<Command>> commands_;
};

// ─────────────────────────────────────────────────────────────────────────────
// UndoRedoManager — maintains the undo/redo history stack
// ─────────────────────────────────────────────────────────────────────────────

class UndoRedoManager {
public:
    UndoRedoManager() = default;

    /// Execute a command and push it to the undo stack.
    void execute(std::unique_ptr<Command> cmd);

    /// Undo the last command.
    bool undo();

    /// Redo the last undone command.
    bool redo();

    /// Can we undo?
    bool can_undo() const { return !undo_stack_.empty(); }

    /// Can we redo?
    bool can_redo() const { return !redo_stack_.empty(); }

    /// Description of the next undo operation.
    std::string undo_description() const;

    /// Description of the next redo operation.
    std::string redo_description() const;

    /// Get the full undo history (most recent first).
    std::vector<std::string> undo_history() const;

    /// Get the redo history.
    std::vector<std::string> redo_history() const;

    /// Clear all history.
    void clear();

    /// Set maximum history size (0 = unlimited).
    void set_max_history(u32 max) { max_history_ = max; }
    u32 max_history() const { return max_history_; }

    /// Current stack sizes.
    u32 undo_count() const { return static_cast<u32>(undo_stack_.size()); }
    u32 redo_count() const { return static_cast<u32>(redo_stack_.size()); }

    /// Revert/advance until exactly `target_undo_count` commands remain on the
    /// undo stack.  Fewer than current ⇒ pops and invokes undo() on each
    /// popped command; more than current ⇒ pulls from the redo stack and
    /// re-executes each.  Out-of-range values are clamped.  Returns the
    /// number of commands actually stepped (positive for undo, negative for
    /// redo).  This is the backend for Unity's Undo History click-to-revert.
    i32 jump_to_undo(u32 target_undo_count);

    /// Mark the current state as "saved" (clean).
    void mark_saved() { saved_index_ = static_cast<i32>(undo_stack_.size()); }

    /// Is the current state different from the saved state?
    bool is_dirty() const {
        return saved_index_ != static_cast<i32>(undo_stack_.size());
    }

private:
    std::deque<std::unique_ptr<Command>> undo_stack_;
    std::deque<std::unique_ptr<Command>> redo_stack_;
    u32 max_history_{100};
    i32 saved_index_{0};
};

} // namespace nexus::editor
