#include "nexus/editor/build_scenes.h"

#include "nexus/core/log.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace nexus::editor {

bool BuildScenes::add(const std::string& path) {
    if (path.empty()) return false;
    if (contains(path)) return false;
    scenes_.push_back(path);
    return true;
}

bool BuildScenes::remove(const std::string& path) {
    auto it = std::find(scenes_.begin(), scenes_.end(), path);
    if (it == scenes_.end()) return false;
    const u32 idx = static_cast<u32>(std::distance(scenes_.begin(), it));
    scenes_.erase(it);
    // Adjust startup_index_:
    //   - If we removed the row before startup, slide the index left.
    //   - If we removed startup itself, snap to the new top (0) — better
    //     than picking an arbitrary neighbour.  Empty list → 0.
    if (scenes_.empty()) {
        startup_index_ = 0;
    } else if (idx < startup_index_) {
        --startup_index_;
    } else if (idx == startup_index_) {
        startup_index_ = 0;
    }
    return true;
}

bool BuildScenes::move(u32 from, u32 to) {
    if (from >= scenes_.size() || to >= scenes_.size()) return false;
    if (from == to) return true;
    std::string entry = std::move(scenes_[from]);
    scenes_.erase(scenes_.begin() + from);
    scenes_.insert(scenes_.begin() + to, std::move(entry));
    // Update startup_index_ to track the moved entry's neighbourhood.
    if (startup_index_ == from) {
        startup_index_ = to;
    } else if (from < startup_index_ && to >= startup_index_) {
        --startup_index_;
    } else if (from > startup_index_ && to <= startup_index_) {
        ++startup_index_;
    }
    return true;
}

bool BuildScenes::set_startup(const std::string& path) {
    const u32 idx = index_of(path);
    if (idx >= scenes_.size()) return false;
    // Move to the front so the array is also reorderable in-place — keeps
    // serialised JSON deterministic and matches Unity's "drag to top" UX.
    if (idx != 0) {
        move(idx, 0);
    }
    startup_index_ = 0;
    return true;
}

void BuildScenes::set_startup_index(u32 idx) {
    if (scenes_.empty()) {
        startup_index_ = 0;
        return;
    }
    if (idx >= scenes_.size()) idx = static_cast<u32>(scenes_.size() - 1);
    startup_index_ = idx;
}

bool BuildScenes::contains(const std::string& path) const {
    return std::find(scenes_.begin(), scenes_.end(), path) != scenes_.end();
}

std::string BuildScenes::startup_scene() const {
    if (scenes_.empty()) return {};
    if (startup_index_ >= scenes_.size()) return scenes_.front();
    return scenes_[startup_index_];
}

u32 BuildScenes::index_of(const std::string& path) const {
    auto it = std::find(scenes_.begin(), scenes_.end(), path);
    return it == scenes_.end() ? static_cast<u32>(scenes_.size())
                               : static_cast<u32>(std::distance(scenes_.begin(), it));
}

std::string BuildScenes::next_after(const std::string& current) const {
    if (scenes_.empty()) return {};
    const u32 idx = index_of(current);
    if (idx >= scenes_.size()) return {};
    const u32 next = (idx + 1u) % static_cast<u32>(scenes_.size());
    return scenes_[next];
}

// ── Persistence ─────────────────────────────────────────────────────────────

std::string BuildScenes::to_json_string() const {
    nlohmann::json j;
    j["startup_index"] = startup_index_;
    j["scenes"]        = scenes_;
    return j.dump(2);
}

bool BuildScenes::from_json_string(const std::string& json) {
    if (json.empty()) return false;
    try {
        auto j = nlohmann::json::parse(json);
        std::vector<std::string> scenes;
        if (j.contains("scenes") && j["scenes"].is_array()) {
            for (const auto& v : j["scenes"]) {
                if (v.is_string()) scenes.push_back(v.get<std::string>());
            }
        }
        u32 idx = j.value("startup_index", 0u);
        if (!scenes.empty() && idx >= scenes.size()) {
            idx = static_cast<u32>(scenes.size() - 1);
        }
        if (scenes.empty()) idx = 0;

        // Apply atomically — only mutate state after a successful parse so
        // a corrupt file can't half-clobber the existing list.
        scenes_        = std::move(scenes);
        startup_index_ = idx;
        return true;
    } catch (const std::exception& e) {
        NX_ERROR("BuildScenes: JSON parse failed: {}", e.what());
        return false;
    }
}

bool BuildScenes::load_from_file(const std::string& path) {
    if (path.empty()) return false;
    std::ifstream in(path);
    if (!in) {
        NX_WARN("BuildScenes: failed to open '{}'", path);
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return from_json_string(ss.str());
}

bool BuildScenes::save_to_file(const std::string& path) const {
    if (path.empty()) return false;
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    std::ofstream out(path);
    if (!out) {
        NX_ERROR("BuildScenes: failed to open '{}' for writing", path);
        return false;
    }
    out << to_json_string();
    if (!out) {
        NX_ERROR("BuildScenes: write failed for '{}'", path);
        return false;
    }
    return true;
}

}  // namespace nexus::editor
