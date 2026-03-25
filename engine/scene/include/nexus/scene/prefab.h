#pragma once

#include "nexus/scene/scene.h"
#include "nexus/scene/scene_serializer.h"
#include <string>
#include <unordered_map>
#include <functional>

namespace nexus {

// ─────────────────────────────────────────────────────────────────────────────
// Prefab — a reusable entity template stored as serialized JSON
// ─────────────────────────────────────────────────────────────────────────────

class Prefab {
public:
    Prefab() = default;
    Prefab(const std::string& name, const std::string& json_data)
        : name_(name), json_data_(json_data) {}

    const std::string& name() const { return name_; }
    const std::string& data() const { return json_data_; }
    bool valid() const { return !json_data_.empty(); }

    /// Create a prefab from the current state of an entity in a scene.
    static Prefab from_entity(const Scene& scene, Entity entity, const std::string& name);

    /// Instantiate this prefab into a scene. Returns the root entity, or INVALID_ENTITY on failure.
    Entity instantiate(Scene& scene) const;

private:
    std::string name_;
    std::string json_data_;
};

// ─────────────────────────────────────────────────────────────────────────────
// PrefabLibrary — manages a collection of named prefabs
// ─────────────────────────────────────────────────────────────────────────────

class PrefabLibrary {
public:
    void add(const Prefab& prefab);
    void remove(const std::string& name);

    const Prefab* get(const std::string& name) const;
    bool has(const std::string& name) const;

    /// Instantiate a named prefab into a scene. Returns null entity on failure.
    Entity instantiate(const std::string& name, Scene& scene) const;

    /// Save all prefabs to a directory.
    bool save_to_directory(const std::string& dir_path) const;

    /// Load all .prefab.json files from a directory.
    bool load_from_directory(const std::string& dir_path);

    const std::unordered_map<std::string, Prefab>& all() const { return prefabs_; }

private:
    std::unordered_map<std::string, Prefab> prefabs_;
};

} // namespace nexus
