#pragma once

#include <nexus/core/types.h>
#include <nexus/core/math.h>

#include <functional>
#include <string>
#include <vector>

namespace nexus {

// ---------------------------------------------------------------------------
// LODLevel — one level of detail
// ---------------------------------------------------------------------------
struct LODLevel {
    u32 level         = 0;
    f32 max_distance  = 0.0f;   // switch to next level beyond this distance
    u32 triangle_count = 0;     // for stats
    u32 mesh_id       = 0;      // user-defined mesh reference
};

// ---------------------------------------------------------------------------
// LODGroup — a set of LOD levels for one model
// ---------------------------------------------------------------------------
struct LODGroup {
    u32 id = 0;
    std::string name;
    std::vector<LODLevel> levels;   // sorted by max_distance ascending
    f32 cull_distance = 0.0f;       // beyond this, don't render at all

    /// Select the appropriate LOD level for the given distance.
    /// Returns nullptr if the object should be culled.
    [[nodiscard]] const LODLevel* select(f32 distance) const;

    /// Add a level. Levels are kept sorted by max_distance.
    void add_level(f32 max_dist, u32 tri_count, u32 mesh_id);
};

// ---------------------------------------------------------------------------
// LODSelection — result of LOD evaluation
// ---------------------------------------------------------------------------
struct LODSelection {
    u32 group_id    = 0;
    u32 lod_level   = 0;
    u32 mesh_id     = 0;
    f32 distance    = 0.0f;
    bool culled     = false;
};

// ---------------------------------------------------------------------------
// LODEvaluator — evaluate LOD for a batch of objects
// ---------------------------------------------------------------------------
class LODEvaluator {
public:
    LODEvaluator() = default;

    /// Set a global LOD bias (multiplier on distance thresholds).
    /// < 1.0 = higher quality, > 1.0 = lower quality.
    void set_bias(f32 bias) { bias_ = bias; }
    [[nodiscard]] f32 bias() const { return bias_; }

    /// Register a LOD group. Returns the group id.
    u32 register_group(LODGroup group);

    /// Find a group by id.
    [[nodiscard]] const LODGroup* find_group(u32 id) const;

    /// Evaluate LOD for a set of (group_id, world_position) pairs.
    [[nodiscard]] std::vector<LODSelection> evaluate(
        const Vec3& camera_pos,
        const std::vector<std::pair<u32, Vec3>>& objects) const;

    /// Total registered groups.
    [[nodiscard]] u32 group_count() const { return static_cast<u32>(groups_.size()); }

private:
    f32 bias_ = 1.0f;
    std::vector<LODGroup> groups_;
};

} // namespace nexus
