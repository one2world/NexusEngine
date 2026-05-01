#pragma once

#include "nexus/core/types.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace nexus::assets {
struct MaterialData;
class AssetRegistry;
}  // namespace nexus::assets

namespace nexus::editor {

// ─────────────────────────────────────────────────────────────────────────────
// MaterialAssetCache — editor-owned MaterialData lookup by file path
// ─────────────────────────────────────────────────────────────────────────────
//
// The engine's MeshRendererComponent has a `material_id` slot but no way to
// edit the underlying material from the editor without leaking
// asset-loading concerns into the panel layer.  This cache fills that gap:
//
//   • Maps absolute path → shared MaterialData pointer.
//   • `load(path)` reads from disk via MaterialImporter (JSON-first, with
//     legacy key=value fallback).
//   • `save(path)` writes JSON.  Editor inspector calls this when the user
//     hits "Apply" / "Save".
//   • `create_default(path)` writes a fresh material with sensible
//     defaults (white albedo, 0 metallic, 1 roughness) — backs the Asset
//     Browser's "Create > Material" entry.
//   • `id_for(path)` allocates a stable 32-bit id usable as
//     MeshRendererComponent::material_id.  Existing paths return the same
//     id so scene save/load round-trips work.
//
// All methods are exception-free in the failure case: a missing file logs
// and returns nullptr / 0; corrupt JSON logs and returns nullptr.
class MaterialAssetCache {
public:
    MaterialAssetCache() = default;
    ~MaterialAssetCache();

    MaterialAssetCache(const MaterialAssetCache&)            = delete;
    MaterialAssetCache& operator=(const MaterialAssetCache&) = delete;

    void set_asset_registry(assets::AssetRegistry* reg) { registry_ = reg; }

    /// Load (or fetch from cache).  Returns nullptr on parse failure or
    /// missing file.  On the path's first appearance a stable material id
    /// is allocated so scenes can reference it.
    std::shared_ptr<assets::MaterialData> load(const std::string& path);

    /// Persist the in-memory MaterialData back to `path` as JSON.  Does
    /// NOT update id_for() or invalidate the cache — the live shared_ptr
    /// is still authoritative.  Returns false if the file couldn't be
    /// written.
    bool save(const std::string& path,
              const assets::MaterialData& data);

    /// Create a new material file at `path` with sensible defaults and
    /// register it with the cache.  Returns the allocated id (0 on
    /// failure).  Does NOT overwrite an existing file unless `overwrite`
    /// is true.
    u32 create_default(const std::string& path, bool overwrite = false);

    /// Fetch the cached MaterialData for an id allocated by load() /
    /// create_default().  Returns nullptr if `id` is unknown.
    std::shared_ptr<assets::MaterialData> get_by_id(u32 id) const;

    /// Allocate (or fetch) the stable 32-bit id for a path.  Always
    /// non-zero.  Used by Inspector when binding MeshRendererComponent's
    /// material_id slot from a dragged-in material file.
    u32 id_for(const std::string& path);

    /// Reverse lookup — returns the path for a tracked id, or "" if not
    /// tracked.  Used by Inspector to display the current material's
    /// filename next to MeshRendererComponent's slot.
    std::string path_for(u32 id) const;

    // Diagnostics.
    u32 size() const { return static_cast<u32>(data_.size()); }
    bool contains(const std::string& path) const {
        return data_.find(path) != data_.end();
    }

private:
    // Material ids start above the primitive-mesh range (1..3) and the
    // imported-mesh range (1024+) so renderer code can disambiguate by
    // band.  Ample headroom for many materials per project.
    static constexpr u32 kFirstMaterialId = 4096u;

    std::unordered_map<std::string, std::shared_ptr<assets::MaterialData>> data_;
    std::unordered_map<std::string, u32> path_to_id_;
    std::unordered_map<u32, std::string> id_to_path_;
    u32 next_id_{kFirstMaterialId};
    assets::AssetRegistry* registry_{nullptr};
};

}  // namespace nexus::editor
