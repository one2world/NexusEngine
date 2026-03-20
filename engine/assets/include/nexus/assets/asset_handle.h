#pragma once

#include "nexus/core/types.h"
#include "nexus/core/string_id.h"
#include <string>
#include <atomic>
#include <memory>
#include <typeindex>

namespace nexus::assets {

// ─────────────────────────────────────────────────────────────────────────────
// AssetType — identifies the kind of asset
// ─────────────────────────────────────────────────────────────────────────────

enum class AssetType : u8 {
    Unknown = 0,
    Texture,
    Mesh,
    Material,
    Shader,
    Audio,
    Font,
    Scene,
    Prefab,
    Script,
    Animation,
    Count
};

const char* asset_type_name(AssetType type);
AssetType asset_type_from_extension(const std::string& ext);

// ─────────────────────────────────────────────────────────────────────────────
// AssetId — uniquely identifies an asset by path hash
// ─────────────────────────────────────────────────────────────────────────────

struct AssetId {
    u64 value{0};

    AssetId() = default;
    explicit AssetId(u64 v) : value(v) {}
    explicit AssetId(const std::string& path);

    bool valid() const { return value != 0; }
    bool operator==(const AssetId& o) const { return value == o.value; }
    bool operator!=(const AssetId& o) const { return value != o.value; }
    bool operator<(const AssetId& o) const { return value < o.value; }
};

// ─────────────────────────────────────────────────────────────────────────────
// AssetMeta — metadata about an asset
// ─────────────────────────────────────────────────────────────────────────────

enum class AssetStatus : u8 {
    Unloaded,
    Loading,
    Loaded,
    Failed
};

struct AssetMeta {
    AssetId id;
    AssetType type{AssetType::Unknown};
    std::string path;             // Virtual path (e.g., "textures/player.png")
    std::string source_path;      // Filesystem path
    AssetStatus status{AssetStatus::Unloaded};
    u64 file_size{0};
    u64 last_modified{0};
    std::atomic<u32> ref_count{0};

