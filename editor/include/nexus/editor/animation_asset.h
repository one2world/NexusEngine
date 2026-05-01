#pragma once

#include "nexus/core/types.h"
#include "nexus/animation/animation_clip.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace nexus::assets { class AssetRegistry; }

namespace nexus::editor {

// ─────────────────────────────────────────────────────────────────────────────
// AnimationAssetCache — editor-owned AnimationClip lookup by file path
// ─────────────────────────────────────────────────────────────────────────────
//
// Manages skeletal `AnimationClip` (engine type) instances loaded from
// `.anim` / `.anim.json` files.  Mirrors the structure of MaterialAssetCache
// and PrefabAssetCache so the editor's three asset families have a uniform
// surface:
//
//   • `load(path)` — parse JSON into a cached AnimationClip; second call
//     is a free lookup.
//   • `save(path, clip)` — write the clip out as JSON; updates cache.
//   • `id_for(path)` — stable 32-bit id for AssetRegistry round-trip.
//   • `get(path)` / `get_by_id(id)` — direct cache access.
//
// All methods are exception-free in the failure case.  Missing files,
// corrupt JSON, dead clips all log and return nullptr / 0 / INVALID.
class AnimationAssetCache {
public:
    AnimationAssetCache() = default;
    ~AnimationAssetCache() = default;

    AnimationAssetCache(const AnimationAssetCache&)            = delete;
    AnimationAssetCache& operator=(const AnimationAssetCache&) = delete;

    void set_asset_registry(assets::AssetRegistry* reg) { registry_ = reg; }

    /// Load (or fetch from cache).  Returns nullptr on parse failure or
    /// missing file.  Allocates a stable id on first appearance.
    const anim::AnimationClip* load(const std::string& path);

    /// Persist `clip` to `path` as JSON.  Updates the cache so subsequent
    /// load() returns the just-written data without disk re-read.
    bool save(const std::string& path, const anim::AnimationClip& clip);

    /// Allocate (or fetch) a stable 32-bit id for a path.  Always non-zero.
    /// Animation ids start at 12288 to leave headroom above material
    /// (4096+) and prefab (8192+) ranges.
    u32 id_for(const std::string& path);

    /// Reverse lookup; "" for unknown ids.
    std::string path_for(u32 id) const;

    /// Direct cache access.  Returns nullptr when not loaded.
    const anim::AnimationClip* get(const std::string& path) const;
    const anim::AnimationClip* get_by_id(u32 id) const;

    u32 size() const { return static_cast<u32>(clips_.size()); }
    bool contains(const std::string& path) const {
        return clips_.find(path) != clips_.end();
    }

private:
    static constexpr u32 kFirstAnimationId = 12288u;

    std::unordered_map<std::string, std::unique_ptr<anim::AnimationClip>> clips_;
    std::unordered_map<std::string, u32> path_to_id_;
    std::unordered_map<u32, std::string> id_to_path_;
    u32 next_id_{kFirstAnimationId};
    assets::AssetRegistry* registry_{nullptr};
};

}  // namespace nexus::editor
