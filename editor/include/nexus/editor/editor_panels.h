#pragma once

#include "nexus/editor/panel.h"
#include <vector>
#include <functional>

namespace nexus {
class Registry;
class Scene;
class ForwardRenderer3D;
class BatchRenderer2D;
}

namespace nexus::editor {

// ─────────────────────────────────────────────────────────────────────────────
// ViewportPanel — 3D/2D scene viewport with gizmo controls
// ─────────────────────────────────────────────────────────────────────────────

enum class GizmoMode : u8 {
    None,
    Translate,
    Rotate,
    Scale
};

enum class GizmoSpace : u8 {
    Local,
    World
};

class ViewportPanel : public Panel {
public:
    ViewportPanel() : Panel("Viewport") {}

    void on_render() override;
    const char* type_id() const override { return "ViewportPanel"; }

    /// Bind scene and renderers for viewport rendering.
    void bind_scene(Scene* scene) { scene_ = scene; }
    void bind_renderer_3d(ForwardRenderer3D* r) { renderer_3d_ = r; }
    void bind_renderer_2d(BatchRenderer2D* r) { renderer_2d_ = r; }

    GizmoMode gizmo_mode() const { return gizmo_mode_; }
    void set_gizmo_mode(GizmoMode mode) { gizmo_mode_ = mode; }

    GizmoSpace gizmo_space() const { return gizmo_space_; }
    void set_gizmo_space(GizmoSpace space) { gizmo_space_ = space; }
    void toggle_gizmo_space() {
        gizmo_space_ = (gizmo_space_ == GizmoSpace::Local)
            ? GizmoSpace::World : GizmoSpace::Local;
    }

    bool is_grid_visible() const { return show_grid_; }
    void set_grid_visible(bool v) { show_grid_ = v; }

    bool is_gizmo_active() const { return gizmo_active_; }
    void set_gizmo_active(bool a) { gizmo_active_ = a; }

    void set_viewport_size(u32 w, u32 h) { width_ = w; height_ = h; }
    u32 viewport_width() const { return width_; }
    u32 viewport_height() const { return height_; }

private:
    GizmoMode gizmo_mode_{GizmoMode::Translate};
    GizmoSpace gizmo_space_{GizmoSpace::World};
    bool show_grid_{true};
    bool gizmo_active_{false};
    u32 width_{800};
    u32 height_{600};
    Scene* scene_{nullptr};
    ForwardRenderer3D* renderer_3d_{nullptr};
    BatchRenderer2D* renderer_2d_{nullptr};
};

// ─────────────────────────────────────────────────────────────────────────────
// HierarchyPanel — entity hierarchy tree view
// ─────────────────────────────────────────────────────────────────────────────

class HierarchyPanel : public Panel {
public:
    HierarchyPanel() : Panel("Hierarchy") {}

    void on_render() override;
    const char* type_id() const override { return "HierarchyPanel"; }

    void set_selected_entity(u32 entity) { selected_ = entity; has_selection_ = true; }
    void clear_selection() { has_selection_ = false; selected_ = 0; }
    bool has_selection() const { return has_selection_; }
    u32 selected_entity() const { return selected_; }

    /// Filter entities by name.
    void set_filter(const std::string& filter) { filter_ = filter; }
    const std::string& filter() const { return filter_; }

    /// Multi-selection support.
    void add_to_selection(u32 entity);
    void remove_from_selection(u32 entity);
    const std::vector<u32>& multi_selection() const { return multi_selection_; }
    void clear_multi_selection() { multi_selection_.clear(); }
    bool is_multi_selected(u32 entity) const;

private:
    u32 selected_{0};
    bool has_selection_{false};
    std::string filter_;
    std::vector<u32> multi_selection_;
};

// ─────────────────────────────────────────────────────────────────────────────
// InspectorPanel — property editor for selected entity
// ─────────────────────────────────────────────────────────────────────────────

struct PropertyEdit {
    std::string component;
    std::string property;
    std::string old_value;
    std::string new_value;
};

class InspectorPanel : public Panel {
public:
    InspectorPanel() : Panel("Inspector") {}

    void on_render() override;
    const char* type_id() const override { return "InspectorPanel"; }

    void set_target_entity(u32 entity) { target_ = entity; has_target_ = true; }
    void clear_target() { has_target_ = false; target_ = 0; }
    bool has_target() const { return has_target_; }
    u32 target_entity() const { return target_; }

