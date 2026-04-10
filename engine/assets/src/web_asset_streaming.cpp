#include "nexus/assets/web_asset_streaming.h"
#include "nexus/core/log.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <set>

namespace nexus::assets {

// ─────────────────────────────────────────────────────────────────────────────
// StreamingManifest
// ─────────────────────────────────────────────────────────────────────────────

bool StreamingManifest::load_from_json(const std::string& json_str) {
    try {
        auto j = nlohmann::json::parse(json_str);

        if (j.contains("base_url")) {
            base_url_ = j["base_url"].get<std::string>();
        }

        entries_.clear();
        id_lookup_.clear();
        path_lookup_.clear();

        if (!j.contains("entries") || !j["entries"].is_array()) {
            NX_ERROR("StreamingManifest: JSON missing 'entries' array");
            return false;
        }

        for (auto& je : j["entries"]) {
            StreamingManifestEntry entry;
            entry.id = AssetId(je.value("id", u64{0}));
            entry.type = static_cast<AssetType>(je.value("type", u8{0}));
            entry.virtual_path = je.value("virtual_path", "");
            entry.chunk_file = je.value("chunk_file", "");
            entry.offset_in_chunk = je.value("offset_in_chunk", u64{0});
            entry.size = je.value("size", u64{0});
            entry.priority = je.value("priority", u32{0});

            if (je.contains("dependencies") && je["dependencies"].is_array()) {
                for (auto& dep : je["dependencies"]) {
                    entry.dependencies.push_back(AssetId(dep.get<u64>()));
                }
            }

            add_entry(entry);
        }

        NX_INFO("StreamingManifest: loaded {} entries from JSON", entries_.size());
        return true;
    } catch (const nlohmann::json::exception& e) {
        NX_ERROR("StreamingManifest: JSON parse error: {}", e.what());
        return false;
    }
}

std::string StreamingManifest::save_to_json() const {
    nlohmann::json j;
    j["base_url"] = base_url_;

    auto& arr = j["entries"];
    arr = nlohmann::json::array();

    for (auto& entry : entries_) {
        nlohmann::json je;
        je["id"] = entry.id.value;
        je["type"] = static_cast<u8>(entry.type);
        je["virtual_path"] = entry.virtual_path;
        je["chunk_file"] = entry.chunk_file;
        je["offset_in_chunk"] = entry.offset_in_chunk;
        je["size"] = entry.size;
        je["priority"] = entry.priority;

        if (!entry.dependencies.empty()) {
            nlohmann::json deps = nlohmann::json::array();
            for (auto& dep : entry.dependencies) {
                deps.push_back(dep.value);
            }
            je["dependencies"] = deps;
        }

        arr.push_back(je);
    }

    return j.dump(2);
}

void StreamingManifest::add_entry(const StreamingManifestEntry& entry) {
    u32 index = static_cast<u32>(entries_.size());
    entries_.push_back(entry);
    id_lookup_[entry.id] = index;
    if (!entry.virtual_path.empty()) {
        path_lookup_[entry.virtual_path] = index;
    }
}

const StreamingManifestEntry* StreamingManifest::find_entry(AssetId id) const {
    auto it = id_lookup_.find(id);
    if (it == id_lookup_.end()) return nullptr;
    return &entries_[it->second];
}

const StreamingManifestEntry* StreamingManifest::find_entry(const std::string& virtual_path) const {
    auto it = path_lookup_.find(virtual_path);
    if (it == path_lookup_.end()) return nullptr;
    return &entries_[it->second];
}

u64 StreamingManifest::total_size() const {
    u64 total = 0;
    for (auto& entry : entries_) {
        total += entry.size;
    }
    return total;
}

std::vector<std::string> StreamingManifest::chunk_files() const {
    std::set<std::string> unique;
    for (auto& entry : entries_) {
        if (!entry.chunk_file.empty()) {
            unique.insert(entry.chunk_file);
        }
    }
    return {unique.begin(), unique.end()};
}

// ─────────────────────────────────────────────────────────────────────────────
// WebAssetStreamer
// ─────────────────────────────────────────────────────────────────────────────

bool WebAssetStreamer::initialize(const StreamingManifest& manifest) {
    manifest_ = manifest;
    ready_assets_.clear();
    requested_assets_.clear();
    requested_chunks_.clear();
    cache_.clear();
    total_downloaded_ = 0;

    // Clear the priority queue
    download_queue_ = {};
    active_downloads_.clear();

    NX_INFO("WebAssetStreamer: initialized with {} assets, base_url='{}'",
            manifest_.entry_count(), manifest_.base_url());
    return true;
}

void WebAssetStreamer::request_asset(AssetId id, u32 priority) {
    if (ready_assets_.count(id)) return;
    if (requested_assets_.count(id)) return;

    auto* entry = manifest_.find_entry(id);
    if (!entry) {
        NX_WARN("WebAssetStreamer: asset {} not found in manifest", id.value);
        return;
    }

    requested_assets_.insert(id);

    // Queue the chunk file if not already requested
    if (!entry->chunk_file.empty() && !requested_chunks_.count(entry->chunk_file)) {
        requested_chunks_.insert(entry->chunk_file);
        u32 effective_priority = std::max(priority, entry->priority);
        download_queue_.push({entry->chunk_file, effective_priority});
    }
}

void WebAssetStreamer::request_asset(const std::string& virtual_path, u32 priority) {
    auto* entry = manifest_.find_entry(virtual_path);
    if (!entry) {
        NX_WARN("WebAssetStreamer: asset '{}' not found in manifest", virtual_path);
        return;
    }
    request_asset(entry->id, priority);
}

void WebAssetStreamer::request_assets_by_type(AssetType type, u32 priority) {
    for (auto& entry : manifest_.entries()) {
        if (entry.type == type) {
            request_asset(entry.id, priority);
        }
    }
}

u32 WebAssetStreamer::update() {
    u32 newly_ready = 0;

    // Check active downloads for completion (desktop: they complete immediately)
    auto it = active_downloads_.begin();
    while (it != active_downloads_.end()) {
        if (it->state == DownloadState::Complete || it->state == DownloadState::Failed) {
            it = active_downloads_.erase(it);
        } else {
            ++it;
        }
    }

    // Start new downloads up to max_concurrent
    while (!download_queue_.empty() &&
           active_downloads_.size() < static_cast<size_t>(max_concurrent_)) {
        auto pending = download_queue_.top();
        download_queue_.pop();

        // Skip if already cached
        if (cache_.count(pending.chunk_file)) {
            // Resolve assets from this cached chunk
            auto assets = resolve_chunk_assets(pending.chunk_file);
            for (auto& aid : assets) {
                if (requested_assets_.count(aid) && !ready_assets_.count(aid)) {
                    ready_assets_.insert(aid);
                    newly_ready++;

                    if (ready_cb_) {
                        auto* entry = manifest_.find_entry(aid);
                        ready_cb_(aid, entry ? entry->virtual_path : "");
                    }
                }
            }
            continue;
        }

        if (start_download(pending.chunk_file)) {
            // On desktop, load_local_chunk completes synchronously,
            // so check if assets became ready
            if (cache_.count(pending.chunk_file)) {
                auto assets = resolve_chunk_assets(pending.chunk_file);
                for (auto& aid : assets) {
                    if (requested_assets_.count(aid) && !ready_assets_.count(aid)) {
                        ready_assets_.insert(aid);
                        newly_ready++;

                        if (ready_cb_) {
                            auto* entry = manifest_.find_entry(aid);
                            ready_cb_(aid, entry ? entry->virtual_path : "");
                        }
                    }
                }
            }
        }
    }

    return newly_ready;
}

bool WebAssetStreamer::is_asset_ready(AssetId id) const {
    return ready_assets_.count(id) > 0;
}

bool WebAssetStreamer::is_asset_ready(const std::string& virtual_path) const {
    auto* entry = manifest_.find_entry(virtual_path);
    if (!entry) return false;
    return ready_assets_.count(entry->id) > 0;
}

std::vector<u8> WebAssetStreamer::get_asset_data(AssetId id) const {
    if (!ready_assets_.count(id)) return {};

    auto* entry = manifest_.find_entry(id);
    if (!entry) return {};

    auto cache_it = cache_.find(entry->chunk_file);
    if (cache_it == cache_.end()) return {};

    // Bump access count (const_cast is acceptable for cache bookkeeping)
    const_cast<CacheEntry&>(cache_it->second).access_count++;

    auto& chunk_data = cache_it->second.data;
    u64 offset = entry->offset_in_chunk;
    u64 size = entry->size;

    if (offset + size > chunk_data.size()) {
        NX_ERROR("WebAssetStreamer: asset data out of range in chunk '{}'",
                 entry->chunk_file);
        return {};
    }

    return {chunk_data.begin() + static_cast<ptrdiff_t>(offset),
            chunk_data.begin() + static_cast<ptrdiff_t>(offset + size)};
}

std::vector<u8> WebAssetStreamer::get_asset_data(const std::string& virtual_path) const {
    auto* entry = manifest_.find_entry(virtual_path);
    if (!entry) return {};
    return get_asset_data(entry->id);
}

DownloadProgress WebAssetStreamer::progress() const {
    DownloadProgress prog;
    prog.bytes_downloaded = total_downloaded_;
    prog.bytes_total = manifest_.total_size();
    prog.assets_ready = static_cast<u32>(ready_assets_.size());
    prog.assets_total = manifest_.entry_count();
    prog.chunks_total = static_cast<u32>(manifest_.chunk_files().size());
    prog.chunks_complete = static_cast<u32>(cache_.size());

    if (!active_downloads_.empty()) {
        prog.current_state = DownloadState::Downloading;
        prog.current_chunk = active_downloads_.front().chunk_file;
    } else if (download_queue_.empty() && requested_assets_.empty()) {
        prog.current_state = DownloadState::Pending;
    } else if (ready_assets_.size() == requested_assets_.size() && !requested_assets_.empty()) {
        prog.current_state = DownloadState::Complete;
    } else {
        prog.current_state = DownloadState::Pending;
    }

    return prog;
}

u64 WebAssetStreamer::cache_size() const {
    u64 total = 0;
    for (auto& [key, entry] : cache_) {
        total += entry.size();
    }
    return total;
}

void WebAssetStreamer::clear_cache() {
    cache_.clear();
    total_downloaded_ = 0;
}

void WebAssetStreamer::evict_lru(u64 target_size) {
    while (cache_size() > target_size && !cache_.empty()) {
        // Find entry with lowest access_count
        auto min_it = cache_.begin();
        for (auto it = cache_.begin(); it != cache_.end(); ++it) {
            if (it->second.access_count < min_it->second.access_count) {
                min_it = it;
            }
        }

        NX_INFO("WebAssetStreamer: evicting chunk '{}' ({} bytes, {} accesses)",
                min_it->first, min_it->second.size(), min_it->second.access_count);

        // Remove ready status for assets in this chunk
        auto assets = resolve_chunk_assets(min_it->first);
        for (auto& aid : assets) {
            ready_assets_.erase(aid);
        }

        cache_.erase(min_it);
    }
}

bool WebAssetStreamer::start_download(const std::string& chunk_file) {
#ifdef __EMSCRIPTEN__
    // TODO: Use emscripten_async_wget2_data() to fetch chunk_file from
    // manifest_.base_url() + "/" + chunk_file.
    // The onload callback should call on_chunk_complete().
    // The onerror callback should mark the ActiveDownload as Failed.
    // The onprogress callback should update bytes_received.
    //
    // Example:
    //   std::string url = manifest_.base_url() + "/" + chunk_file;
    //   emscripten_async_wget2_data(url.c_str(), "GET", "", this, true,
    //       [](unsigned, void* arg, void* buf, unsigned sz) { ... },
    //       [](unsigned, void* arg, int code, const char* msg) { ... },
    //       [](unsigned, void* arg, int loaded, int total) { ... });

    NX_WARN("WebAssetStreamer: Emscripten HTTP fetch not yet implemented for '{}'",
            chunk_file);
    return false;
#else
    // Desktop fallback: load from local directory
    return load_local_chunk(chunk_file);
#endif
}

bool WebAssetStreamer::load_local_chunk(const std::string& chunk_file) {
    std::string path = local_dir_;
    if (!path.empty() && path.back() != '/') {
        path += '/';
    }
    path += chunk_file;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        NX_ERROR("WebAssetStreamer: failed to open local chunk '{}'", path);

        ActiveDownload dl;
        dl.chunk_file = chunk_file;
        dl.state = DownloadState::Failed;
        active_downloads_.push_back(dl);
        return false;
    }