    AssetMeta() = default;
    AssetMeta(const AssetMeta& o)
        : id(o.id), type(o.type), path(o.path), source_path(o.source_path),
          status(o.status), file_size(o.file_size), last_modified(o.last_modified),
          ref_count(o.ref_count.load()) {}
    AssetMeta& operator=(const AssetMeta& o) {
        if (this != &o) {
            id = o.id;
            type = o.type;
            path = o.path;
            source_path = o.source_path;
            status = o.status;
            file_size = o.file_size;
            last_modified = o.last_modified;
            ref_count.store(o.ref_count.load());
        }
        return *this;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// AssetData — base class for loaded asset data
// ─────────────────────────────────────────────────────────────────────────────

class AssetData {
public:
    virtual ~AssetData() = default;
    virtual AssetType type() const = 0;
    virtual u64 memory_usage() const { return 0; }
};

// ─────────────────────────────────────────────────────────────────────────────
// Concrete asset data types
// ─────────────────────────────────────────────────────────────────────────────

struct TextureData : AssetData {
    std::vector<u8> pixels;
    u32 width{0};
    u32 height{0};
    u32 channels{0};
    bool is_hdr{false};

    AssetType type() const override { return AssetType::Texture; }
    u64 memory_usage() const override { return pixels.size(); }
};

struct MeshData : AssetData {
    struct Vertex {
        f32 position[3]{};
        f32 normal[3]{};
        f32 texcoord[2]{};
        f32 tangent[4]{};
    };

    std::vector<Vertex> vertices;
    std::vector<u32> indices;
    std::string name;

    AssetType type() const override { return AssetType::Mesh; }
    u64 memory_usage() const override {
        return vertices.size() * sizeof(Vertex) + indices.size() * sizeof(u32);
    }
};

struct AudioData : AssetData {
    std::vector<u8> samples;
    u32 sample_rate{44100};
    u32 channels{2};
    u32 bits_per_sample{16};
    f32 duration{0.0f};

    AssetType type() const override { return AssetType::Audio; }
    u64 memory_usage() const override { return samples.size(); }
};

struct ShaderData : AssetData {
    std::string vertex_source;
    std::string fragment_source;
    std::string compute_source;

    AssetType type() const override { return AssetType::Shader; }
    u64 memory_usage() const override {
        return vertex_source.size() + fragment_source.size() + compute_source.size();
    }
};

struct ScriptData : AssetData {
    std::string source;

    AssetType type() const override { return AssetType::Script; }
    u64 memory_usage() const override { return source.size(); }
};

struct MaterialData : AssetData {
    std::string shader_path;
    std::string albedo_texture;
    std::string normal_texture;
    std::string metallic_roughness_texture;
    f32 metallic{0.0f};
    f32 roughness{1.0f};
    f32 color[4]{1.0f, 1.0f, 1.0f, 1.0f};

    AssetType type() const override { return AssetType::Material; }
    u64 memory_usage() const override { return sizeof(*this); }
};

struct AnimationData : AssetData {
    struct Keyframe {
        f32 time{0.0f};
        f32 value[4]{};    // up to vec4
        u32 components{1}; // 1=scalar, 3=vec3, 4=quat
    };

    struct Channel {
        std::string target_node;
        std::string property; // "translation", "rotation", "scale"
        std::vector<Keyframe> keyframes;
    };

    std::string name;
    f32 duration{0.0f};
    std::vector<Channel> channels;

    AssetType type() const override { return AssetType::Animation; }
    u64 memory_usage() const override {
        u64 size = sizeof(*this);
        for (auto& ch : channels) {
            size += ch.keyframes.size() * sizeof(Keyframe);
        }
        return size;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// AssetHandle<T> — type-safe reference-counted handle to an asset
// ─────────────────────────────────────────────────────────────────────────────

template <typename T>
class AssetHandle {
    static_assert(std::is_base_of_v<AssetData, T>,
                  "AssetHandle<T> requires T derived from AssetData");
public:
    AssetHandle() = default;

    AssetHandle(AssetMeta* meta, std::shared_ptr<T> data)
        : meta_(meta), data_(std::move(data)) {
        if (meta_) meta_->ref_count.fetch_add(1);
    }

    AssetHandle(const AssetHandle& o)
        : meta_(o.meta_), data_(o.data_) {
        if (meta_) meta_->ref_count.fetch_add(1);
    }

    AssetHandle(AssetHandle&& o) noexcept
        : meta_(o.meta_), data_(std::move(o.data_)) {
        o.meta_ = nullptr;
    }

    AssetHandle& operator=(const AssetHandle& o) {
        if (this != &o) {
            release();
            meta_ = o.meta_;
            data_ = o.data_;
            if (meta_) meta_->ref_count.fetch_add(1);
        }
        return *this;
    }

    AssetHandle& operator=(AssetHandle&& o) noexcept {
        if (this != &o) {
            release();
            meta_ = o.meta_;
            data_ = std::move(o.data_);
            o.meta_ = nullptr;
        }
        return *this;
    }

    ~AssetHandle() { release(); }

    bool valid() const { return meta_ != nullptr && data_ != nullptr; }
    explicit operator bool() const { return valid(); }

    T* get() { return data_.get(); }
    const T* get() const { return data_.get(); }
    T* operator->() { return data_.get(); }
    const T* operator->() const { return data_.get(); }
    T& operator*() { return *data_; }
    const T& operator*() const { return *data_; }

    AssetId id() const { return meta_ ? meta_->id : AssetId{}; }
    const std::string& path() const {
        static const std::string empty;
        return meta_ ? meta_->path : empty;
    }
    AssetStatus status() const { return meta_ ? meta_->status : AssetStatus::Unloaded; }
    u32 ref_count() const { return meta_ ? meta_->ref_count.load() : 0; }

private:
    void release() {
        if (meta_) {
            meta_->ref_count.fetch_sub(1);
            meta_ = nullptr;
        }
        data_.reset();
    }

    AssetMeta* meta_{nullptr};
    std::shared_ptr<T> data_;
};

} // namespace nexus::assets

// std::hash for AssetId
template <>
struct std::hash<nexus::assets::AssetId> {
    std::size_t operator()(const nexus::assets::AssetId& id) const noexcept {
        return static_cast<std::size_t>(id.value);
    }
};
