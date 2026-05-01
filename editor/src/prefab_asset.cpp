#include "nexus/editor/prefab_asset.h"

#include "nexus/assets/asset_registry.h"
#include "nexus/core/log.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace nexus::editor {

namespace {

// Stable AssetId registration — keeps prefab paths discoverable across
// scene save/load round-trips.  Mirrors the helper in MaterialAssetCache.
void ensure_registered(nexus::assets::AssetRegistry* reg,
                       const std::string& path) {
    if (!reg) return;
    if (reg->find_by_path(path)) return;
    reg->register_asset(path, path, nexus::assets::AssetType::Prefab);
}

}  // namespace

const Prefab* PrefabAssetCache::load(const std::string& path) {
    if (path.empty()) return nullptr;

    if (auto it = prefabs_.find(path); it != prefabs_.end()) {
        return &it->second;
    }

    std::ifstream in(path);
    if (!in) {
        NX_ERROR("PrefabAssetCache: failed to open '{}'", path);
        return nullptr;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    std::string json = ss.str();
    if (json.empty()) {
        NX_ERROR("PrefabAssetCache: '{}' is empty", path);
        return nullptr;
    }

    // Quick sanity gate — Prefab::instantiate will already log a parse
    // failure on invalid JSON, but failing here lets us avoid caching a
    // useless entry that would silently fail on every instantiate.
    if (json.find_first_not_of(" \t\r\n") == std::string::npos ||
        (json.front() != '{' && json.front() != '[')) {
        NX_ERROR("PrefabAssetCache: '{}' is not valid JSON", path);
        return nullptr;
    }

    // Stem the filename to derive a human-readable name; strip both
    // .prefab.json and .prefab so either convention round-trips.
    namespace fs = std::filesystem;
    std::string name = fs::path(path).filename().string();
    auto endswith = [](const std::string& s, const std::string& suf) {
        return s.size() >= suf.size() &&
               s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
    };
    if (endswith(name, ".prefab.json")) {
        name = name.substr(0, name.size() - 12);
    } else if (endswith(name, ".prefab")) {
        name = name.substr(0, name.size() - 7);
    } else {
        name = fs::path(name).stem().string();
    }

    prefabs_.emplace(path, Prefab{name, std::move(json)});
    if (path_to_id_.find(path) == path_to_id_.end()) {
        const u32 id = next_id_++;
        path_to_id_[path] = id;
        id_to_path_[id]   = path;
    }
    ensure_registered(registry_, path);
    return &prefabs_.at(path);
}

const Prefab* PrefabAssetCache::save_from_entity(const std::string& path,
                                                  const Scene& scene,
                                                  Entity entity) {
    if (path.empty()) return nullptr;
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    // ec ignored — file write below surfaces real failure.

    if (!scene.registry().alive(entity)) {
        NX_ERROR("PrefabAssetCache: entity {} is not alive",
                 static_cast<u32>(entity));
        return nullptr;
    }

    std::string name = fs::path(path).stem().string();
    Prefab fresh = Prefab::from_entity(scene, entity, name);
    if (!fresh.valid()) {
        NX_ERROR("PrefabAssetCache: from_entity returned empty for '{}'", path);
        return nullptr;
    }

    std::ofstream out(path);
    if (!out) {
        NX_ERROR("PrefabAssetCache: failed to open '{}' for writing", path);
        return nullptr;
    }
    out << fresh.data();
    if (!out) {
        NX_ERROR("PrefabAssetCache: write failed for '{}'", path);
        return nullptr;
    }

    prefabs_[path] = std::move(fresh);
    if (path_to_id_.find(path) == path_to_id_.end()) {
        const u32 id = next_id_++;
        path_to_id_[path] = id;
        id_to_path_[id]   = path;
    }
    ensure_registered(registry_, path);
    return &prefabs_.at(path);
}

Entity PrefabAssetCache::instantiate(const std::string& path, Scene& scene) {
    const Prefab* p = load(path);
    if (!p) return INVALID_ENTITY;
    return p->instantiate(scene);
}

u32 PrefabAssetCache::id_for(const std::string& path) {
    if (path.empty()) return 0u;
    if (auto it = path_to_id_.find(path); it != path_to_id_.end()) {
        return it->second;
    }
    const u32 id = next_id_++;
    path_to_id_[path] = id;
    id_to_path_[id]   = path;
    return id;
}

std::string PrefabAssetCache::path_for(u32 id) const {
    auto it = id_to_path_.find(id);
    return it == id_to_path_.end() ? std::string{} : it->second;
}

const Prefab* PrefabAssetCache::get(const std::string& path) const {
    auto it = prefabs_.find(path);
    return it == prefabs_.end() ? nullptr : &it->second;
}

}  // namespace nexus::editor
