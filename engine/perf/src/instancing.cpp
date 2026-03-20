#include "nexus/perf/instancing.h"

namespace nexus {

// ---------------------------------------------------------------------------
// InstanceBatch
// ---------------------------------------------------------------------------

void InstanceBatch::add(const Mat4& transform, const Vec4& color, u32 user_data) {
    instances.push_back({transform, color, user_data});
}

void InstanceBatch::clear() {
    instances.clear();
}

// ---------------------------------------------------------------------------
// InstanceManager
// ---------------------------------------------------------------------------

void InstanceManager::submit(u32 mesh_id, u32 material_id, const Mat4& transform,
                              const Vec4& color, u32 user_data) {
    BatchKey key{mesh_id, material_id};
    auto& batch = batches_[key];
    batch.mesh_id     = mesh_id;
    batch.material_id = material_id;
    batch.add(transform, color, user_data);
}

void InstanceManager::clear() {
    batches_.clear();
}

u32 InstanceManager::total_instances() const {
    u32 total = 0;
    for (const auto& [_, b] : batches_) {
        total += b.count();
    }
    return total;
}

InstanceManager::SplitResult InstanceManager::split() const {
    SplitResult result;
    for (const auto& [_, batch] : batches_) {
        if (batch.count() >= instance_threshold_) {
            result.instanced.push_back(&batch);
        } else {
            result.individual.push_back(&batch);
        }
    }
    return result;
}

} // namespace nexus
