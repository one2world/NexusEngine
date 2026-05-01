#pragma once

#include "nexus/core/types.h"
#include "nexus/scene/scene.h"
#include "nexus/scene/prefab.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace nexus::assets { class AssetRegistry; }

namespace nexus::editor {

// ─────────────────────────────────────────────────────────────────────────────
// PrefabAssetCache — editor-owned Prefab lookup by file path
// ─────────────────────────────────────────────────────────────────────────────
//
// The engine ships with `nexus::Prefab` and `nexus::PrefabLibrary` for
// runtime use, but neither integrates with the editor's drag-drop / asset
// id surface.  This cache fills that gap:
//
//   • `load(path)` reads `.prefab` / `.prefab.json` JSON from disk into a
//     `Prefab` instance and caches it.
//   • `save_from_entity(path, scene, entity)` builds a Prefab from a live
//     entity tree and writes it to disk in one step — the editor's "Save
//     as Prefab..." path goes through here.
//   • `instantiate(path, scene)` loads (or fetches from cache) and
//     instantiates the prefab.  Returns the new root entity, or
//     INVALID_ENTITY on failure.
//   • `id_for(path)` allocates a stable 32-bit id for a path so the
//     AssetRegistry round-trips correctly across scene save/load.
//
// Failure paths are exception-free: missing files, parse errors, invalid
// entity arguments all log + return a sentinel without crashing.
class PrefabAssetCache {
public:
    PrefabAssetCache() = default;
    ~PrefabAssetCache() = default;

    PrefabAssetCache(const PrefabAssetCache&)            = delete;
    PrefabAssetCache& operator=(const PrefabAssetCache&) = delete;

    void set_asset_registry(assets::AssetRegistry* reg) { registry_ = reg; }

    /// Load a .prefab file from disk into the cache.  Returns nullptr on
    /// missing file or empty / unparseable JSON.  Caches by path so a
    /// second call is a free lookup.
    const Prefab* load(const std::string& path);

    /// Build a Prefab from `entity`'s subtree, write it as JSON to `path`,
    /// cache it, and return the cached pointer.  Returns nullptr if the
    /// entity is dead or the file write fails.
    const Prefab* save_from_entity(const std::string& path,
                                    const Scene& scene,
                                    Entity entity);

    /// Load (or fetch) and instantiate the prefab into `scene`.  Returns
    /// the new root entity, or INVALID_ENTITY on failure.  Convenience
    /// over load() + Prefab::instantiate so the editor can drop a .prefab
    /// directly without two-line boilerplate.
    Entity instantiate(const std::string& path, Scene& scene);

    /// Stable 32-bit id allocation.  Used by AssetRegistry round-trip and
    /// by potential future PrefabComponent slots.  Always non-zero.
    u32 id_for(const std::string& path);

    /// Reverse lookup; returns "" for unknown ids.
    std::string path_for(u32 id) const;

    /// Direct cache access.  Returns nullptr when the path hasn't been
    /// loaded.  Tests use this to verify cache state without re-parsing.
    const Prefab* get(const std::string& path) const;

    u32 size() const { return static_cast<u32>(prefabs_.size()); }
    bool contains(const std::string& path) const {
        return prefabs_.find(path) != prefabs_.end();
    }

private:
    // Prefab ids start above the material range (4096..) and the imported
    // mesh range (1024..) so renderer code can disambiguate by band.
    static constexpr u32 kFirstPrefabId = 8192u;

    std::unordered_map<std::string, Prefab> prefabs_;
    std::unordered_map<std::string, u32>    path_to_id_;
    std::unordered_map<u32, std::string>    id_to_path_;
    u32 next_id_{kFirstPrefabId};
    assets::AssetRegistry* registry_{nullptr};
};

}  // namespace nexus::editor
