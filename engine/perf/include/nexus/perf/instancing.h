#pragma once

#include <nexus/core/types.h>
#include <nexus/core/math.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace nexus {

// ---------------------------------------------------------------------------
// InstanceData — per-instance transform + optional user data
// ---------------------------------------------------------------------------
struct InstanceData {
    Mat4 transform{1.0f};
    Vec4 color{1.0f};       // optional per-instance tint
    u32  user_data = 0;     // user-defined (e.g. material index)
};

// ---------------------------------------------------------------------------
// InstanceBatch — a group of instances sharing the same mesh + material
// ---------------------------------------------------------------------------
struct InstanceBatch {
    u32 mesh_id     = 0;
    u32 material_id = 0;
    std::vector<InstanceData> instances;

    /// Add an instance.
    void add(const Mat4& transform, const Vec4& color = Vec4{1.0f}, u32 user_data = 0);

    /// Remove all instances.
    void clear();

    /// Number of instances.
    [[nodiscard]] u32 count() const { return static_cast<u32>(instances.size()); }
};

// ---------------------------------------------------------------------------
// BatchKey — uniquely identifies a batch by mesh + material
// ---------------------------------------------------------------------------
struct BatchKey {
    u32 mesh_id     = 0;
    u32 material_id = 0;

    bool operator==(const BatchKey& o) const {
        return mesh_id == o.mesh_id && material_id == o.material_id;
    }
};

struct BatchKeyHash {
    std::size_t operator()(const BatchKey& k) const {
        return std::hash<u64>{}((static_cast<u64>(k.mesh_id) << 32) | k.material_id);
    }
};

// ---------------------------------------------------------------------------
// InstanceManager — collects objects, auto-batches by mesh+material
// ---------------------------------------------------------------------------
class InstanceManager {
public:
    InstanceManager() = default;

    /// Submit an instance for batching.
    void submit(u32 mesh_id, u32 material_id, const Mat4& transform,
                const Vec4& color = Vec4{1.0f}, u32 user_data = 0);

    /// Flush / clear all batches for the next frame.
    void clear();

    /// Get all current batches.
    [[nodiscard]] const std::unordered_map<BatchKey, InstanceBatch, BatchKeyHash>&
    batches() const { return batches_; }

    /// Total number of instances across all batches.
    [[nodiscard]] u32 total_instances() const;

    /// Total number of batches (= draw calls for instanced rendering).
    [[nodiscard]] u32 batch_count() const { return static_cast<u32>(batches_.size()); }

    /// Minimum instance count to actually use instanced rendering.
    void set_instance_threshold(u32 t) { instance_threshold_ = t; }
    [[nodiscard]] u32 instance_threshold() const { return instance_threshold_; }

    /// Split batches into instanced (>= threshold) and non-instanced.
    struct SplitResult {
        std::vector<const InstanceBatch*> instanced;
        std::vector<const InstanceBatch*> individual;
    };
    [[nodiscard]] SplitResult split() const;

private:
    std::unordered_map<BatchKey, InstanceBatch, BatchKeyHash> batches_;
    u32 instance_threshold_ = 2;
};

} // namespace nexus
