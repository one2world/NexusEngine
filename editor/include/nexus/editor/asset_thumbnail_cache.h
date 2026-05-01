#pragma once

#include "nexus/core/types.h"
#include "nexus/rhi/rhi.h"

#include <atomic>
#include <cstdint>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nexus::editor {

// ─────────────────────────────────────────────────────────────────────────────
// AssetThumbnailCache — async image decode + RHI upload + LRU eviction
// ─────────────────────────────────────────────────────────────────────────────
//
// Decodes image files (.png/.jpg/.tga/.bmp/.hdr) on background threads using
// stb_image, uploads the RGBA8 pixel buffer to an RHI texture on the main
// thread (RHI is not thread-safe), and exposes the resulting handle via
// `lookup()` so the AssetBrowserPanel can render real GPU thumbnails.
//
// Threading model:
//   - request()    main thread — enqueues a decode job, returns 0 immediately
//                  if no entry yet, or the cached handle if available.
//   - drain()      main thread — pulls completed decode jobs off the queue
//                  and uploads pixels to the RHI; called once per frame.
//   - decode worker pool runs in std::async — never touches RHI.
//
// LRU eviction:
//   - Configurable capacity (default 256 entries).
//   - lookup()/request() promote the entry to the MRU end.
//   - When capacity is exceeded, the LRU end is evicted and its texture
//     released via RHI::destroy_texture.
//
// Failure handling:
//   - stb_image decode failure → entry marked DecodeFailed; the panel still
//     renders the colored fallback tile.  Subsequent request() returns 0
//     and does NOT re-enqueue (avoids hammering broken files every frame).
//
// Test entry points avoid RHI: the cache exposes `inject_decoded_pixels()`
// so unit tests can drive the upload path without an OpenGL context.
class AssetThumbnailCache {
public:
    enum class State : u8 {
        Empty,         // no entry — request() will enqueue
        Pending,       // decode in flight
        Decoded,       // pixels ready, awaiting upload
        Uploaded,      // texture handle valid
        DecodeFailed,  // permanent failure; do not retry
    };

    struct Entry {
        State            state{State::Empty};
        u32              width{0};
        u32              height{0};
        rhi::TextureHandle texture{rhi::INVALID_HANDLE};
        // Pixel buffer is held only between decode completion and upload —
        // freed once the texture has been uploaded so the cache footprint
        // is bounded by GPU memory, not CPU memory.
        std::vector<u8>  pixels;
    };

    // capacity = 0 disables LRU eviction (used by tests).
    explicit AssetThumbnailCache(rhi::RHI* rhi = nullptr, u32 capacity = 256);
    ~AssetThumbnailCache();

    AssetThumbnailCache(const AssetThumbnailCache&)            = delete;
    AssetThumbnailCache& operator=(const AssetThumbnailCache&) = delete;

    // Late binding so editor_main can construct the cache before the RHI is
    // ready and wire it in once the GL context exists.
    void set_rhi(rhi::RHI* rhi) { rhi_ = rhi; }

    // Request a thumbnail for `path`.  Returns the texture handle if already
    // uploaded, otherwise enqueues a decode job and returns 0.  Permanent
    // failures (DecodeFailed) also return 0 without re-enqueueing.
    rhi::TextureHandle request(const std::string& path);

    // Look up without enqueueing.  Returns 0 if not Uploaded.
    rhi::TextureHandle lookup(const std::string& path) const;

    // Pump completed decode jobs and upload their pixels to the RHI.  Call
    // once per frame from the main thread.  Returns the number of textures
    // uploaded this call (useful for telemetry and tests).
    u32 drain();

    // Force-release: drops the texture and rewinds state to Empty.  Used by
    // explicit invalidation (e.g. file changed on disk) and shutdown.
    void invalidate(const std::string& path);

    // Clears every entry and releases textures.  Safe to call on shutdown.
    void clear();

    // Test hook — bypass async decode and directly inject decoded pixels for
    // a path.  Marks the entry Decoded so the next drain() upload-tests the
    // RHI path without filesystem I/O.
    void inject_decoded_pixels(const std::string& path,
                               u32 width, u32 height,
                               std::vector<u8> rgba8);

    // Test hook — short-circuit a request to DecodeFailed without spawning
    // a worker.  Verifies the broken-file backoff path.
    void mark_failed(const std::string& path);

    // Diagnostics.
    u32 size() const;
    u32 capacity() const { return capacity_; }
    void set_capacity(u32 c);
    State state(const std::string& path) const;
    u32 in_flight_count() const;

private:
    struct DecodeResult {
        std::string path;
        bool        ok{false};
        u32         width{0};
        u32         height{0};
        std::vector<u8> pixels;  // RGBA8, w*h*4 bytes
    };

    // Synchronous decode helper — runs on a worker thread.  Pure function:
    // input file → DecodeResult.  Exposed for test injection.
    static DecodeResult decode_file(std::string path);

    void enqueue_decode(const std::string& path);
    void promote_to_mru(const std::string& path);
    void evict_if_over_capacity();
    void release_entry(Entry& e);

    rhi::RHI* rhi_;
    u32       capacity_;

    mutable std::mutex                       mu_;
    std::unordered_map<std::string, Entry>   entries_;
    std::list<std::string>                   lru_;  // front = MRU, back = LRU
    std::unordered_map<std::string,
                       std::list<std::string>::iterator> lru_iter_;

    // In-flight decode futures.  Each future wraps DecodeResult; drain()
    // collects the ready ones each frame and uploads their pixels.
    std::vector<std::future<DecodeResult>> in_flight_;
    // Paths currently in flight, so request() doesn't re-enqueue.
    std::unordered_set<std::string>        in_flight_paths_;
};

}  // namespace nexus::editor