    std::vector<u8> data{std::istreambuf_iterator<char>(file),
                         std::istreambuf_iterator<char>{}};

    // Record as an active download that completes immediately
    ActiveDownload dl;
    dl.chunk_file = chunk_file;
    dl.url = path;
    dl.bytes_received = data.size();
    dl.bytes_total = data.size();
    dl.state = DownloadState::Complete;
    active_downloads_.push_back(dl);

    on_chunk_complete(chunk_file, data);
    return true;
}

void WebAssetStreamer::on_chunk_complete(const std::string& chunk_file,
                                         const std::vector<u8>& data) {
    CacheEntry entry;
    entry.chunk_file = chunk_file;
    entry.data = data;
    entry.timestamp = static_cast<u64>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    entry.access_count = 0;

    total_downloaded_ += data.size();
    cache_[chunk_file] = std::move(entry);

    NX_INFO("WebAssetStreamer: chunk '{}' complete ({} bytes)", chunk_file, data.size());

    // Enforce cache limit
    if (cache_size() > cache_limit_) {
        evict_lru(cache_limit_);
    }
}

std::vector<AssetId> WebAssetStreamer::resolve_chunk_assets(const std::string& chunk_file) const {
    std::vector<AssetId> result;
    for (auto& entry : manifest_.entries()) {
        if (entry.chunk_file == chunk_file) {
            result.push_back(entry.id);
        }
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// generate_manifest — split a package into streaming chunks
// ─────────────────────────────────────────────────────────────────────────────

StreamingManifest WebAssetStreamer::generate_manifest(const AssetPackage& package,
                                                      const std::string& base_url,
                                                      u64 chunk_size) {
    StreamingManifest manifest;
    manifest.set_base_url(base_url);

    u32 chunk_index = 0;
    u64 current_chunk_offset = 0;
    std::string current_chunk = "chunk_0.bin";

    for (auto& pkg_entry : package.entries()) {
        // Start a new chunk if adding this entry would exceed the size limit
        if (current_chunk_offset > 0 && current_chunk_offset + pkg_entry.size > chunk_size) {
            chunk_index++;
            current_chunk = "chunk_" + std::to_string(chunk_index) + ".bin";
            current_chunk_offset = 0;
        }

        StreamingManifestEntry entry;
        entry.id = pkg_entry.id;
        entry.type = pkg_entry.type;
        entry.virtual_path = pkg_entry.path;
        entry.chunk_file = current_chunk;
        entry.offset_in_chunk = current_chunk_offset;
        entry.size = pkg_entry.size;
        entry.priority = 0;

        manifest.add_entry(entry);
        current_chunk_offset += pkg_entry.size;
    }

    NX_INFO("WebAssetStreamer: generated manifest with {} entries across {} chunks",
            manifest.entry_count(), manifest.chunk_files().size());
    return manifest;
}

} // namespace nexus::assets
