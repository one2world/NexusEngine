#include "nexus/editor/panel.h"
#include <algorithm>

namespace nexus::editor {

// ── DockSpace ───────────────────────────────────────────────────────────────

DockSpace::DockSpace() {
    // Create root node
    DockNode root;
    root.id = 0;
    root.position = DockPosition::Center;
    root.size_ratio = 1.0f;
    nodes_.push_back(root);
}

void DockSpace::dock(const std::string& panel_title, DockPosition pos,
                      f32 size_ratio) {
    panel_positions_[panel_title] = pos;

    // Find or create a node for this position
    bool found = false;
    for (auto& node : nodes_) {
        if (node.position == pos && node.id != 0) {
            node.panels.push_back(panel_title);
            found = true;
            break;
        }
    }

    if (!found) {
        DockNode node;
        node.id = next_node_id_++;
        node.position = pos;
        node.size_ratio = size_ratio;
        node.panels.push_back(panel_title);
        nodes_.push_back(node);
        nodes_[0].children.push_back(node.id);
    }
}

void DockSpace::undock(const std::string& panel_title) {
    panel_positions_.erase(panel_title);

    for (auto& node : nodes_) {
        auto it = std::find(node.panels.begin(), node.panels.end(), panel_title);
        if (it != node.panels.end()) {
            node.panels.erase(it);
            break;
        }
    }
}

DockPosition DockSpace::get_position(const std::string& panel_title) const {
    auto it = panel_positions_.find(panel_title);
    return it != panel_positions_.end() ? it->second : DockPosition::Float;
}

std::vector<std::string> DockSpace::panels_at(DockPosition pos) const {
    std::vector<std::string> result;
    for (auto& [title, p] : panel_positions_) {
        if (p == pos) result.push_back(title);
    }
    return result;
}

// ── PanelManager ────────────────────────────────────────────────────────────

void PanelManager::add_panel(std::unique_ptr<Panel> panel) {
    panels_.push_back(std::move(panel));
}

void PanelManager::remove_panel(const std::string& title) {
    panels_.erase(
        std::remove_if(panels_.begin(), panels_.end(),
            [&](const auto& p) { return p->title() == title; }),
        panels_.end());
}

Panel* PanelManager::find(const std::string& title) {
    for (auto& p : panels_) {
        if (p->title() == title) return p.get();
    }
    return nullptr;
}

void PanelManager::update(f32 dt) {
    for (auto& p : panels_) {
        if (p->is_visible()) {
            p->on_update(dt);
        }
    }
}

void PanelManager::render() {
    for (auto& p : panels_) {
        if (p->is_visible()) {
            p->on_render();
        }
    }
}

void PanelManager::focus(const std::string& title) {
    for (auto& p : panels_) {
        p->set_focused(p->title() == title);
    }
}

Panel* PanelManager::focused() const {
    for (auto& p : panels_) {
        if (p->is_focused()) return p.get();
    }
    return nullptr;
}

} // namespace nexus::editor
