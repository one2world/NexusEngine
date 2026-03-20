#include "nexus/assets/asset_handle.h"
#include <algorithm>

namespace nexus::assets {

// ── AssetType helpers ───────────────────────────────────────────────────────

const char* asset_type_name(AssetType type) {
    switch (type) {
        case AssetType::Texture:   return "Texture";
        case AssetType::Mesh:      return "Mesh";
        case AssetType::Material:  return "Material";
        case AssetType::Shader:    return "Shader";
        case AssetType::Audio:     return "Audio";
        case AssetType::Font:      return "Font";
        case AssetType::Scene:     return "Scene";
        case AssetType::Prefab:    return "Prefab";
        case AssetType::Script:    return "Script";
        case AssetType::Animation: return "Animation";
        default:                   return "Unknown";
    }
}

AssetType asset_type_from_extension(const std::string& ext) {
    std::string lower = ext;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // Textures
    if (lower == ".png" || lower == ".jpg" || lower == ".jpeg" ||
        lower == ".tga" || lower == ".bmp" || lower == ".hdr")
        return AssetType::Texture;

    // Meshes
    if (lower == ".gltf" || lower == ".glb" || lower == ".obj" || lower == ".fbx")
        return AssetType::Mesh;

    // Audio
    if (lower == ".wav" || lower == ".ogg" || lower == ".mp3" || lower == ".flac")
        return AssetType::Audio;

    // Shaders
    if (lower == ".glsl" || lower == ".vert" || lower == ".frag" || lower == ".comp" ||
        lower == ".spv")
        return AssetType::Shader;

    // Scripts
    if (lower == ".lua" || lower == ".nxs")
        return AssetType::Script;

    // Materials
    if (lower == ".mat" || lower == ".material")
        return AssetType::Material;

    // Animations
    if (lower == ".anim")
        return AssetType::Animation;

    // Scenes
    if (lower == ".scene" || lower == ".nxscene")
        return AssetType::Scene;

    // Prefabs
    if (lower == ".prefab")
        return AssetType::Prefab;

    // Fonts
    if (lower == ".ttf" || lower == ".otf")
        return AssetType::Font;

    return AssetType::Unknown;
}

// ── AssetId ─────────────────────────────────────────────────────────────────

AssetId::AssetId(const std::string& path) {
    // FNV-1a hash
    u64 hash = 14695981039346656037ULL;
    for (char c : path) {
        hash ^= static_cast<u64>(static_cast<unsigned char>(c));
        hash *= 1099511628211ULL;
    }
    value = hash;
}

} // namespace nexus::assets
