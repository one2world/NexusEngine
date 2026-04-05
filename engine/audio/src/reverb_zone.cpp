#include "nexus/audio/reverb_zone.h"
#include "nexus/core/log.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace nexus::audio {

// ── Helper ──────────────────────────────────────────────────────────────────

static float smoothstep(float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    return t * t * (3.0f - 2.0f * t);
}

// ── ReverbZoneManager ───────────────────────────────────────────────────────

u32 ReverbZoneManager::add_zone(const ReverbZone& zone) {
    u32 index = static_cast<u32>(zones_.size());
    zones_.push_back(zone);
    zone_weights_.push_back(0.0f);
    NX_INFO("Reverb zone added: '{}' (index={})", zone.name, index);
    return index;
}

void ReverbZoneManager::remove_zone(u32 index) {
    if (index >= static_cast<u32>(zones_.size())) {
        NX_WARN("ReverbZoneManager::remove_zone: index {} out of range", index);
        return;
    }
    NX_INFO("Reverb zone removed: '{}' (index={})", zones_[index].name, index);
    zones_.erase(zones_.begin() + static_cast<std::ptrdiff_t>(index));
    zone_weights_.erase(zone_weights_.begin() + static_cast<std::ptrdiff_t>(index));
}

void ReverbZoneManager::clear() {
    zones_.clear();
    zone_weights_.clear();
    blended_config_ = {};
    active_zone_count_ = 0;
}

float ReverbZoneManager::compute_zone_influence(const ReverbZone& zone, Vec3 listener_pos) const {
    float fade = zone.config.fade_distance;

    if (zone.shape == ReverbZoneShape::AABB) {
        // Compute signed distance from listener to AABB surface.
        // Negative = inside, positive = outside.
        Vec3 delta = listener_pos - zone.center;
        Vec3 q{
            std::abs(delta.x) - zone.half_extents.x,
            std::abs(delta.y) - zone.half_extents.y,
            std::abs(delta.z) - zone.half_extents.z
        };

        // Signed distance to box surface
        Vec3 clamped{
            std::max(q.x, 0.0f),
            std::max(q.y, 0.0f),
            std::max(q.z, 0.0f)
        };
        float outside_dist = std::sqrt(clamped.x * clamped.x +
                                       clamped.y * clamped.y +
                                       clamped.z * clamped.z);
        float inside_dist = std::min(std::max(q.x, std::max(q.y, q.z)), 0.0f);
        float signed_dist = outside_dist + inside_dist;

        if (signed_dist <= 0.0f) {
            // Inside the box: fade based on distance to surface
            float dist_to_edge = -signed_dist;
            if (fade > 0.0f && dist_to_edge < fade) {
                float t = dist_to_edge / fade;
                return smoothstep(t);
            }
            return 1.0f;
        }

        // Outside the box: fade out over fade_distance
        if (fade > 0.0f && signed_dist < fade) {
            float t = 1.0f - signed_dist / fade;
            return smoothstep(t);
        }
        return 0.0f;

    } else {
        // Sphere
        Vec3 delta = listener_pos - zone.center;
        float dist = std::sqrt(delta.x * delta.x +
                               delta.y * delta.y +
                               delta.z * delta.z);

        if (dist <= zone.radius) {
            // Inside the sphere: fade based on distance to surface
            float dist_to_edge = zone.radius - dist;
            if (fade > 0.0f && dist_to_edge < fade) {
                float t = dist_to_edge / fade;
                return smoothstep(t);
            }
            return 1.0f;
        }

        // Outside the sphere: fade out over fade_distance
        float outside_dist = dist - zone.radius;
        if (fade > 0.0f && outside_dist < fade) {
            float t = 1.0f - outside_dist / fade;
            return smoothstep(t);
        }
        return 0.0f;
    }
}

void ReverbZoneManager::update_listener(Vec3 listener_position) {
    u32 count = static_cast<u32>(zones_.size());

    // Compute raw influences
    for (u32 i = 0; i < count; ++i) {
        if (zones_[i].enabled) {
            zone_weights_[i] = compute_zone_influence(zones_[i], listener_position);
        } else {
            zone_weights_[i] = 0.0f;
        }
    }

    // Build a list of active zone indices, sorted by priority (higher first)
    std::vector<u32> active_indices;
    active_indices.reserve(count);
    for (u32 i = 0; i < count; ++i) {
        if (zone_weights_[i] > 0.0f) {
            active_indices.push_back(i);
        }
    }

    std::sort(active_indices.begin(), active_indices.end(),
              [this](u32 a, u32 b) {
                  return zones_[a].config.priority > zones_[b].config.priority;
              });

    active_zone_count_ = static_cast<u32>(active_indices.size());

    if (active_zone_count_ == 0) {
        blended_config_ = {};
        return;
    }

    // Normalize weights if total exceeds 1.0
    float total_weight = 0.0f;
    for (u32 idx : active_indices) {
        total_weight += zone_weights_[idx];
    }

    float normalizer = (total_weight > 1.0f) ? (1.0f / total_weight) : 1.0f;

    // Blend configs as weighted average
    float blended_room_size = 0.0f;
    float blended_damping = 0.0f;
    float blended_wet = 0.0f;
    float blended_dry = 0.0f;
    float weight_sum = 0.0f;

    for (u32 idx : active_indices) {
        float w = zone_weights_[idx] * normalizer;
        const auto& cfg = zones_[idx].config.reverb;
        blended_room_size += cfg.room_size * w;
        blended_damping += cfg.damping * w;
        blended_wet += cfg.wet * w;
        blended_dry += cfg.dry * w;
        weight_sum += w;
    }

    // If total weight < 1.0, blend remaining portion toward dry defaults
    if (weight_sum < 1.0f) {
        float remaining = 1.0f - weight_sum;
        blended_dry += 1.0f * remaining;
        // room_size, damping, wet stay at 0 for the "no reverb" portion
    }

    blended_config_.room_size = blended_room_size;
    blended_config_.damping = blended_damping;
    blended_config_.wet = blended_wet;
    blended_config_.dry = blended_dry;
}

float ReverbZoneManager::zone_weight(u32 index) const {
    if (index >= static_cast<u32>(zone_weights_.size())) {
        return 0.0f;
    }
    return zone_weights_[index];
}

} // namespace nexus::audio
