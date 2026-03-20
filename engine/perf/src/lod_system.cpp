#include "nexus/perf/lod_system.h"

#include <algorithm>

namespace nexus {

// ---------------------------------------------------------------------------
// LODGroup
// ---------------------------------------------------------------------------

const LODLevel* LODGroup::select(f32 distance) const {
    if (cull_distance > 0.0f && distance >= cull_distance) return nullptr;
    if (levels.empty()) return nullptr;

    for (const auto& level : levels) {
        if (distance < level.max_distance) {
            return &level;
        }
    }
    // Beyond all level distances but within cull distance: use the last level.
    return &levels.back();
}

void LODGroup::add_level(f32 max_dist, u32 tri_count, u32 mesh_id) {
    LODLevel lv;
    lv.level          = static_cast<u32>(levels.size());
    lv.max_distance   = max_dist;
    lv.triangle_count = tri_count;
    lv.mesh_id        = mesh_id;
    levels.push_back(lv);

    // Keep sorted by max_distance.
    std::sort(levels.begin(), levels.end(),
        [](const LODLevel& a, const LODLevel& b) { return a.max_distance < b.max_distance; });

    // Re-assign level indices after sort.
    for (u32 i = 0; i < static_cast<u32>(levels.size()); ++i) {
        levels[i].level = i;
    }
}

// ---------------------------------------------------------------------------
// LODEvaluator
// ---------------------------------------------------------------------------

u32 LODEvaluator::register_group(LODGroup group) {
    u32 id = static_cast<u32>(groups_.size());
    group.id = id;
    groups_.push_back(std::move(group));
    return id;
}

const LODGroup* LODEvaluator::find_group(u32 id) const {
    if (id >= static_cast<u32>(groups_.size())) return nullptr;
    return &groups_[id];
}

std::vector<LODSelection> LODEvaluator::evaluate(
    const Vec3& camera_pos,
    const std::vector<std::pair<u32, Vec3>>& objects) const {

    std::vector<LODSelection> results;
    results.reserve(objects.size());

    for (const auto& [group_id, world_pos] : objects) {
        LODSelection sel;
        sel.group_id = group_id;
        sel.distance = glm::distance(camera_pos, world_pos);

        if (group_id >= static_cast<u32>(groups_.size())) {
            sel.culled = true;
            results.push_back(sel);
            continue;
        }

        const auto& group = groups_[group_id];
        f32 biased_distance = sel.distance * bias_;

        const LODLevel* lv = group.select(biased_distance);
        if (!lv) {
            sel.culled = true;
        } else {
            sel.lod_level = lv->level;
            sel.mesh_id   = lv->mesh_id;
        }
        results.push_back(sel);
    }
    return results;
}

} // namespace nexus
