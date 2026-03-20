#include "nexus/assets/asset_registry.h"
#include "nexus/core/log.h"
#include <filesystem>

namespace nexus::assets {

AssetId AssetRegistry::register_asset(const std::string& virtual_path,
                                       const std::string& source_path,
                                       AssetType type) {
    AssetId id(virtual_path);

    if (assets_.count(id)) {
        // Already registered — update source path if needed
        auto& meta = assets_[id];
        meta.source_path = source_path;
        if (type != AssetType::Unknown) meta.type = type;
        return id;
    }

    AssetMeta meta;
    meta.id = id;
    meta.path = virtual_path;
    meta.source_path = source_path;

    // Auto-detect type from extension if not specified
    if (type == AssetType::Unknown && !source_path.empty()) {
        std::filesystem::path p(source_path);
        if (p.has_extension()) {
            type = asset_type_from_extension(p.extension().string());
        }
    }
    meta.type = type;

    // Get file info
    if (!source_path.empty()) {
        std::error_code ec;
        auto fsize = std::filesystem::file_size(source_path, ec);
        if (!ec) meta.file_size = fsize;

        auto ftime = std::filesystem::last_write_time(source_path, ec);
        if (!ec) {
            meta.last_modified = static_cast<u64>(ftime.time_since_epoch().count());
        }
    }

    assets_[id] = meta;
    path_lookup_[virtual_path] = id;

    notify(Event::Registered, id);
    return id;
}

bool AssetRegistry::unregister_asset(AssetId id) {
    auto it = assets_.find(id);
    if (it == assets_.end()) return false;

    path_lookup_.erase(it->second.path);
    data_.erase(id);
    assets_.erase(it);

    notify(Event::Unloaded, id);
    return true;
}

AssetMeta* AssetRegistry::find(AssetId id) {
    auto it = assets_.find(id);
    return it != assets_.end() ? &it->second : nullptr;
}

const AssetMeta* AssetRegistry::find(AssetId id) const {
    auto it = assets_.find(id);
    return it != assets_.end() ? &it->second : nullptr;
}

AssetMeta* AssetRegistry::find_by_path(const std::string& path) {
    auto it = path_lookup_.find(path);
    if (it == path_lookup_.end()) return nullptr;
    return find(it->second);
}

const AssetMeta* AssetRegistry::find_by_path(const std::string& path) const {
    auto it = path_lookup_.find(path);
    if (it == path_lookup_.end()) return nullptr;
    return find(it->second);
}

void AssetRegistry::store_data(AssetId id, std::shared_ptr<AssetData> data) {
    auto* meta = find(id);
    if (!meta) return;

    data_[id] = std::move(data);
    meta->status = AssetStatus::Loaded;

    notify(Event::Loaded, id);
}

std::shared_ptr<AssetData> AssetRegistry::get_data(AssetId id) const {
    auto it = data_.find(id);
    return it != data_.end() ? it->second : nullptr;
}

std::vector<AssetId> AssetRegistry::all_assets() const {
    std::vector<AssetId> result;
    result.reserve(assets_.size());
    for (auto& [id, _] : assets_) {
        result.push_back(id);
    }
    return result;
}

std::vector<AssetId> AssetRegistry::assets_of_type(AssetType type) const {
    std::vector<AssetId> result;
    for (auto& [id, meta] : assets_) {
        if (meta.type == type) result.push_back(id);
    }
    return result;
}

u32 AssetRegistry::count() const {
    return static_cast<u32>(assets_.size());
}

u64 AssetRegistry::total_memory_usage() const {
    u64 total = 0;
    for (auto& [id, data] : data_) {
        if (data) total += data->memory_usage();
    }
    return total;
}

u32 AssetRegistry::garbage_collect() {
    u32 collected = 0;
    std::vector<AssetId> to_remove;

    for (auto& [id, meta] : assets_) {
        if (meta.ref_count.load() == 0 && meta.status == AssetStatus::Loaded) {
            to_remove.push_back(id);
        }
    }

    for (auto& id : to_remove) {
        data_.erase(id);
        auto* meta = find(id);
        if (meta) {
            meta->status = AssetStatus::Unloaded;
            notify(Event::Unloaded, id);
        }
        ++collected;
    }

    return collected;
}

void AssetRegistry::clear() {
    data_.clear();
    path_lookup_.clear();
    assets_.clear();
}

void AssetRegistry::notify(Event event, AssetId id) {
    if (event_callback_) {
        event_callback_(event, id);
    }
}

} // namespace nexus::assets
