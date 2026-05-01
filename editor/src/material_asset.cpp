#include "nexus/editor/material_asset.h"

#include "nexus/assets/asset_handle.h"
#include "nexus/assets/asset_loader.h"
#include "nexus/assets/asset_registry.h"
#include "nexus/core/log.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

namespace nexus::editor {

MaterialAssetCache::~MaterialAssetCache() = default;

namespace {

// Importer is stateless and cheap; one shared instance avoids per-call ctor.
nexus::assets::MaterialImporter& importer() {
    static nexus::assets::MaterialImporter inst;
    return inst;
}

// Pulls (or creates) a stable AssetId for a path so dropped materials
// survive scene save/load round-trips.  Mirrors the pattern in
// AssetDropImporter — keeps registry membership consistent across panels.
void ensure_registered(nexus::assets::AssetRegistry* reg,
                       const std::string& path) {
    if (!reg) return;
    if (reg->find_by_path(path)) return;
    reg->register_asset(path, path, nexus::assets::AssetType::Material);
}

}  // namespace

std::shared_ptr<nexus::assets::MaterialData>
MaterialAssetCache::load(const std::string& path) {
    if (path.empty()) return nullptr;

    if (auto it = data_.find(path); it != data_.end()) {
        return it->second;
    }

    nexus::assets::AssetMeta meta;
    meta.path = path;
    auto generic = importer().import(path, meta);
    if (!generic) {
        // Importer already logged the reason — nothing to add.
        return nullptr;
    }
    auto mat = std::dynamic_pointer_cast<nexus::assets::MaterialData>(generic);
    if (!mat) {
        NX_ERROR("MaterialAssetCache: importer returned wrong type for '{}'", path);
        return nullptr;
    }

    data_[path] = mat;
    // Allocate id eagerly so the very first call to id_for() after load()
    // returns the same value the cache will keep handing out.
    if (path_to_id_.find(path) == path_to_id_.end()) {
        const u32 id = next_id_++;
        path_to_id_[path] = id;
        id_to_path_[id]   = path;
    }
    ensure_registered(registry_, path);
    return mat;
}

bool MaterialAssetCache::save(const std::string& path,
                               const nexus::assets::MaterialData& data) {
    if (path.empty()) return false;
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    // Ignore ec — create_directories returns false on "already exists",
    // and the actual write below will surface any real I/O failure.

    nlohmann::json j = {
        {"shader",             data.shader_path},
        {"albedo",             data.albedo_texture},
        {"normal",             data.normal_texture},
        {"metallic_roughness", data.metallic_roughness_texture},
        {"metallic",           data.metallic},
        {"roughness",          data.roughness},
        {"color",              {data.color[0], data.color[1],
                                data.color[2], data.color[3]}},
    };

    std::ofstream out(path);
    if (!out) {
        NX_ERROR("MaterialAssetCache: failed to open '{}' for writing", path);
        return false;
    }
    out << j.dump(2);
    if (!out) {
        NX_ERROR("MaterialAssetCache: write failed for '{}'", path);
        return false;
    }

    // Cache the just-written data so subsequent load() returns the same
    // pointer the user just saved without round-tripping through disk.
    auto cached = std::make_shared<nexus::assets::MaterialData>(data);
    data_[path] = cached;
    if (path_to_id_.find(path) == path_to_id_.end()) {
        const u32 id = next_id_++;
        path_to_id_[path] = id;
        id_to_path_[id]   = path;
    }
    ensure_registered(registry_, path);
    return true;
}

u32 MaterialAssetCache::create_default(const std::string& path, bool overwrite) {
    namespace fs = std::filesystem;
    if (path.empty()) return 0u;
    if (!overwrite && fs::exists(path)) {
        NX_WARN("MaterialAssetCache: refused to overwrite existing '{}'", path);
        // Still register so the editor can open the existing file via id.
        load(path);
        return id_for(path);
    }
    nexus::assets::MaterialData fresh;
    fresh.color[0] = 1.0f;
    fresh.color[1] = 1.0f;
    fresh.color[2] = 1.0f;
    fresh.color[3] = 1.0f;
    fresh.metallic  = 0.0f;
    fresh.roughness = 1.0f;
    if (!save(path, fresh)) return 0u;
    return id_for(path);
}

std::shared_ptr<nexus::assets::MaterialData>
MaterialAssetCache::get_by_id(u32 id) const {
    auto it_p = id_to_path_.find(id);
    if (it_p == id_to_path_.end()) return nullptr;
    auto it_d = data_.find(it_p->second);
    return it_d == data_.end() ? nullptr : it_d->second;
}

u32 MaterialAssetCache::id_for(const std::string& path) {
    if (path.empty()) return 0u;
    if (auto it = path_to_id_.find(path); it != path_to_id_.end()) {
        return it->second;
    }
    const u32 id = next_id_++;
    path_to_id_[path] = id;
    id_to_path_[id]   = path;
    return id;
}

std::string MaterialAssetCache::path_for(u32 id) const {
    auto it = id_to_path_.find(id);
    return it == id_to_path_.end() ? std::string{} : it->second;
}

}  // namespace nexus::editor
