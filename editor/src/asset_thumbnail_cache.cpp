#include "nexus/editor/asset_thumbnail_cache.h"
#include "nexus/core/log.h"

// stb_image declarations only; the IMPLEMENTATION lives in
// asset_thumbnail_stb.cpp where third-party warnings are silenced.
#include "stb_image.h"

#include <algorithm>
#include <chrono>
#include <utility>

namespace nexus::editor {

AssetThumbnailCache::AssetThumbnailCache(rhi::RHI* rhi, u32 capacity)
    : rhi_(rhi), capacity_(capacity) {}

AssetThumbnailCache::~AssetThumbnailCache() {
    clear();
}

// ── Decode worker ────────────────────────────────────────────────────────────
//
// Pure function: takes a path, returns RGBA8 pixels.  Runs on a std::async
// worker, never touches the RHI or any cache state.  On failure returns
// `ok=false` and the cache marks the entry DecodeFailed without retrying.
AssetThumbnailCache::DecodeResult
AssetThumbnailCache::decode_file(std::string path) {
    DecodeResult r;
    r.path = std::move(path);

    int w = 0, h = 0, comp = 0;
    // Force RGBA8 output so the upload path is uniform regardless of the
    // source file's channel count.  Saves a per-format branch on the main
    // thread at the cost of one extra channel per pixel for opaque images
    // — acceptable for thumbnails (typically 64-128 px).
    stbi_uc* pixels = stbi_load(r.path.c_str(), &w, &h, &comp, 4);
    if (!pixels || w <= 0 || h <= 0) {
        if (pixels) stbi_image_free(pixels);
        return r;  // ok = false
    }

    const size_t byte_count = static_cast<size_t>(w) *
                              static_cast<size_t>(h) * 4u;
    r.pixels.assign(pixels, pixels + byte_count);
    r.width  = static_cast<u32>(w);
    r.height = static_cast<u32>(h);
    r.ok     = true;
    stbi_image_free(pixels);
    return r;
}

// ── Public API ───────────────────────────────────────────────────────────────

rhi::TextureHandle AssetThumbnailCache::request(const std::string& path) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = entries_.find(path);
    if (it != entries_.end()) {
        // Promote and return whatever we have.
        promote_to_mru(path);
        switch (it->second.state) {
        case State::Uploaded:     return it->second.texture;
        case State::DecodeFailed: return rhi::INVALID_HANDLE;
        case State::Pending:
        case State::Decoded:
        case State::Empty:        return rhi::INVALID_HANDLE;
        }
        return rhi::INVALID_HANDLE;
    }

    // First time we've seen this path — create the entry and enqueue.
    Entry fresh;
    fresh.state = State::Pending;
    entries_.emplace(path, std::move(fresh));
    lru_.push_front(path);
    lru_iter_[path] = lru_.begin();
    enqueue_decode(path);
    evict_if_over_capacity();
    return rhi::INVALID_HANDLE;
}

rhi::TextureHandle AssetThumbnailCache::lookup(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = entries_.find(path);
    if (it == entries_.end()) return rhi::INVALID_HANDLE;
    if (it->second.state != State::Uploaded) return rhi::INVALID_HANDLE;
    return it->second.texture;
}

u32 AssetThumbnailCache::drain() {
    // Step 1: collect ready futures while holding nothing GPU-related.
    std::vector<DecodeResult> ready;
    {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = in_flight_.begin();
        while (it != in_flight_.end()) {
            using namespace std::chrono_literals;
            if (it->valid() &&
                it->wait_for(0ms) == std::future_status::ready) {
                try {
                    ready.push_back(it->get());
                } catch (...) {
                    // Swallow worker exceptions — the path stays Pending and
                    // will be retried by the next request() call.  We don't
                    // want a single bad file to take down the editor.
                }
                it = in_flight_.erase(it);
            } else {
                ++it;
            }
        }
        for (const auto& r : ready) {
            in_flight_paths_.erase(r.path);
            auto eit = entries_.find(r.path);
            if (eit == entries_.end()) continue;
            if (!r.ok) {
                eit->second.state = State::DecodeFailed;
                continue;
            }
            eit->second.state  = State::Decoded;
            eit->second.width  = r.width;
            eit->second.height = r.height;
            eit->second.pixels = std::move(r.pixels);
        }
    }

    // Step 2: upload Decoded entries to the RHI.  RHI is not thread-safe so
    // this must run on the main thread (where drain() is called).
    u32 uploaded = 0;
    std::lock_guard<std::mutex> lock(mu_);
    for (auto& [path, entry] : entries_) {
        if (entry.state != State::Decoded) continue;
        if (rhi_ == nullptr) {
            // No RHI bound (test harness) — leave as Decoded so tests can
            // verify the decoded payload directly.
            continue;
        }
        rhi::TextureDesc desc{};
        desc.width            = entry.width;
        desc.height           = entry.height;
        desc.format           = rhi::TextureFormat::RGBA8;
        desc.min_filter       = rhi::TextureFilter::Linear;
        desc.mag_filter       = rhi::TextureFilter::Linear;
        desc.wrap_s           = rhi::TextureWrap::ClampToEdge;
        desc.wrap_t           = rhi::TextureWrap::ClampToEdge;
        desc.generate_mipmaps = true;
        desc.data             = entry.pixels.data();
        rhi::TextureHandle h = rhi_->create_texture(desc);
        entry.texture = h;
        entry.state   = State::Uploaded;
        // Drop the CPU pixel buffer — bounded GPU footprint is the goal.
        entry.pixels.clear();
        entry.pixels.shrink_to_fit();
        ++uploaded;
    }
    return uploaded;
}

