#pragma once

#include "nexus/scene/scene.h"
#include <string>

namespace nexus {

// ---------------------------------------------------------------------------
// SceneSerializer - JSON-based scene load/save
// ---------------------------------------------------------------------------
class SceneSerializer {
public:
    explicit SceneSerializer(Scene& scene) : scene_(scene) {}

    /// Serialize the scene to a JSON file.
    bool save(const std::string& filepath) const;

    /// Deserialize a scene from a JSON file.
    bool load(const std::string& filepath);

    /// Serialize to a JSON string (for in-memory use).
    std::string to_json() const;

    /// Deserialize from a JSON string.
    bool from_json(const std::string& json_str);

    /// Deep-duplicate an entity and all of its descendants via JSON round-trip.
    /// The new root becomes a sibling of `src` (inherits `src`'s parent) and has
    /// "(N)" appended to its TagComponent name, matching Unity's Duplicate
    /// behavior. Returns INVALID_ENTITY if the source is not alive.
    Entity duplicate_entity(Entity src);

private:
    Scene& scene_;
};

} // namespace nexus
