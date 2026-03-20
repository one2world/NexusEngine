#include "nexus/assets/asset_loader.h"
#include "nexus/core/log.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace nexus::assets {

// ── AssetImporter base ──────────────────────────────────────────────────────

bool AssetImporter::supports(const std::string& extension) const {
    std::string lower = extension;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    auto exts = supported_extensions();
    return std::find(exts.begin(), exts.end(), lower) != exts.end();
}

// ── TextureImporter ─────────────────────────────────────────────────────────

std::vector<std::string> TextureImporter::supported_extensions() const {
    return {".png", ".jpg", ".jpeg", ".tga", ".bmp", ".hdr"};
}

std::shared_ptr<AssetData> TextureImporter::import(const std::string& path,
                                                     const AssetMeta& /*meta*/) {
    // Read raw file bytes (real engine would use stb_image here)
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        NX_ERROR("TextureImporter: failed to open {}", path);
        return nullptr;
    }

    auto data = std::make_shared<TextureData>();
    data->pixels.assign(std::istreambuf_iterator<char>(file),
                        std::istreambuf_iterator<char>{});

    // Placeholder dimensions — real importer would decode the image
    data->width = 1;
    data->height = 1;
    data->channels = 4;

    std::filesystem::path p(path);
    data->is_hdr = (p.extension() == ".hdr");

    return data;
}

// ── MeshImporter ────────────────────────────────────────────────────────────

std::vector<std::string> MeshImporter::supported_extensions() const {
    return {".gltf", ".glb", ".obj", ".fbx"};
}

std::shared_ptr<AssetData> MeshImporter::import(const std::string& path,
                                                  const AssetMeta& /*meta*/) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        NX_ERROR("MeshImporter: failed to open {}", path);
        return nullptr;
    }

    auto data = std::make_shared<MeshData>();
    std::filesystem::path p(path);
    data->name = p.stem().string();

    // Read raw bytes as placeholder (real engine would parse glTF/OBJ)
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>{});

    // Create a single-triangle placeholder mesh
    MeshData::Vertex v{};
    v.position[0] = 0.0f; v.position[1] = 0.0f; v.position[2] = 0.0f;
    v.normal[2] = 1.0f;
    data->vertices.push_back(v);
    v.position[0] = 1.0f;
    data->vertices.push_back(v);
    v.position[1] = 1.0f;
    data->vertices.push_back(v);

    data->indices = {0, 1, 2};

    return data;
}

// ── AudioImporter ───────────────────────────────────────────────────────────

std::vector<std::string> AudioImporter::supported_extensions() const {
    return {".wav", ".ogg", ".mp3", ".flac"};
}

std::shared_ptr<AssetData> AudioImporter::import(const std::string& path,
                                                   const AssetMeta& /*meta*/) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        NX_ERROR("AudioImporter: failed to open {}", path);
        return nullptr;
    }

    auto data = std::make_shared<AudioData>();
    data->samples.assign(std::istreambuf_iterator<char>(file),
                         std::istreambuf_iterator<char>{});
    data->sample_rate = 44100;
    data->channels = 2;
    data->bits_per_sample = 16;

    // Estimate duration from raw size
    if (data->channels > 0 && data->bits_per_sample > 0 && data->sample_rate > 0) {
        u64 bytes_per_sample = (data->bits_per_sample / 8) * data->channels;
        if (bytes_per_sample > 0) {
            data->duration = static_cast<f32>(data->samples.size()) /
                            static_cast<f32>(bytes_per_sample * data->sample_rate);
        }
    }

    return data;
}

// ── ShaderImporter ──────────────────────────────────────────────────────────

std::vector<std::string> ShaderImporter::supported_extensions() const {
    return {".glsl", ".vert", ".frag", ".comp", ".spv"};
}

std::shared_ptr<AssetData> ShaderImporter::import(const std::string& path,
                                                    const AssetMeta& /*meta*/) {
    std::ifstream file(path);
    if (!file.is_open()) {
        NX_ERROR("ShaderImporter: failed to open {}", path);
        return nullptr;
    }

    auto data = std::make_shared<ShaderData>();
    std::stringstream buf;
    buf << file.rdbuf();
    std::string source = buf.str();

    std::filesystem::path p(path);
    auto ext = p.extension().string();

    if (ext == ".vert") {
        data->vertex_source = source;
    } else if (ext == ".frag") {
        data->fragment_source = source;
    } else if (ext == ".comp") {
        data->compute_source = source;
    } else {
        // .glsl — treat as combined vertex+fragment
        data->vertex_source = source;
        data->fragment_source = source;
    }

    return data;
}

// ── ScriptImporter ──────────────────────────────────────────────────────────

std::vector<std::string> ScriptImporter::supported_extensions() const {
    return {".lua", ".nxs"};
}

