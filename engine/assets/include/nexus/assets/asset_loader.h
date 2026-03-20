#pragma once

#include "nexus/assets/asset_registry.h"
#include <future>
#include <queue>
#include <mutex>

namespace nexus::assets {

// ─────────────────────────────────────────────────────────────────────────────
// AssetImporter — interface for importing specific asset types
// ─────────────────────────────────────────────────────────────────────────────

class AssetImporter {
public:
    virtual ~AssetImporter() = default;

    /// Which asset type this importer handles.
    virtual AssetType handled_type() const = 0;

    /// Which file extensions this importer supports (e.g., ".png", ".jpg").
    virtual std::vector<std::string> supported_extensions() const = 0;

    /// Returns true if this importer can handle the given extension.
    bool supports(const std::string& extension) const;

    /// Import asset data from file. Returns nullptr on failure.
    virtual std::shared_ptr<AssetData> import(const std::string& path,
                                               const AssetMeta& meta) = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// Built-in importers
// ─────────────────────────────────────────────────────────────────────────────

class TextureImporter : public AssetImporter {
public:
    AssetType handled_type() const override { return AssetType::Texture; }
    std::vector<std::string> supported_extensions() const override;
    std::shared_ptr<AssetData> import(const std::string& path,
                                       const AssetMeta& meta) override;
};

class MeshImporter : public AssetImporter {
public:
    AssetType handled_type() const override { return AssetType::Mesh; }
    std::vector<std::string> supported_extensions() const override;
    std::shared_ptr<AssetData> import(const std::string& path,
                                       const AssetMeta& meta) override;
};

class AudioImporter : public AssetImporter {
public:
    AssetType handled_type() const override { return AssetType::Audio; }
    std::vector<std::string> supported_extensions() const override;
    std::shared_ptr<AssetData> import(const std::string& path,
                                       const AssetMeta& meta) override;
};

class ShaderImporter : public AssetImporter {
public:
    AssetType handled_type() const override { return AssetType::Shader; }
    std::vector<std::string> supported_extensions() const override;
    std::shared_ptr<AssetData> import(const std::string& path,
                                       const AssetMeta& meta) override;
};

class ScriptImporter : public AssetImporter {
public:
    AssetType handled_type() const override { return AssetType::Script; }
    std::vector<std::string> supported_extensions() const override;
    std::shared_ptr<AssetData> import(const std::string& path,
                                       const AssetMeta& meta) override;
};

class MaterialImporter : public AssetImporter {
public:
    AssetType handled_type() const override { return AssetType::Material; }
    std::vector<std::string> supported_extensions() const override;
    std::shared_ptr<AssetData> import(const std::string& path,
                                       const AssetMeta& meta) override;
};

// ─────────────────────────────────────────────────────────────────────────────
// LoadRequest — a queued load operation
// ─────────────────────────────────────────────────────────────────────────────

struct LoadRequest {
    AssetId id;
    u32 priority{0}; // Higher = load first

    bool operator<(const LoadRequest& o) const { return priority < o.priority; }
};

// ─────────────────────────────────────────────────────────────────────────────
// ProgressInfo — reported during async loading
// ─────────────────────────────────────────────────────────────────────────────

struct ProgressInfo {
    u32 total{0};
    u32 completed{0};
    u32 failed{0};
    AssetId current_asset;
    f32 fraction() const { return total > 0 ? static_cast<f32>(completed) / static_cast<f32>(total) : 1.0f; }
};

// ─────────────────────────────────────────────────────────────────────────────
// AssetLoader — loads assets synchronously or asynchronously
// ─────────────────────────────────────────────────────────────────────────────

class AssetLoader {
public:
    explicit AssetLoader(AssetRegistry& registry);
    ~AssetLoader();

    /// Register a custom importer. Takes ownership.
    void register_importer(std::unique_ptr<AssetImporter> importer);

    /// Register all built-in importers.
    void register_default_importers();

    /// Load an asset synchronously.
    bool load_sync(AssetId id);
    bool load_sync(const std::string& path);

    /// Queue an asset for async loading.
    void load_async(AssetId id, u32 priority = 0);
    void load_async(const std::string& path, u32 priority = 0);

    /// Process one item from the async queue. Returns true if work was done.
    bool process_one();

    /// Process all queued items.
    u32 process_all();

    /// Get async loading progress.
    ProgressInfo progress() const;

    /// Set progress callback.
    using ProgressCallback = std::function<void(const ProgressInfo&)>;
    void set_progress_callback(ProgressCallback cb) { progress_cb_ = std::move(cb); }

    /// Number of registered importers.
    u32 importer_count() const;

    /// Find the importer for a given file extension.
    AssetImporter* find_importer(const std::string& extension) const;

    /// Find the importer for a given asset type.
    AssetImporter* find_importer_for_type(AssetType type) const;

private:
    bool do_load(AssetId id);

    AssetRegistry& registry_;
    std::vector<std::unique_ptr<AssetImporter>> importers_;
    std::priority_queue<LoadRequest> load_queue_;
    ProgressInfo progress_;
    ProgressCallback progress_cb_;
};

} // namespace nexus::assets
