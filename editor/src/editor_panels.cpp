#include "nexus/editor/editor_panels.h"
#include <algorithm>

namespace nexus::editor {

// ── HierarchyPanel ─────────────────────────────────────────────────────────

void HierarchyPanel::add_to_selection(u32 entity) {
    if (!is_multi_selected(entity)) {
        multi_selection_.push_back(entity);
    }
}

void HierarchyPanel::remove_from_selection(u32 entity) {
    multi_selection_.erase(
        std::remove(multi_selection_.begin(), multi_selection_.end(), entity),
        multi_selection_.end());
}

bool HierarchyPanel::is_multi_selected(u32 entity) const {
    return std::find(multi_selection_.begin(), multi_selection_.end(), entity)
           != multi_selection_.end();
}

// ── InspectorPanel ──────────────────────────────────────────────────────────

std::vector<PropertyEdit> InspectorPanel::drain_edits() {
    std::vector<PropertyEdit> result;
    std::swap(result, pending_edits_);
    return result;
}

// ── ConsolePanel ────────────────────────────────────────────────────────────

void ConsolePanel::add_message(const std::string& text, LogLevel level) {
    ConsoleMessage msg;
    msg.text = text;
    msg.level = level;
    messages_.push_back(std::move(msg));

    // Prune if exceeding max
    while (messages_.size() > max_messages_) {
        messages_.erase(messages_.begin());
    }
}

void ConsolePanel::clear() {
    messages_.clear();
}

void ConsolePanel::set_level_filter(LogLevel level, bool show) {
    switch (level) {
        case LogLevel::Info:    show_info_ = show; break;
        case LogLevel::Warning: show_warning_ = show; break;
        case LogLevel::Error:   show_error_ = show; break;
        case LogLevel::Debug:   show_debug_ = show; break;
    }
}

bool ConsolePanel::is_level_shown(LogLevel level) const {
    switch (level) {
        case LogLevel::Info:    return show_info_;
        case LogLevel::Warning: return show_warning_;
        case LogLevel::Error:   return show_error_;
        case LogLevel::Debug:   return show_debug_;
    }
    return true;
}

// ── AssetBrowserPanel ───────────────────────────────────────────────────────

void AssetBrowserPanel::navigate_to(const std::string& path) {
    // Trim history forward if we navigated back then go somewhere new
    if (history_index_ >= 0 &&
        history_index_ + 1 < static_cast<i32>(history_.size())) {
        history_.erase(history_.begin() + history_index_ + 1, history_.end());
    }

    history_.push_back(path);
    history_index_ = static_cast<i32>(history_.size()) - 1;
    current_path_ = path;
}

void AssetBrowserPanel::navigate_up() {
    auto pos = current_path_.find_last_of('/');
    if (pos != std::string::npos && pos > 0) {
        navigate_to(current_path_.substr(0, pos));
    } else if (!current_path_.empty()) {
        navigate_to("");
    }
}

void AssetBrowserPanel::go_back() {
    if (can_go_back()) {
        history_index_--;
        current_path_ = history_[static_cast<size_t>(history_index_)];
    }
}

void AssetBrowserPanel::go_forward() {
    if (can_go_forward()) {
        history_index_++;
        current_path_ = history_[static_cast<size_t>(history_index_)];
    }
}

} // namespace nexus::editor
