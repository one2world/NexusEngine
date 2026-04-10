#pragma once

#include "nexus/assets/asset_handle.h"
#include "nexus/assets/asset_package.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <queue>
#include <mutex>

namespace nexus::assets {

// ─────────────────────────────────────────────────────────────────────────────
// StreamingChunk — descriptor for a downloadable chunk file
// ─────────────────────────────────────────────────────────────────────────────

struct StreamingChunk {
    std::string url;           // Relative URL to chunk file
    u64 offset{0};             // Offset within the chunk file
    u64 size{0};               // Uncompressed size
    u64 compressed_size{0};    // 0 = not compressed
    u32 checksum{0};           // CRC32
};

// ─────────────────────────────────────────────────────────────────────────────
// StreamingManifestEntry — maps an asset ID to its chunk location
// ─────────────────────────────────────────────────────────────────────────────

struct StreamingManifestEntry {
    AssetId id;
    AssetType type{AssetType::Unknown};
    std::string virtual_path;
    std::string chunk_file;     // Which chunk file contains this asset
    u64 offset_in_chunk{0};     // Offset within the chunk
    u64 size{0};
    u32 priority{0};            // Higher = load earlier
    std::vector<AssetId> dependencies; // Assets that must load first
};

// ─────────────────────────────────────────────────────────────────────────────
// StreamingManifest — describes all assets available for streaming download
// ─────────────────────────────────────────────────────────────────────────────

class StreamingManifest {
public:
    StreamingManifest() = default;

    bool load_from_json(const std::string& json_str);
    std::string save_to_json() const;

    void add_entry(const StreamingManifestEntry& entry);
    const StreamingManifestEntry* find_entry(AssetId id) const;
    const StreamingManifestEntry* find_entry(const std::string& virtual_path) const;

    const std::vector<StreamingManifestEntry>& entries() const { return entries_; }
    u32 entry_count() const { return static_cast<u32>(entries_.size()); }

    // Base URL for all chunk downloads
    void set_base_url(const std::string& url) { base_url_ = url; }
    const std::string& base_url() const { return base_url_; }

    // Total download size across all entries
    u64 total_size() const;

    // Get unique chunk file list
    std::vector<std::string> chunk_files() const;

private:
    std::string base_url_;
    std::vector<StreamingManifestEntry> entries_;
    std::unordered_map<AssetId, u32> id_lookup_;
    std::unordered_map<std::string, u32> path_lookup_;
};

// ─────────────────────────────────────────────────────────────────────────────
// Download state and progress tracking
// ─────────────────────────────────────────────────────────────────────────────

enum class DownloadState : u8 {
    Pending,
    Downloading,
    Complete,
    Failed,
    Cached
};

struct DownloadProgress {
    u64 bytes_downloaded{0};
    u64 bytes_total{0};
    u32 chunks_complete{0};
    u32 chunks_total{0};
    u32 assets_ready{0};
    u32 assets_total{0};
    DownloadState current_state{DownloadState::Pending};
    std::string current_chunk;
    f32 fraction() const {
        return bytes_total > 0
            ? static_cast<f32>(bytes_downloaded) / static_cast<f32>(bytes_total)
            : 0.0f;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// CacheEntry — holds downloaded chunk data in memory
// ─────────────────────────────────────────────────────────────────────────────

struct CacheEntry {
    std::string chunk_file;
    std::vector<u8> data;
    u64 timestamp{0};          // When cached
    u32 access_count{0};
    u64 size() const { return data.size(); }
};

// ─────────────────────────────────────────────────────────────────────────────
// WebAssetStreamer — streams assets over HTTP (or from local dir on desktop)
// ─────────────────────────────────────────────────────────────────────────────
//
// On Emscripten: uses emscripten_async_wget for HTTP fetches.
// On desktop:    simulates by reading from a local directory (for testing).

class WebAssetStreamer {
public:
    WebAssetStreamer() = default;
    ~WebAssetStreamer() = default;

    // Initialize with a manifest
    bool initialize(const StreamingManifest& manifest);

    // Set the local fallback directory (for desktop testing)
    void set_local_directory(const std::string& dir) { local_dir_ = dir; }

    // Request an asset to be streamed
    void request_asset(AssetId id, u32 priority = 0);
    void request_asset(const std::string& virtual_path, u32 priority = 0);

    // Request all assets of a type
    void request_assets_by_type(AssetType type, u32 priority = 0);

    // Process pending downloads (call each frame)
    // Returns number of assets that became ready this tick
    u32 update();

    // Check if an asset is ready
    bool is_asset_ready(AssetId id) const;
    bool is_asset_ready(const std::string& virtual_path) const;

    // Get downloaded asset data
    std::vector<u8> get_asset_data(AssetId id) const;
    std::vector<u8> get_asset_data(const std::string& virtual_path) const;

    // Progress info
    DownloadProgress progress() const;

    // Cache management
    u64 cache_size() const;
    void set_cache_limit(u64 bytes) { cache_limit_ = bytes; }
    void clear_cache();
    void evict_lru(u64 target_size);

    // Callback when an asset becomes ready
    using AssetReadyCallback = std::function<void(AssetId, const std::string&)>;
    void set_ready_callback(AssetReadyCallback cb) { ready_cb_ = std::move(cb); }

    // Is streaming active?
    bool is_active() const { return !download_queue_.empty() || !active_downloads_.empty(); }

    // Concurrent download limit
    void set_max_concurrent(u32 n) { max_concurrent_ = n; }
    u32 max_concurrent() const { return max_concurrent_; }

    // Generate a manifest from an AssetPackage (for build pipeline)
    static StreamingManifest generate_manifest(const AssetPackage& package,
                                                const std::string& base_url,
                                                u64 chunk_size = 1024 * 1024);

private:
    struct PendingDownload {
        std::string chunk_file;
        u32 priority{0};
        bool operator<(const PendingDownload& o) const { return priority < o.priority; }
    };

    struct ActiveDownload {
        std::string chunk_file;
        std::string url;
        DownloadState state{DownloadState::Downloading};
        u64 bytes_received{0};
        u64 bytes_total{0};
    };

    bool start_download(const std::string& chunk_file);
    bool load_local_chunk(const std::string& chunk_file);
    void on_chunk_complete(const std::string& chunk_file, const std::vector<u8>& data);
    std::vector<AssetId> resolve_chunk_assets(const std::string& chunk_file) const;

    StreamingManifest manifest_;
    std::string local_dir_;

    std::priority_queue<PendingDownload> download_queue_;
    std::vector<ActiveDownload> active_downloads_;
    std::unordered_map<std::string, CacheEntry> cache_;  // chunk_file -> data
    std::unordered_set<AssetId> ready_assets_;
    std::unordered_set<AssetId> requested_assets_;
    std::unordered_set<std::string> requested_chunks_;

    u64 cache_limit_{256 * 1024 * 1024}; // 256 MB default
    u32 max_concurrent_{4};
    u64 total_downloaded_{0};

    AssetReadyCallback ready_cb_;
};

} // namespace nexus::assets
