#include "nexus/editor/animation_asset.h"

#include "nexus/assets/asset_registry.h"
#include "nexus/core/log.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

namespace nexus::editor {

namespace {

void ensure_registered(nexus::assets::AssetRegistry* reg,
                       const std::string& path) {
    if (!reg) return;
    if (reg->find_by_path(path)) return;
    reg->register_asset(path, path, nexus::assets::AssetType::Animation);
}

// JSON ↔ AnimationClip ──────────────────────────────────────────────────────
//
// Schema (canonical):
// {
//   "name": "...",
//   "duration": 0.0,
//   "channels": [
//     { "bone": 0,
//       "positions": [ {"t": 0.0, "v": [x,y,z]}, ... ],
//       "rotations": [ {"t": 0.0, "v": [x,y,z,w]}, ... ],
//       "scales":    [ {"t": 0.0, "v": [x,y,z]}, ... ]
//     },
//     ...
//   ],
//   "events": [
//     { "t": 0.0, "name": "..." }, ...
//   ]
// }
nlohmann::json clip_to_json(const anim::AnimationClip& clip) {
    nlohmann::json j;
    j["name"]     = clip.name();
    j["duration"] = clip.duration();

    nlohmann::json channels = nlohmann::json::array();
    for (const auto& ch : clip.channels()) {
        nlohmann::json jc;
        jc["bone"] = ch.bone_index;
        nlohmann::json pos = nlohmann::json::array();
        for (const auto& k : ch.positions) {
            pos.push_back({{"t", k.time},
                            {"v", {k.value.x, k.value.y, k.value.z}}});
        }
        nlohmann::json rot = nlohmann::json::array();
        for (const auto& k : ch.rotations) {
            rot.push_back({{"t", k.time},
                            {"v", {k.value.x, k.value.y,
                                   k.value.z, k.value.w}}});
        }
        nlohmann::json scl = nlohmann::json::array();
        for (const auto& k : ch.scales) {
            scl.push_back({{"t", k.time},
                            {"v", {k.value.x, k.value.y, k.value.z}}});
        }
        jc["positions"] = std::move(pos);
        jc["rotations"] = std::move(rot);
        jc["scales"]    = std::move(scl);
        channels.push_back(std::move(jc));
    }
    j["channels"] = std::move(channels);

    nlohmann::json events = nlohmann::json::array();
    for (const auto& ev : clip.events()) {
        events.push_back({{"t", ev.time}, {"name", ev.name}});
    }
    j["events"] = std::move(events);
    return j;
}

// Returns a heap-allocated AnimationClip on success; nullptr on parse error.
std::unique_ptr<anim::AnimationClip>
clip_from_json(const std::string& source, const std::string& path_for_err) {
    try {
        auto j = nlohmann::json::parse(source);
        const std::string name = j.value("name", "");
        const f32 duration     = j.value("duration", 0.0f);

        auto clip = std::make_unique<anim::AnimationClip>(name, duration);

        if (j.contains("channels") && j["channels"].is_array()) {
            for (const auto& jc : j["channels"]) {
                anim::BoneChannel ch;
                ch.bone_index = jc.value("bone", -1);

                auto take_keys = [](const nlohmann::json& jk, auto& dest,
                                    int v_count) {
                    if (!jk.is_array()) return;
                    for (const auto& kf : jk) {
                        const auto& v = kf["v"];
                        if (!v.is_array() ||
                            static_cast<int>(v.size()) != v_count) {
                            continue;
                        }
                        typename std::remove_reference<decltype(dest)>::type::value_type k{};
                        k.time = kf.value("t", 0.0f);
                        if constexpr (std::is_same_v<decltype(k.value), Vec3>) {
                            k.value = Vec3(v[0].get<f32>(), v[1].get<f32>(),
                                           v[2].get<f32>());
                        } else {
                            // Quat — glm constructor is (w, x, y, z).
                            k.value = Quat(v[3].get<f32>(), v[0].get<f32>(),
                                           v[1].get<f32>(), v[2].get<f32>());
                        }
                        dest.push_back(k);
                    }
                };
                if (jc.contains("positions")) take_keys(jc["positions"], ch.positions, 3);
                if (jc.contains("rotations")) take_keys(jc["rotations"], ch.rotations, 4);
                if (jc.contains("scales"))    take_keys(jc["scales"],    ch.scales,    3);
                clip->add_channel(std::move(ch));
            }
        }

        if (j.contains("events") && j["events"].is_array()) {
            for (const auto& je : j["events"]) {
                clip->add_event(je.value("t", 0.0f),
                                je.value("name", std::string{}));
            }
        }
        return clip;
    } catch (const std::exception& e) {
        NX_ERROR("AnimationAssetCache: JSON parse failed for '{}': {}",
                 path_for_err, e.what());
        return nullptr;
    }
}

}  // namespace

const anim::AnimationClip*
AnimationAssetCache::load(const std::string& path) {
    if (path.empty()) return nullptr;

    if (auto it = clips_.find(path); it != clips_.end()) {
        return it->second.get();
    }

    std::ifstream in(path);
    if (!in) {
        NX_ERROR("AnimationAssetCache: failed to open '{}'", path);
        return nullptr;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    auto clip = clip_from_json(ss.str(), path);
    if (!clip) return nullptr;

    auto* raw = clip.get();
    clips_[path] = std::move(clip);
    if (path_to_id_.find(path) == path_to_id_.end()) {
        const u32 id = next_id_++;
        path_to_id_[path] = id;
        id_to_path_[id]   = path;
    }
    ensure_registered(registry_, path);
    return raw;
}

bool AnimationAssetCache::save(const std::string& path,
                                const anim::AnimationClip& clip) {
    if (path.empty()) return false;
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);

    nlohmann::json j = clip_to_json(clip);
    std::ofstream out(path);
    if (!out) {
        NX_ERROR("AnimationAssetCache: failed to open '{}' for writing", path);
        return false;
    }
    out << j.dump(2);
    if (!out) {
        NX_ERROR("AnimationAssetCache: write failed for '{}'", path);
        return false;
    }

    // Cache the just-written data so subsequent load() is free.  Build a
    // fresh AnimationClip via the same JSON path so save+load are exactly
    // equivalent (no accidental in-memory drift).
    auto roundtripped = clip_from_json(j.dump(), path);
    if (roundtripped) {
        clips_[path] = std::move(roundtripped);
    }
    if (path_to_id_.find(path) == path_to_id_.end()) {
        const u32 id = next_id_++;
        path_to_id_[path] = id;
        id_to_path_[id]   = path;
    }
    ensure_registered(registry_, path);
    return true;
}

u32 AnimationAssetCache::id_for(const std::string& path) {
    if (path.empty()) return 0u;
    if (auto it = path_to_id_.find(path); it != path_to_id_.end()) {
        return it->second;
    }
    const u32 id = next_id_++;
    path_to_id_[path] = id;
    id_to_path_[id]   = path;
    return id;
}

std::string AnimationAssetCache::path_for(u32 id) const {
    auto it = id_to_path_.find(id);
    return it == id_to_path_.end() ? std::string{} : it->second;
}

const anim::AnimationClip*
AnimationAssetCache::get(const std::string& path) const {
    auto it = clips_.find(path);
    return it == clips_.end() ? nullptr : it->second.get();
}

const anim::AnimationClip*
AnimationAssetCache::get_by_id(u32 id) const {
    auto it = id_to_path_.find(id);
    if (it == id_to_path_.end()) return nullptr;
    return get(it->second);
}

anim::AnimationClip*
AnimationAssetCache::get_by_id_mutable(u32 id) {
    auto it = id_to_path_.find(id);
    if (it == id_to_path_.end()) return nullptr;
    auto cit = clips_.find(it->second);
    return cit == clips_.end() ? nullptr : cit->second.get();
}

}  // namespace nexus::editor