    /// Track pending edits.
    void push_edit(const PropertyEdit& edit) { pending_edits_.push_back(edit); }
    std::vector<PropertyEdit> drain_edits();

    /// Lock the inspector to current target (don't follow selection).
    bool is_locked() const { return locked_; }
    void set_locked(bool l) { locked_ = l; }

private:
    u32 target_{0};
    bool has_target_{false};
    bool locked_{false};
    std::vector<PropertyEdit> pending_edits_;
};

// ─────────────────────────────────────────────────────────────────────────────
// ConsolePanel — log output and command input
// ─────────────────────────────────────────────────────────────────────────────

enum class LogLevel : u8 {
    Info, Warning, Error, Debug
};

struct ConsoleMessage {
    std::string text;
    LogLevel level{LogLevel::Info};
    f32 timestamp{0.0f};
};

class ConsolePanel : public Panel {
public:
    ConsolePanel() : Panel("Console") {}

    void on_render() override;
    const char* type_id() const override { return "ConsolePanel"; }

    void add_message(const std::string& text, LogLevel level = LogLevel::Info);
    void clear();

    const std::vector<ConsoleMessage>& messages() const { return messages_; }
    u32 message_count() const { return static_cast<u32>(messages_.size()); }

    /// Filter by log level.
    void set_level_filter(LogLevel level, bool show);
    bool is_level_shown(LogLevel level) const;

    /// Auto-scroll to bottom.
    bool auto_scroll() const { return auto_scroll_; }
    void set_auto_scroll(bool s) { auto_scroll_ = s; }

    /// Max messages before pruning.
    void set_max_messages(u32 max) { max_messages_ = max; }
    u32 max_messages() const { return max_messages_; }

private:
    std::vector<ConsoleMessage> messages_;
    bool show_info_{true};
    bool show_warning_{true};
    bool show_error_{true};
    bool show_debug_{true};
    bool auto_scroll_{true};
    u32 max_messages_{1000};
};

// ─────────────────────────────────────────────────────────────────────────────
// AssetBrowserPanel — browse and manage project assets
// ─────────────────────────────────────────────────────────────────────────────

struct AssetBrowserEntry {
    std::string name;
    std::string path;
    std::string extension;
    bool is_directory{false};
    u64 file_size{0};
};

class AssetBrowserPanel : public Panel {
public:
    AssetBrowserPanel() : Panel("Asset Browser") {}

    void on_render() override;
    const char* type_id() const override { return "AssetBrowserPanel"; }

    /// Set the root directory for browsing.
    void set_root_path(const std::string& path) { root_path_ = path; }
    const std::string& root_path() const { return root_path_; }

    /// Navigate into a directory.
    void navigate_to(const std::string& path);
    void navigate_up();
    const std::string& current_path() const { return current_path_; }

    /// Get directory history.
    const std::vector<std::string>& history() const { return history_; }
    bool can_go_back() const { return history_index_ > 0; }
    bool can_go_forward() const {
        return history_index_ + 1 < static_cast<i32>(history_.size());
    }
    void go_back();
    void go_forward();

    /// Selected asset.
    void set_selected(const std::string& path) { selected_ = path; }
    const std::string& selected() const { return selected_; }

    /// Search/filter.
    void set_search(const std::string& query) { search_ = query; }
    const std::string& search() const { return search_; }

    /// Set entries for current directory.
    void set_entries(std::vector<AssetBrowserEntry> entries) {
        entries_ = std::move(entries);
    }
    const std::vector<AssetBrowserEntry>& entries() const { return entries_; }

    /// View mode.
    enum class ViewMode : u8 { Grid, List };
    ViewMode view_mode() const { return view_mode_; }
    void set_view_mode(ViewMode mode) { view_mode_ = mode; }

    /// Thumbnail size for grid view.
    u32 thumbnail_size() const { return thumbnail_size_; }
    void set_thumbnail_size(u32 size) { thumbnail_size_ = size; }

private:
    std::string root_path_;
    std::string current_path_;
    std::string selected_;
    std::string search_;
    std::vector<std::string> history_;
    i32 history_index_{-1};
    std::vector<AssetBrowserEntry> entries_;
    ViewMode view_mode_{ViewMode::Grid};
    u32 thumbnail_size_{64};
};

} // namespace nexus::editor
