#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include <vector>
#include <string>
#include <functional>

namespace nexus {

/// A single texel sample for lightmap baking.
struct LightmapTexel {
    Vec3 position{0.0f};
    Vec3 normal{0.0f};
    bool valid{false};
};

/// Configuration for lightmap baking.
struct LightmapConfig {
    u32 resolution{256};        // Lightmap texture resolution (square)
    u32 samples_per_texel{64};  // Hemisphere samples per texel
    u32 bounces{2};             // Number of light bounces
    float bias{0.001f};         // Ray offset to avoid self-intersection
    float intensity{1.0f};      // Global intensity multiplier
};

/// A directional light for the baker.
struct BakeLight {
    Vec3 direction{0.0f, -1.0f, 0.0f};
    Vec3 color{1.0f};
    float intensity{1.0f};
};

/// Result of a lightmap bake.
struct LightmapResult {
    u32 width{0};
    u32 height{0};
    std::vector<Vec3> pixels;   // HDR RGB per texel
    std::vector<u8> pixels_ldr; // LDR RGBA8 for export

    /// Convert HDR to LDR with basic tone mapping.
    void tonemap(float exposure = 1.0f);
};

/// Simple triangle for ray intersection.
struct BakeTriangle {
    Vec3 v0, v1, v2;
    Vec3 n0, n1, n2;
    Vec2 uv0, uv1, uv2;  // UV2 (lightmap UVs)
};

/// Mesh data for baking.
struct BakeMesh {
    std::vector<BakeTriangle> triangles;
    Mat4 transform{1.0f};
};

/// LightmapBaker — CPU-based lightmap baking with hemisphere sampling.
class LightmapBaker {
public:
    LightmapBaker() = default;

    /// Set bake configuration.
    void set_config(const LightmapConfig& config) { config_ = config; }
    const LightmapConfig& config() const { return config_; }

    /// Add a mesh to the bake scene.
    void add_mesh(const BakeMesh& mesh);

    /// Add a directional light.
    void add_light(const BakeLight& light);

    /// Clear all meshes and lights.
    void clear();

    /// Bake lightmap for a specific mesh index.
    /// Returns the baked lightmap result.
    LightmapResult bake(u32 mesh_index);

    /// Bake all meshes. Returns one result per mesh.
    std::vector<LightmapResult> bake_all();

    /// Progress callback: (current_texel, total_texels)
    using ProgressCallback = std::function<void(u32, u32)>;
    void set_progress_callback(ProgressCallback cb) { progress_cb_ = std::move(cb); }

    u32 mesh_count() const { return static_cast<u32>(meshes_.size()); }
    u32 light_count() const { return static_cast<u32>(lights_.size()); }

private:
    /// Rasterize triangle UV2 coordinates onto texel grid.
    void rasterize_triangle(const BakeTriangle& tri, const Mat4& transform,
                            std::vector<LightmapTexel>& texels, u32 resolution);

    /// Sample hemisphere with cosine-weighted distribution.
    Vec3 cosine_weighted_hemisphere(Vec3 normal, float u1, float u2);

    /// Simple ray-scene intersection (returns true if occluded).
    bool trace_ray(Vec3 origin, Vec3 direction, float max_dist) const;

    /// Compute direct lighting at a point.
    Vec3 compute_direct(Vec3 position, Vec3 normal) const;

    LightmapConfig config_;
    std::vector<BakeMesh> meshes_;
    std::vector<BakeLight> lights_;
    ProgressCallback progress_cb_;
};

} // namespace nexus
