#pragma once

#include "nexus/scene/scene.h"
#include "nexus/scene/scene_serializer.h"
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>

namespace nexus {

// ─────────────────────────────────────────────────────────────────────────────
// SceneManager — manages scene loading, switching, and transitions
// ─────────────────────────────────────────────────────────────────────────────

class SceneManager {
public:
    using SceneSetupFn = std::function<void(Scene&)>;

    SceneManager() = default;

    /// Register a scene by name with an optional setup function.
    void register_scene(const std::string& name, SceneSetupFn setup = nullptr);

    /// Register a scene by name with a file path to load from.
    void register_scene_file(const std::string& name, const std::string& path);

    /// Load and switch to a scene by name. Returns false if not registered.
    bool load_scene(const std::string& name);

    /// Queue a scene to load at the end of the current frame.
    void queue_load_scene(const std::string& name);

    /// Process any queued scene loads. Call once per frame.
    void process_pending();

    /// Get the active scene.
    Scene* active_scene() { return active_scene_.get(); }
    const Scene* active_scene() const { return active_scene_.get(); }

    /// Get the name of the active scene.
    const std::string& active_scene_name() const { return active_name_; }

    /// Check if a scene is registered.
    bool has_scene(const std::string& name) const;

    /// Get all registered scene names.
    std::vector<std::string> scene_names() const;

    /// Set callback for when a scene is about to be unloaded.
    void set_on_scene_unload(std::function<void(const std::string&)> cb) {
        on_unload_ = std::move(cb);
    }

    /// Set callback for when a new scene finishes loading.
    void set_on_scene_loaded(std::function<void(const std::string&)> cb) {
        on_loaded_ = std::move(cb);
    }

private:
    struct SceneEntry {
        std::string name;
        std::string file_path;
        SceneSetupFn setup;
    };

    std::unordered_map<std::string, SceneEntry> scenes_;
    std::unique_ptr<Scene> active_scene_;
    std::string active_name_;
    std::string pending_scene_;

    std::function<void(const std::string&)> on_unload_;
    std::function<void(const std::string&)> on_loaded_;
};

} // namespace nexus
