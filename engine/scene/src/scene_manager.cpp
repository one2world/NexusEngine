#include "nexus/scene/scene_manager.h"
#include "nexus/core/log.h"

namespace nexus {

void SceneManager::register_scene(const std::string& name, SceneSetupFn setup) {
    scenes_[name] = SceneEntry{name, "", std::move(setup)};
}

void SceneManager::register_scene_file(const std::string& name, const std::string& path) {
    scenes_[name] = SceneEntry{name, path, nullptr};
}

bool SceneManager::load_scene(const std::string& name) {
    auto it = scenes_.find(name);
    if (it == scenes_.end()) {
        NX_WARN("SceneManager: scene '{}' not registered", name);
        return false;
    }

    // Notify unload
    if (active_scene_ && on_unload_) {
        on_unload_(active_name_);
    }

    // Create fresh scene
    auto new_scene = std::make_unique<Scene>();

    const auto& entry = it->second;

    // Load from file if path specified
    if (!entry.file_path.empty()) {
        SceneSerializer serializer(*new_scene);
        if (!serializer.load(entry.file_path)) {
            NX_WARN("SceneManager: failed to load scene file '{}'", entry.file_path);
            return false;
        }
    }

    // Run setup function if provided
    if (entry.setup) {
        entry.setup(*new_scene);
    }

    active_scene_ = std::move(new_scene);
    active_name_ = name;

    // Notify loaded
    if (on_loaded_) {
        on_loaded_(active_name_);
    }

    NX_APP_INFO("SceneManager: loaded scene '{}'", name);
    return true;
}

void SceneManager::queue_load_scene(const std::string& name) {
    pending_scene_ = name;
}

void SceneManager::process_pending() {
    if (!pending_scene_.empty()) {
        std::string name = std::move(pending_scene_);
        pending_scene_.clear();
        load_scene(name);
    }
}

bool SceneManager::has_scene(const std::string& name) const {
    return scenes_.find(name) != scenes_.end();
}

std::vector<std::string> SceneManager::scene_names() const {
    std::vector<std::string> names;
    names.reserve(scenes_.size());
    for (const auto& [name, _] : scenes_) {
        names.push_back(name);
    }
    return names;
}

} // namespace nexus
