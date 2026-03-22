#include "nexus/assets/asset_loader.h"
#include "nexus/core/log.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

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

// Parse image dimensions from file headers without a full decode library.
// Supports PNG, BMP, TGA headers.  Falls back to raw-pixel import for unknown
// formats (the pixel data is stored as-is for the RHI to upload).

static bool parse_png_header(const std::vector<u8>& raw, u32& w, u32& h) {
    // PNG IHDR: bytes 16-19 = width (big-endian), 20-23 = height
    if (raw.size() < 24) return false;
    if (raw[0] != 0x89 || raw[1] != 'P' || raw[2] != 'N' || raw[3] != 'G')
        return false;
    w = (u32(raw[16]) << 24) | (u32(raw[17]) << 16) |
        (u32(raw[18]) << 8)  |  u32(raw[19]);
    h = (u32(raw[20]) << 24) | (u32(raw[21]) << 16) |
        (u32(raw[22]) << 8)  |  u32(raw[23]);
    return w > 0 && h > 0;
}

static bool parse_bmp_header(const std::vector<u8>& raw, u32& w, u32& h, u32& ch) {
    if (raw.size() < 54) return false;
    if (raw[0] != 'B' || raw[1] != 'M') return false;
    auto read_u32_le = [&](size_t off) -> u32 {
        return u32(raw[off]) | (u32(raw[off+1]) << 8) |
               (u32(raw[off+2]) << 16) | (u32(raw[off+3]) << 24);
    };
    auto read_u16_le = [&](size_t off) -> u16 {
        return u16(raw[off]) | (u16(raw[off+1]) << 8);
    };
    w  = read_u32_le(18);
    h  = read_u32_le(22);
    ch = read_u16_le(28) / 8; // bits-per-pixel -> channels
    return w > 0 && h > 0;
}

static bool parse_tga_header(const std::vector<u8>& raw, u32& w, u32& h, u32& ch) {
    if (raw.size() < 18) return false;
    w  = u32(raw[12]) | (u32(raw[13]) << 8);
    h  = u32(raw[14]) | (u32(raw[15]) << 8);
    ch = raw[16] / 8;
    return w > 0 && h > 0;
}

std::shared_ptr<AssetData> TextureImporter::import(const std::string& path,
                                                     const AssetMeta& /*meta*/) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        NX_ERROR("TextureImporter: failed to open {}", path);
        return nullptr;
    }

    auto data = std::make_shared<TextureData>();
    data->pixels.assign(std::istreambuf_iterator<char>(file),
                        std::istreambuf_iterator<char>{});

    std::filesystem::path p(path);
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    data->is_hdr = (ext == ".hdr");
    data->channels = 4; // default

    // Try to read real dimensions from format headers
    bool parsed = false;
    if (ext == ".png") {
        parsed = parse_png_header(data->pixels, data->width, data->height);
    } else if (ext == ".bmp") {
        parsed = parse_bmp_header(data->pixels, data->width, data->height, data->channels);
    } else if (ext == ".tga") {
        parsed = parse_tga_header(data->pixels, data->width, data->height, data->channels);
    } else if (ext == ".jpg" || ext == ".jpeg") {
        // JPEG SOF0 parsing: search for 0xFF 0xC0 marker
        for (size_t i = 0; i + 9 < data->pixels.size(); ++i) {
            if (data->pixels[i] == 0xFF && data->pixels[i+1] == 0xC0) {
                data->height = (u32(data->pixels[i+5]) << 8) | u32(data->pixels[i+6]);
                data->width  = (u32(data->pixels[i+7]) << 8) | u32(data->pixels[i+8]);
                data->channels = data->pixels[i+9];
                parsed = true;
                break;
            }
        }
    }

    if (!parsed) {
        // Fallback: assume raw RGBA if we can infer dimensions
        u64 pixel_count = data->pixels.size() / 4;
        u32 side = static_cast<u32>(std::sqrt(static_cast<double>(pixel_count)));
        data->width  = (side > 0) ? side : 1;
        data->height = (side > 0) ? side : 1;
        data->channels = 4;
    }

    NX_INFO("TextureImporter: loaded '{}' ({}x{}, {}ch)",
            p.filename().string(), data->width, data->height, data->channels);
    return data;
}

