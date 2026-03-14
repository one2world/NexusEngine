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

private:
    Scene& scene_;
};

} // namespace nexus
