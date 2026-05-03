#pragma once

#include "nexus/scene/scene.h"

#include <functional>
#include <string>
#include <vector>

// nlohmann::json is forward-declared via its own header; we re-declare it
// here so consumers don't need to pull <nlohmann/json.hpp> into every
// translation unit that uses an extension (it's heavy).  The Extension
// callbacks pass a `void*` — implementations static_cast back to
// `nlohmann::json*` (or `const nlohmann::json*` for the reader).  Keeps
// the public header free of nlohmann's transitive include cost.
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

    // ── Extension hooks (M19) ────────────────────────────────────────────
    //
    // External libraries (e.g. nexus::scripting) can register per-component
    // read/write callbacks to plug into the JSON entity dump without
    // forcing engine/scene to depend on them.  This breaks the
    // scene → scripting → scene cycle that previously prevented
    // ScriptComponent from round-tripping.
    //
    // Calling convention:
    //   writer(json_ptr, registry, entity)  — entity_json is `nlohmann::json*`
    //   reader(json_ptr, registry, entity)  — entity_json is `const nlohmann::json*`
    //
    // Writer decides if the entity has the component and inserts the value
    // under whichever key it owns.  Reader checks `entity_json->contains(key)`
    // and adds/restores the component.  Pointer is never null inside the
    // serializer — callbacks may rely on that.
    //
    // Hooks are global (registered once at startup, typically by the
    // editor or a runtime bootstrap module).  Tests can call
    // `clear_extensions()` to reset between cases.
    using ExtensionWriter =
        std::function<void(void* entity_json_ptr,
                           const Registry& reg,
                           Entity e)>;
    using ExtensionReader =
        std::function<void(const void* entity_json_ptr,
                           Registry& reg,
                           Entity e)>;

    struct Extension {
        std::string     name;     // diagnostic only — surfaces in logs
        ExtensionWriter writer;
        ExtensionReader reader;
    };

    /// Register a writer/reader pair.  Returns the index of the entry so
    /// callers can later unregister it (e.g. on shutdown).
    static u32 register_extension(std::string name,
                                  ExtensionWriter writer,
                                  ExtensionReader reader);

    /// Clear every registered extension.  Used by tests; production code
    /// should rarely need this.
    static void clear_extensions();

    /// Number of currently registered extensions.  Tests use this for
    /// before/after assertions.
    static u32 extension_count();

private:
    Scene& scene_;
};

} // namespace nexus
