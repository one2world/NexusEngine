#pragma once

#include "nexus/core/types.h"
#include "nexus/audio/audio_buffer.h"

#include <string>
#include <unordered_map>

namespace nexus::audio  { class AudioEngine; }
namespace nexus::assets { class AssetRegistry; }

namespace nexus::editor {

// Pull AudioClipId / INVALID_CLIP_ID into this scope so member declarations
// stay terse.  Pure type aliases; no runtime cost.
using audio::AudioClipId;
using audio::INVALID_CLIP_ID;

// ─────────────────────────────────────────────────────────────────────────────
// AudioAssetCache — editor-owned AudioClipId lookup by file path
// ─────────────────────────────────────────────────────────────────────────────
//
// Bridges drag-dropped audio files (.wav / .ogg) to AudioEngine's clip
// registry without leaking AudioEngine knowledge into the panel layer.
//
//   • `import(path)` calls AudioEngine::load_clip and stores the returned
//     AudioClipId by path.  Idempotent — re-import returns the cached id.
//   • `id_for(path)` is the same as `import(path)` but documents the
//     intent for callers binding `AudioSourceComponent::clip_id`.
//   • `path_for(id)` reverse lookup for Inspector display.
//
// Failure is exception-free: missing file / decode error logs and returns
// 0 (INVALID_CLIP_ID).  AudioEngine owns the bytes; this cache only owns
// the path → id mapping plus the AssetRegistry registration.
class AudioAssetCache {
public:
    AudioAssetCache() = default;
    ~AudioAssetCache() = default;

    AudioAssetCache(const AudioAssetCache&)            = delete;
    AudioAssetCache& operator=(const AudioAssetCache&) = delete;

    void set_audio_engine(audio::AudioEngine* eng)      { engine_ = eng; }
    void set_asset_registry(assets::AssetRegistry* reg) { registry_ = reg; }

    /// Load (or fetch from cache).  Returns 0 on failure.  Allocates a
    /// stable AudioClipId on the path's first appearance.
    AudioClipId import(const std::string& path);

    /// Stable id for a path; allocates without touching AudioEngine.  Use
    /// when you need the slot before the audio engine is available.
    /// import() will reconcile the value when called later.
    AudioClipId id_for(const std::string& path) const;

    /// Reverse lookup; "" for unknown ids.
    std::string path_for(AudioClipId id) const;

    /// Diagnostics.
    u32 size() const { return static_cast<u32>(path_to_id_.size()); }
    bool contains(const std::string& path) const {
        return path_to_id_.find(path) != path_to_id_.end();
    }

private:
    audio::AudioEngine*    engine_{nullptr};
    assets::AssetRegistry* registry_{nullptr};

    // path → AudioClipId (zero means cached failure or pending import).
    mutable std::unordered_map<std::string, AudioClipId> path_to_id_;
    mutable std::unordered_map<AudioClipId, std::string> id_to_path_;
};

}  // namespace nexus::editor
