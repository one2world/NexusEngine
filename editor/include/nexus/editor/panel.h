#pragma once

#include "nexus/core/types.h"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <memory>

namespace nexus::editor {

// ─────────────────────────────────────────────────────────────────────────────
// Panel — base class for dockable editor panels
// ─────────────────────────────────────────────────────────────────────────────

class Panel {
public:
    explicit Panel(const std::string& title) : title_(title) {}
    virtual ~Panel() = default;

    const std::string& title() const { return title_; }
    bool is_visible() const { return visible_; }
    void set_visible(bool v) { visible_ = v; }
    void toggle_visible() { visible_ = !visible_; }

    bool is_focused() const { return focused_; }
    void set_focused(bool f) { focused_ = f; }

    /// Called each frame to render the panel contents.
    virtual void on_update(f32 dt) { (void)dt; }
    virtual void on_render() = 0;

    /// Panel lifecycle.
    virtual void on_open() {}
    virtual void on_close() {}

    /// Unique type identifier for this panel class.
    virtual const char* type_id() const = 0;

protected:
    std::string title_;
    bool visible_{true};
    bool focused_{false};
};

// ─────────────────────────────────────────────────────────────────────────────
// DockSpace — manages panel layout
// ─────────────────────────────────────────────────────────────────────────────

enum class DockPosition : u8 {
    Center,
    Left,
    Right,
    Top,
    Bottom,
    Float
};

struct DockNode {
    u32 id{0};
    DockPosition position{DockPosition::Center};
    f32 size_ratio{0.5f};           // Fraction of parent size
    std::vector<std::string> panels; // Panel titles docked here
    std::vector<u32> children;       // Child dock node IDs
};

class DockSpace {
public:
    DockSpace();

    /// Dock a panel at a position relative to the root.
    void dock(const std::string& panel_title, DockPosition pos, f32 size_ratio = 0.25f);

    /// Undock a panel.
    void undock(const std::string& panel_title);

    /// Get the dock position of a panel.
    DockPosition get_position(const std::string& panel_title) const;

    /// Get all panels at a dock position.
    std::vector<std::string> panels_at(DockPosition pos) const;

    /// Root node.
    const DockNode& root() const { return nodes_[0]; }

    /// Node count.
    u32 node_count() const { return static_cast<u32>(nodes_.size()); }

private:
    std::vector<DockNode> nodes_;
    std::unordered_map<std::string, DockPosition> panel_positions_;
    u32 next_node_id_{1};
};

// ─────────────────────────────────────────────────────────────────────────────
// PanelManager — owns and manages all editor panels
// ─────────────────────────────────────────────────────────────────────────────

class PanelManager {
public:
    PanelManager() = default;

    /// Register a panel. Takes ownership.
    void add_panel(std::unique_ptr<Panel> panel);

    /// Remove a panel by title.
    void remove_panel(const std::string& title);

    /// Find a panel by title.
    Panel* find(const std::string& title);

    template <typename T>
    T* find_typed(const std::string& title) {
        return dynamic_cast<T*>(find(title));
    }

    /// Update all visible panels.
    void update(f32 dt);

    /// Render all visible panels.
    void render();

    /// Get all panels.
    const std::vector<std::unique_ptr<Panel>>& panels() const { return panels_; }

    /// Panel count.
    u32 count() const { return static_cast<u32>(panels_.size()); }

    /// Set a panel as focused (unfocusing all others).
    void focus(const std::string& title);

    /// Get the currently focused panel.
    Panel* focused() const;

    /// Dock space.
    DockSpace& dock_space() { return dock_space_; }
    const DockSpace& dock_space() const { return dock_space_; }

private:
    std::vector<std::unique_ptr<Panel>> panels_;
    DockSpace dock_space_;
};

} // namespace nexus::editor
