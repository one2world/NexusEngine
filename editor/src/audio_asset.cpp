#include "nexus/editor/audio_asset.h"

#include "nexus/assets/asset_registry.h"
#include "nexus/audio/audio_engine.h"
#include "nexus/core/log.h"

#include <filesystem>

namespace nexus::editor {

namespace {

void ensure_registered(nexus::assets::AssetRegistry* reg,
                       const std::string& path) {
    if (!reg) return;
    if (reg->find_by_path(path)) return;
    reg->register_asset(path, path, nexus::assets::AssetType::Audio);
}

}  // namespace

AudioClipId AudioAssetCache::import(const std::string& path) {
    if (path.empty()) return INVALID_CLIP_ID;
    if (auto it = path_to_id_.find(path); it != path_to_id_.end()) {
        if (it->second != INVALID_CLIP_ID) return it->second;
        // Previous import failed; let it retry below since the file may
        // have appeared on disk in the meantime.
    }
    if (!engine_) {
        NX_WARN("AudioAssetCache: no AudioEngine bound, can't import '{}'", path);
        return INVALID_CLIP_ID;
    }

    namespace fs = std::filesystem;
    const std::string name = fs::path(path).stem().string();
    const AudioClipId id = engine_->load_clip(name, path);
    if (id == INVALID_CLIP_ID) {
        NX_ERROR("AudioAssetCache: AudioEngine rejected '{}'", path);
        path_to_id_[path] = INVALID_CLIP_ID;  // remember failure to suppress retries
        return INVALID_CLIP_ID;
    }

    path_to_id_[path] = id;
    id_to_path_[id]   = path;
    ensure_registered(registry_, path);
    NX_INFO("AudioAssetCache: imported '{}' as clip {}", path, id);
    return id;
}

AudioClipId AudioAssetCache::id_for(const std::string& path) const {
    if (path.empty()) return INVALID_CLIP_ID;
    auto it = path_to_id_.find(path);
    return it == path_to_id_.end() ? INVALID_CLIP_ID : it->second;
}

std::string AudioAssetCache::path_for(AudioClipId id) const {
    if (id == INVALID_CLIP_ID) return {};
    auto it = id_to_path_.find(id);
    return it == id_to_path_.end() ? std::string{} : it->second;
}

}  // namespace nexus::editor
