#pragma once

#include "nexus/assets/asset_handle.h"
#include <unordered_map>
#include <mutex>
#include <vector>
#include <functional>
#include <string>

namespace nexus::assets {

// ─────────────────────────────────────────────────────────────────────────────
// AssetRegistry — central catalog of all known assets
// ─────────────────────────────────────────────────────────────────────────────

class AssetRegistry {
public:
    AssetRegistry() = default;

    /// Register an asset path. Returns the assigned AssetId.
    AssetId register_asset(const std::string& virtual_path,
                           const std::string& source_path,
                           AssetType type = AssetType::Unknown);

    /// Unregister an asset.
    bool unregister_asset(AssetId id);

    /// Find asset metadata by id.
    AssetMeta* find(AssetId id);
    const AssetMeta* find(AssetId id) const;

    /// Find asset metadata by virtual path.
    AssetMeta* find_by_path(const std::string& path);
    const AssetMeta* find_by_path(const std::string& path) const;

    /// Store loaded data for an asset.
    void store_data(AssetId id, std::shared_ptr<AssetData> data);

    /// Get loaded data for an asset.
    std::shared_ptr<AssetData> get_data(AssetId id) const;

    /// Get a typed handle to an asset.
    template <typename T>
    AssetHandle<T> get_handle(AssetId id) {
        auto* meta = find(id);
        if (!meta) return {};
        auto data = get_data(id);
        if (!data) return {};
        auto typed = std::dynamic_pointer_cast<T>(data);
        if (!typed) return {};
        return AssetHandle<T>(meta, typed);
    }

    template <typename T>
    AssetHandle<T> get_handle(const std::string& path) {
        auto* meta = find_by_path(path);
        if (!meta) return {};
        return get_handle<T>(meta->id);
    }

    /// Get all registered asset ids.
    std::vector<AssetId> all_assets() const;

    /// Get all assets of a specific type.
    std::vector<AssetId> assets_of_type(AssetType type) const;

    /// Total number of registered assets.
    u32 count() const;

    /// Total memory usage of loaded assets.
    u64 total_memory_usage() const;

    /// Unload assets with zero references.
    u32 garbage_collect();

    /// Clear all assets.
    void clear();

    /// Callback for asset events.
    enum class Event { Registered, Loaded, Unloaded, Reloaded, Failed };
    using EventCallback = std::function<void(Event, AssetId)>;

    void set_event_callback(EventCallback cb) { event_callback_ = std::move(cb); }

private:
    void notify(Event event, AssetId id);

    std::unordered_map<AssetId, AssetMeta> assets_;
    std::unordered_map<std::string, AssetId> path_lookup_;
    std::unordered_map<AssetId, std::shared_ptr<AssetData>> data_;
    EventCallback event_callback_;
};

} // namespace nexus::assets
