#pragma once

#include "nexus/audio/audio_effects.h"
#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include <vector>
#include <string>

namespace nexus::audio {

/// Shape of a reverb zone.
enum class ReverbZoneShape : u8 {
    AABB,
    Sphere
};

/// Configuration for a reverb zone.
struct ReverbZoneConfig {
    ReverbEffect::Config reverb;    // Reverb DSP parameters
    float fade_distance{2.0f};      // Distance over which reverb blends in/out
    u32 priority{0};                // Higher priority zones override lower
};

/// A spatial reverb zone in the scene.
struct ReverbZone {
    std::string name;
    ReverbZoneShape shape{ReverbZoneShape::AABB};
    Vec3 center{0.0f};
    Vec3 half_extents{5.0f};       // For AABB
    float radius{5.0f};            // For Sphere
    ReverbZoneConfig config;
    bool enabled{true};
};

/// ReverbZoneManager -- manages spatial reverb zones and blending.
class ReverbZoneManager {
public:
    ReverbZoneManager() = default;

    /// Add a reverb zone. Returns its index.
    u32 add_zone(const ReverbZone& zone);

    /// Remove a zone by index.
    void remove_zone(u32 index);

    /// Get/set zone by index.
    ReverbZone& zone(u32 index) { return zones_[index]; }
    const ReverbZone& zone(u32 index) const { return zones_[index]; }

    /// Number of zones.
    u32 zone_count() const { return static_cast<u32>(zones_.size()); }

    /// Clear all zones.
    void clear();

    /// Update the listener position. Recomputes active zone blending.
    void update_listener(Vec3 listener_position);

    /// Get the current blended reverb config based on listener position.
    /// Returns a config with parameters interpolated between active zones.
    ReverbEffect::Config current_reverb() const { return blended_config_; }

    /// Whether any zone is currently affecting the listener.
    bool is_active() const { return active_zone_count_ > 0; }

    /// Number of zones currently influencing the listener.
    u32 active_zone_count() const { return active_zone_count_; }

    /// Get the blend weight of the listener in a specific zone (0 = outside, 1 = fully inside).
    float zone_weight(u32 index) const;

private:
    /// Compute how much the listener is inside a zone (0..1).
    float compute_zone_influence(const ReverbZone& zone, Vec3 listener_pos) const;

    std::vector<ReverbZone> zones_;
    std::vector<float> zone_weights_;
    ReverbEffect::Config blended_config_;
    u32 active_zone_count_{0};
};

} // namespace nexus::audio
