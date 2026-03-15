#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include "nexus/rhi/rhi.h"
#include <vector>

namespace nexus {

// ─────────────────────────────────────────────────────────────────────────────
// Heightmap - stores height data for terrain generation
// ─────────────────────────────────────────────────────────────────────────────

class Heightmap {
public:
    void create(u32 width, u32 height, float default_height = 0.0f);
    void create_from_data(u32 width, u32 height, const float* data);

    float get(u32 x, u32 z) const;
    void  set(u32 x, u32 z, float value);

    /// Sample height with bilinear interpolation (normalized coords 0-1).
    float sample(float u, float v) const;

    /// Compute normal at a point via central differences.
    Vec3 normal_at(u32 x, u32 z, float cell_size) const;

    u32 width() const { return width_; }
    u32 height() const { return height_; }
    const std::vector<float>& data() const { return data_; }

private:
    u32 width_{0}, height_{0};
    std::vector<float> data_;
};

// ─────────────────────────────────────────────────────────────────────────────
// TerrainChunk - a single LOD-capable terrain patch
// ─────────────────────────────────────────────────────────────────────────────

struct TerrainChunk {
    rhi::BufferHandle vbo{rhi::INVALID_HANDLE};
    rhi::BufferHandle ibo{rhi::INVALID_HANDLE};
    u32 index_count{0};
    u32 lod{0};
    Vec3 center{0.0f};
    float size{0.0f};
};

// ─────────────────────────────────────────────────────────────────────────────
// TerrainRenderer - heightmap-based terrain with LOD
// ─────────────────────────────────────────────────────────────────────────────

class TerrainRenderer {
public:
    struct Config {
        float cell_size{1.0f};        // world units per heightmap cell
        float height_scale{50.0f};    // vertical scale multiplier
        u32   chunk_size{32};         // cells per chunk edge
        u32   max_lod{4};             // number of LOD levels
        float lod_distance{100.0f};   // distance for LOD transitions
    };

    struct SplatLayer {
        rhi::TextureHandle texture{rhi::INVALID_HANDLE};
        float uv_scale{1.0f};
    };

    void init(rhi::RHI* rhi, const Config& config);
    void shutdown();

    /// Load terrain from a heightmap.
    void load_heightmap(const Heightmap& heightmap);

    /// Set splatmap layers (up to 4).
    void set_splat_layers(const std::vector<SplatLayer>& layers);

    /// Get the height at a world XZ position.
    float height_at(float world_x, float world_z) const;

    /// Get the normal at a world XZ position.
    Vec3 normal_at(float world_x, float world_z) const;

    /// Render the terrain.
    void render(rhi::RHI* rhi, const Mat4& view_projection, Vec3 camera_pos);

    const Config& config() const { return config_; }
    u32 chunk_count() const { return static_cast<u32>(chunks_.size()); }

private:
    void generate_chunks();
    TerrainChunk generate_chunk(u32 start_x, u32 start_z, u32 lod);

    rhi::RHI* rhi_{nullptr};
    Config config_;
    Heightmap heightmap_;

    rhi::ShaderHandle   shader_{rhi::INVALID_HANDLE};
    rhi::PipelineHandle pipeline_{rhi::INVALID_HANDLE};

    std::vector<TerrainChunk> chunks_;
    std::vector<SplatLayer> splat_layers_;
};

} // namespace nexus