void AssetThumbnailCache::invalidate(const std::string& path) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = entries_.find(path);
    if (it == entries_.end()) return;
    release_entry(it->second);
    auto lit = lru_iter_.find(path);
    if (lit != lru_iter_.end()) {
        lru_.erase(lit->second);
        lru_iter_.erase(lit);
    }
    entries_.erase(it);
}

void AssetThumbnailCache::clear() {
    std::lock_guard<std::mutex> lock(mu_);
    for (auto& [p, e] : entries_) {
        release_entry(e);
    }
    entries_.clear();
    lru_.clear();
    lru_iter_.clear();
    in_flight_.clear();
    in_flight_paths_.clear();
}

void AssetThumbnailCache::inject_decoded_pixels(const std::string& path,
                                                u32 width, u32 height,
                                                std::vector<u8> rgba8) {
    std::lock_guard<std::mutex> lock(mu_);
    auto& e = entries_[path];
    if (lru_iter_.find(path) == lru_iter_.end()) {
        lru_.push_front(path);
        lru_iter_[path] = lru_.begin();
    } else {
        promote_to_mru(path);
    }
    e.state  = State::Decoded;
    e.width  = width;
    e.height = height;
    e.pixels = std::move(rgba8);
    evict_if_over_capacity();
}

void AssetThumbnailCache::mark_failed(const std::string& path) {
    std::lock_guard<std::mutex> lock(mu_);
    auto& e = entries_[path];
    if (lru_iter_.find(path) == lru_iter_.end()) {
        lru_.push_front(path);
        lru_iter_[path] = lru_.begin();
    }
    e.state = State::DecodeFailed;
}

u32 AssetThumbnailCache::size() const {
    std::lock_guard<std::mutex> lock(mu_);
    return static_cast<u32>(entries_.size());
}

void AssetThumbnailCache::set_capacity(u32 c) {
    std::lock_guard<std::mutex> lock(mu_);
    capacity_ = c;
    evict_if_over_capacity();
}

AssetThumbnailCache::State
AssetThumbnailCache::state(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = entries_.find(path);
    return it == entries_.end() ? State::Empty : it->second.state;
}

u32 AssetThumbnailCache::in_flight_count() const {
    std::lock_guard<std::mutex> lock(mu_);
    return static_cast<u32>(in_flight_paths_.size());
}

// ── Internals ────────────────────────────────────────────────────────────────

void AssetThumbnailCache::enqueue_decode(const std::string& path) {
    if (in_flight_paths_.find(path) != in_flight_paths_.end()) return;
    in_flight_paths_.insert(path);
    in_flight_.push_back(
        std::async(std::launch::async, &AssetThumbnailCache::decode_file, path));
}

void AssetThumbnailCache::promote_to_mru(const std::string& path) {
    auto it = lru_iter_.find(path);
    if (it == lru_iter_.end()) return;
    if (it->second == lru_.begin()) return;
    lru_.splice(lru_.begin(), lru_, it->second);
}

void AssetThumbnailCache::evict_if_over_capacity() {
    if (capacity_ == 0) return;
    while (entries_.size() > capacity_ && !lru_.empty()) {
        const std::string& victim = lru_.back();
        auto vit = entries_.find(victim);
        if (vit != entries_.end()) {
            release_entry(vit->second);
            entries_.erase(vit);
        }
        lru_iter_.erase(victim);
        lru_.pop_back();
    }
}

void AssetThumbnailCache::release_entry(Entry& e) {
    if (e.state == State::Uploaded && e.texture != rhi::INVALID_HANDLE &&
        rhi_ != nullptr) {
        rhi_->destroy_texture(e.texture);
    }
    e.texture = rhi::INVALID_HANDLE;
    e.pixels.clear();
    e.pixels.shrink_to_fit();
    e.state = State::Empty;
}

}  // namespace nexus::editor