std::shared_ptr<AssetData> ScriptImporter::import(const std::string& path,
                                                    const AssetMeta& /*meta*/) {
    std::ifstream file(path);
    if (!file.is_open()) {
        NX_ERROR("ScriptImporter: failed to open {}", path);
        return nullptr;
    }

    auto data = std::make_shared<ScriptData>();
    std::stringstream buf;
    buf << file.rdbuf();
    data->source = buf.str();

    return data;
}

// ── MaterialImporter ────────────────────────────────────────────────────────

std::vector<std::string> MaterialImporter::supported_extensions() const {
    return {".mat", ".material"};
}

std::shared_ptr<AssetData> MaterialImporter::import(const std::string& path,
                                                      const AssetMeta& /*meta*/) {
    std::ifstream file(path);
    if (!file.is_open()) {
        NX_ERROR("MaterialImporter: failed to open {}", path);
        return nullptr;
    }

    auto data = std::make_shared<MaterialData>();

    // Simple key-value parsing (real engine would use JSON)
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        // Trim spaces
        auto trim = [](std::string& s) {
            auto start = s.find_first_not_of(" \t");
            auto end = s.find_last_not_of(" \t");
            s = (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
        };
        trim(key);
        trim(val);

        if (key == "shader") data->shader_path = val;
        else if (key == "albedo") data->albedo_texture = val;
        else if (key == "normal") data->normal_texture = val;
        else if (key == "metallic_roughness") data->metallic_roughness_texture = val;
        else if (key == "metallic") data->metallic = std::stof(val);
        else if (key == "roughness") data->roughness = std::stof(val);
    }

    return data;
}

// ── AssetLoader ─────────────────────────────────────────────────────────────

AssetLoader::AssetLoader(AssetRegistry& registry) : registry_(registry) {}
AssetLoader::~AssetLoader() = default;

void AssetLoader::register_importer(std::unique_ptr<AssetImporter> importer) {
    importers_.push_back(std::move(importer));
}

void AssetLoader::register_default_importers() {
    register_importer(std::make_unique<TextureImporter>());
    register_importer(std::make_unique<MeshImporter>());
    register_importer(std::make_unique<AudioImporter>());
    register_importer(std::make_unique<ShaderImporter>());
    register_importer(std::make_unique<ScriptImporter>());
    register_importer(std::make_unique<MaterialImporter>());
}

bool AssetLoader::load_sync(AssetId id) {
    return do_load(id);
}

bool AssetLoader::load_sync(const std::string& path) {
    auto* meta = registry_.find_by_path(path);
    if (!meta) return false;
    return do_load(meta->id);
}

void AssetLoader::load_async(AssetId id, u32 priority) {
    load_queue_.push({id, priority});
    progress_.total++;
}

void AssetLoader::load_async(const std::string& path, u32 priority) {
    auto* meta = registry_.find_by_path(path);
    if (!meta) return;
    load_async(meta->id, priority);
}

bool AssetLoader::process_one() {
    if (load_queue_.empty()) return false;

    auto request = load_queue_.top();
    load_queue_.pop();

    progress_.current_asset = request.id;
    bool ok = do_load(request.id);

    if (ok) {
        progress_.completed++;
    } else {
        progress_.failed++;
    }

    if (progress_cb_) {
        progress_cb_(progress_);
    }

    return true;
}

u32 AssetLoader::process_all() {
    u32 processed = 0;
    while (process_one()) {
        ++processed;
    }
    return processed;
}

ProgressInfo AssetLoader::progress() const {
    return progress_;
}

u32 AssetLoader::importer_count() const {
    return static_cast<u32>(importers_.size());
}

AssetImporter* AssetLoader::find_importer(const std::string& extension) const {
    for (auto& imp : importers_) {
        if (imp->supports(extension)) return imp.get();
    }
    return nullptr;
}

AssetImporter* AssetLoader::find_importer_for_type(AssetType type) const {
    for (auto& imp : importers_) {
        if (imp->handled_type() == type) return imp.get();
    }
    return nullptr;
}

bool AssetLoader::do_load(AssetId id) {
    auto* meta = registry_.find(id);
    if (!meta) {
        NX_ERROR("AssetLoader: unknown asset id {}", id.value);
        return false;
    }

    if (meta->status == AssetStatus::Loaded) return true;

    meta->status = AssetStatus::Loading;

    // Find the right importer
    std::filesystem::path p(meta->source_path);
    std::string ext = p.has_extension() ? p.extension().string() : "";

    AssetImporter* importer = nullptr;
    if (!ext.empty()) {
        importer = find_importer(ext);
    }
    if (!importer && meta->type != AssetType::Unknown) {
        importer = find_importer_for_type(meta->type);
    }

    if (!importer) {
        NX_ERROR("AssetLoader: no importer for '{}' ({})", meta->path, ext);
        meta->status = AssetStatus::Failed;
        return false;
    }

    auto data = importer->import(meta->source_path, *meta);
    if (!data) {
        meta->status = AssetStatus::Failed;
        return false;
    }

    registry_.store_data(id, std::move(data));
    return true;
}

} // namespace nexus::assets