// ── MeshImporter ────────────────────────────────────────────────────────────

std::vector<std::string> MeshImporter::supported_extensions() const {
    return {".gltf", ".glb", ".obj", ".fbx"};
}

std::shared_ptr<AssetData> MeshImporter::import(const std::string& path,
                                                  const AssetMeta& /*meta*/) {
    std::ifstream file(path);
    if (!file.is_open()) {
        NX_ERROR("MeshImporter: failed to open {}", path);
        return nullptr;
    }

    auto data = std::make_shared<MeshData>();
    std::filesystem::path p(path);
    data->name = p.stem().string();
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (ext == ".obj") {
        // Wavefront OBJ parser
        std::vector<std::array<f32, 3>> positions;
        std::vector<std::array<f32, 3>> normals;
        std::vector<std::array<f32, 2>> texcoords;

        // Map of "v/vt/vn" -> index for deduplication
        std::unordered_map<std::string, u32> vertex_map;

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream iss(line);
            std::string token;
            iss >> token;

            if (token == "v") {
                std::array<f32, 3> pos{};
                iss >> pos[0] >> pos[1] >> pos[2];
                positions.push_back(pos);
            } else if (token == "vn") {
                std::array<f32, 3> n{};
                iss >> n[0] >> n[1] >> n[2];
                normals.push_back(n);
            } else if (token == "vt") {
                std::array<f32, 2> uv{};
                iss >> uv[0] >> uv[1];
                texcoords.push_back(uv);
            } else if (token == "f") {
                // Parse face vertices (triangulate quads)
                std::vector<u32> face_indices;
                std::string face_token;
                while (iss >> face_token) {
                    auto it = vertex_map.find(face_token);
                    if (it != vertex_map.end()) {
                        face_indices.push_back(it->second);
                        continue;
                    }

                    MeshData::Vertex vert{};
                    // Parse v, v/vt, v/vt/vn, v//vn
                    int vi = 0, ti = 0, ni = 0;
                    if (std::sscanf(face_token.c_str(), "%d/%d/%d", &vi, &ti, &ni) == 3 ||
                        std::sscanf(face_token.c_str(), "%d//%d", &vi, &ni) == 2 ||
                        std::sscanf(face_token.c_str(), "%d/%d", &vi, &ti) == 2 ||
                        std::sscanf(face_token.c_str(), "%d", &vi) == 1) {

                        if (vi != 0) {
                            size_t idx = (vi > 0) ? size_t(vi - 1) : positions.size() + size_t(vi);
                            if (idx < positions.size()) {
                                vert.position[0] = positions[idx][0];
                                vert.position[1] = positions[idx][1];
                                vert.position[2] = positions[idx][2];
                            }
                        }
                        if (ni != 0) {
                            size_t idx = (ni > 0) ? size_t(ni - 1) : normals.size() + size_t(ni);
                            if (idx < normals.size()) {
                                vert.normal[0] = normals[idx][0];
                                vert.normal[1] = normals[idx][1];
                                vert.normal[2] = normals[idx][2];
                            }
                        }
                        if (ti != 0) {
                            size_t idx = (ti > 0) ? size_t(ti - 1) : texcoords.size() + size_t(ti);
                            if (idx < texcoords.size()) {
                                vert.texcoord[0] = texcoords[idx][0];
                                vert.texcoord[1] = texcoords[idx][1];
                            }
                        }
                    }

                    u32 new_idx = static_cast<u32>(data->vertices.size());
                    data->vertices.push_back(vert);
                    vertex_map[face_token] = new_idx;
                    face_indices.push_back(new_idx);
                }

                // Triangulate (fan triangulation for convex polygons)
                for (size_t i = 2; i < face_indices.size(); ++i) {
                    data->indices.push_back(face_indices[0]);
                    data->indices.push_back(face_indices[i - 1]);
                    data->indices.push_back(face_indices[i]);
                }
            }
        }

        // Generate flat normals if none were provided
        if (normals.empty() && data->indices.size() >= 3) {
            for (size_t i = 0; i + 2 < data->indices.size(); i += 3) {
                auto& v0 = data->vertices[data->indices[i]];
                auto& v1 = data->vertices[data->indices[i + 1]];
                auto& v2 = data->vertices[data->indices[i + 2]];

                f32 e1[3] = {v1.position[0] - v0.position[0],
                             v1.position[1] - v0.position[1],
                             v1.position[2] - v0.position[2]};
                f32 e2[3] = {v2.position[0] - v0.position[0],
                             v2.position[1] - v0.position[1],
                             v2.position[2] - v0.position[2]};
                f32 n[3] = {e1[1]*e2[2] - e1[2]*e2[1],
                            e1[2]*e2[0] - e1[0]*e2[2],
                            e1[0]*e2[1] - e1[1]*e2[0]};
                f32 len = std::sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
                if (len > 0.0f) { n[0] /= len; n[1] /= len; n[2] /= len; }

                for (int k = 0; k < 3; ++k) {
                    auto& v = data->vertices[data->indices[i + size_t(k)]];
                    v.normal[0] = n[0]; v.normal[1] = n[1]; v.normal[2] = n[2];
                }
            }
        }

        NX_INFO("MeshImporter: loaded OBJ '{}' ({} verts, {} tris)",
                data->name, data->vertices.size(), data->indices.size() / 3);
    } else if (ext == ".gltf" || ext == ".glb") {
        // Minimal glTF 2.0 JSON parser for mesh data
        // Reads the first mesh/primitive from a .gltf file (JSON-based)
        std::string json_str;
        u32 bin_offset = 0;
        std::vector<u8> glb_bin;

        if (ext == ".glb") {
            // GLB format: 12-byte header + JSON chunk + BIN chunk
            file.seekg(0, std::ios::end);
            size_t file_size = static_cast<size_t>(file.tellg());
            file.seekg(0);
            if (file_size < 20) {
                NX_ERROR("MeshImporter: GLB file too small");
                return nullptr;
            }
            u32 magic, version, length;
            file.read(reinterpret_cast<char*>(&magic), 4);
            file.read(reinterpret_cast<char*>(&version), 4);
            file.read(reinterpret_cast<char*>(&length), 4);
            if (magic != 0x46546C67) { // 'glTF'
                NX_ERROR("MeshImporter: invalid GLB magic");
                return nullptr;
            }
            // JSON chunk
            u32 json_len, json_type;
            file.read(reinterpret_cast<char*>(&json_len), 4);
            file.read(reinterpret_cast<char*>(&json_type), 4);
            json_str.resize(json_len);
            file.read(json_str.data(), json_len);
            // BIN chunk
            if (static_cast<size_t>(file.tellg()) + 8 <= file_size) {
                u32 bin_len, bin_type;
                file.read(reinterpret_cast<char*>(&bin_len), 4);
                file.read(reinterpret_cast<char*>(&bin_type), 4);
                glb_bin.resize(bin_len);
                file.read(reinterpret_cast<char*>(glb_bin.data()), bin_len);
            }
        } else {
            // Plain .gltf JSON
            std::ostringstream oss;
            oss << file.rdbuf();
            json_str = oss.str();
        }

        // Minimal JSON value extraction helpers
        auto find_array = [&](const std::string& json, const std::string& key) -> std::string {
            size_t pos = json.find("\"" + key + "\"");
            if (pos == std::string::npos) return "";
            pos = json.find('[', pos);
            if (pos == std::string::npos) return "";
            int depth = 0;
            size_t start = pos;
            for (size_t i = pos; i < json.size(); ++i) {
                if (json[i] == '[') depth++;
                else if (json[i] == ']') { depth--; if (depth == 0) return json.substr(start, i - start + 1); }
            }
            return "";
        };

        auto find_int = [&](const std::string& json, const std::string& key) -> i64 {
            size_t pos = json.find("\"" + key + "\"");
            if (pos == std::string::npos) return -1;
            pos = json.find(':', pos);
            if (pos == std::string::npos) return -1;
            pos++;
            while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
            return std::stoll(json.substr(pos));
        };

        auto find_object_at = [&](const std::string& arr, size_t start) -> std::pair<std::string, size_t> {
            size_t pos = arr.find('{', start);
            if (pos == std::string::npos) return {"", std::string::npos};
            int depth = 0;
            size_t s = pos;
            for (size_t i = pos; i < arr.size(); ++i) {
                if (arr[i] == '{') depth++;
                else if (arr[i] == '}') { depth--; if (depth == 0) return {arr.substr(s, i - s + 1), i + 1}; }
            }
            return {"", std::string::npos};
        };

        // Parse accessors, bufferViews
        std::string accessors_arr = find_array(json_str, "accessors");
        std::string views_arr = find_array(json_str, "bufferViews");
        std::string meshes_arr = find_array(json_str, "meshes");

        struct AccessorInfo { i64 view; i64 count; i64 comp_type; std::string type; i64 byte_offset; };
        struct BufferViewInfo { i64 buffer; i64 offset; i64 length; i64 stride; };

        std::vector<AccessorInfo> accessors;
        std::vector<BufferViewInfo> buffer_views;

        // Parse buffer views
        {
            size_t pos = 0;
            while (true) {
                auto [obj, next] = find_object_at(views_arr, pos);
                if (next == std::string::npos) break;
                BufferViewInfo bv{};
                bv.buffer = find_int(obj, "buffer");
                bv.offset = find_int(obj, "byteOffset");
                if (bv.offset < 0) bv.offset = 0;
                bv.length = find_int(obj, "byteLength");
                bv.stride = find_int(obj, "byteStride");
                if (bv.stride < 0) bv.stride = 0;
                buffer_views.push_back(bv);
                pos = next;
            }
        }

        // Parse accessors
        {
            size_t pos = 0;
            while (true) {
                auto [obj, next] = find_object_at(accessors_arr, pos);
                if (next == std::string::npos) break;
                AccessorInfo acc{};
                acc.view = find_int(obj, "bufferView");
                acc.count = find_int(obj, "count");
                acc.comp_type = find_int(obj, "componentType");
                acc.byte_offset = find_int(obj, "byteOffset");
                if (acc.byte_offset < 0) acc.byte_offset = 0;
                // Determine type
                size_t tp = obj.find("\"type\"");
                if (tp != std::string::npos) {
                    size_t q1 = obj.find('"', tp + 6);
                    if (q1 != std::string::npos) {
                        size_t q2 = obj.find('"', q1 + 1);
                        if (q2 != std::string::npos) acc.type = obj.substr(q1 + 1, q2 - q1 - 1);
                    }
                }
                accessors.push_back(acc);
                pos = next;
            }
        }

        // Parse first mesh/primitive
        auto [mesh_obj, _m] = find_object_at(meshes_arr, 0);
        std::string prims_arr = find_array(mesh_obj, "primitives");
        auto [prim_obj, _p] = find_object_at(prims_arr, 0);

        i64 idx_accessor = find_int(prim_obj, "indices");
        // Find POSITION, NORMAL, TEXCOORD_0 in attributes
        std::string attrs_str;
        size_t attr_pos = prim_obj.find("\"attributes\"");
        if (attr_pos != std::string::npos) {
            auto [attr_obj, _a] = find_object_at(prim_obj, attr_pos);
            attrs_str = attr_obj;
        }
        i64 pos_accessor = find_int(attrs_str, "POSITION");
        i64 norm_accessor = find_int(attrs_str, "NORMAL");
        i64 uv_accessor = find_int(attrs_str, "TEXCOORD_0");

        // Helper to read float data from buffer
        auto read_floats = [&](i64 acc_idx, u32 components) -> std::vector<f32> {
            std::vector<f32> result;
            if (acc_idx < 0 || acc_idx >= static_cast<i64>(accessors.size())) return result;
            const auto& acc = accessors[acc_idx];
            if (acc.view < 0 || acc.view >= static_cast<i64>(buffer_views.size())) return result;
            const auto& bv = buffer_views[acc.view];

            const u8* buf_data = glb_bin.data();
            size_t buf_size = glb_bin.size();
            if (!buf_data || buf_size == 0) return result;

            size_t offset = static_cast<size_t>(bv.offset + acc.byte_offset);
            size_t stride = bv.stride > 0 ? static_cast<size_t>(bv.stride) : (components * sizeof(f32));

            result.reserve(static_cast<size_t>(acc.count) * components);
            for (i64 i = 0; i < acc.count; ++i) {
                size_t base = offset + static_cast<size_t>(i) * stride;
                for (u32 c = 0; c < components; ++c) {
                    f32 val = 0.0f;
                    if (base + (c + 1) * sizeof(f32) <= buf_size) {
                        std::memcpy(&val, buf_data + base + c * sizeof(f32), sizeof(f32));
                    }
                    result.push_back(val);
                }
            }
            return result;
        };

        auto read_indices = [&](i64 acc_idx) -> std::vector<u32> {
            std::vector<u32> result;
            if (acc_idx < 0 || acc_idx >= static_cast<i64>(accessors.size())) return result;
            const auto& acc = accessors[acc_idx];
            if (acc.view < 0 || acc.view >= static_cast<i64>(buffer_views.size())) return result;
            const auto& bv = buffer_views[acc.view];

            const u8* buf_data = glb_bin.data();
            size_t buf_size = glb_bin.size();
            if (!buf_data || buf_size == 0) return result;

            size_t offset = static_cast<size_t>(bv.offset + acc.byte_offset);
            result.reserve(static_cast<size_t>(acc.count));

            for (i64 i = 0; i < acc.count; ++i) {
                u32 idx = 0;
                if (acc.comp_type == 5123) { // UNSIGNED_SHORT
                    u16 v = 0;
                    size_t o = offset + static_cast<size_t>(i) * sizeof(u16);
                    if (o + sizeof(u16) <= buf_size) std::memcpy(&v, buf_data + o, sizeof(u16));
                    idx = v;
                } else if (acc.comp_type == 5125) { // UNSIGNED_INT
                    size_t o = offset + static_cast<size_t>(i) * sizeof(u32);
                    if (o + sizeof(u32) <= buf_size) std::memcpy(&idx, buf_data + o, sizeof(u32));
                } else if (acc.comp_type == 5121) { // UNSIGNED_BYTE
                    size_t o = offset + static_cast<size_t>(i);
                    if (o < buf_size) idx = buf_data[o];
                }
                result.push_back(idx);
            }
            return result;
        };

        // Read vertex data
        auto positions = read_floats(pos_accessor, 3);
        auto norms = read_floats(norm_accessor, 3);
        auto uvs = read_floats(uv_accessor, 2);
        auto indices = read_indices(idx_accessor);

        u32 vert_count = static_cast<u32>(positions.size() / 3);
        data->vertices.reserve(vert_count);
        for (u32 i = 0; i < vert_count; ++i) {
            MeshData::Vertex v{};
            v.position[0] = positions[i * 3];
            v.position[1] = positions[i * 3 + 1];
            v.position[2] = positions[i * 3 + 2];
            if (i * 3 + 2 < norms.size()) {
                v.normal[0] = norms[i * 3];
                v.normal[1] = norms[i * 3 + 1];
                v.normal[2] = norms[i * 3 + 2];
            }
            if (i * 2 + 1 < uvs.size()) {
                v.texcoord[0] = uvs[i * 2];
                v.texcoord[1] = uvs[i * 2 + 1];
            }
            data->vertices.push_back(v);
        }

        if (!indices.empty()) {
            data->indices = std::move(indices);
        } else {
            data->indices.resize(vert_count);
            for (u32 i = 0; i < vert_count; ++i) data->indices[i] = i;
        }

        NX_INFO("MeshImporter: loaded glTF '{}' ({} verts, {} tris)",
                data->name, data->vertices.size(), data->indices.size() / 3);
    } else {
        // FBX: placeholder - full FBX requires a dedicated library
        NX_WARN("MeshImporter: format '{}' not fully supported, creating placeholder", ext);

        MeshData::Vertex v{};
        v.normal[2] = 1.0f;
        v.position[0] = -0.5f; v.position[1] = -0.5f; v.position[2] = 0.0f;
        data->vertices.push_back(v);
        v.position[0] =  0.5f; v.texcoord[0] = 1.0f;
        data->vertices.push_back(v);
        v.position[1] =  0.5f; v.texcoord[1] = 1.0f;
        data->vertices.push_back(v);
        v.position[0] = -0.5f; v.texcoord[0] = 0.0f;
        data->vertices.push_back(v);

        data->indices = {0, 1, 2, 0, 2, 3};
    }

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
