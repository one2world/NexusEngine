#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace nexus::anim {

// ─────────────────────────────────────────────────────────────────────────────
// SpriteFrame - a single frame in a sprite animation
// ─────────────────────────────────────────────────────────────────────────────

struct SpriteFrame {
    Vec2  uv_min{0.0f, 0.0f};
    Vec2  uv_max{1.0f, 1.0f};
    float duration{1.0f / 12.0f};  // seconds per frame
};

// ─────────────────────────────────────────────────────────────────────────────
// SpriteAnimation - a sequence of frames
// ─────────────────────────────────────────────────────────────────────────────

struct SpriteAnimation {
    std::string name;
    std::vector<SpriteFrame> frames;
    bool looping{true};
};

// ─────────────────────────────────────────────────────────────────────────────
// SpriteAnimator - plays sprite animations, outputs current UV coords
// ─────────────────────────────────────────────────────────────────────────────

class SpriteAnimator {
public:
    /// Add an animation.
    void add_animation(const std::string& name, SpriteAnimation anim);

    /// Play an animation by name.
    void play(const std::string& name, bool reset = true);

    /// Stop the current animation.
    void stop();

    /// Advance time and update current frame.
    void update(float dt);

    /// Get current frame UV coordinates.
    Vec2 current_uv_min() const;
    Vec2 current_uv_max() const;

    /// Get current animation name.
    const std::string& current_animation() const { return current_name_; }

    /// Check if animation has finished (non-looping only).
    bool finished() const { return finished_; }

    /// Get current frame index.
    u32 current_frame_index() const { return frame_index_; }

    /// Speed multiplier (1.0 = normal).
    void set_speed(float speed) { speed_ = speed; }
    float speed() const { return speed_; }

    /// Generate frames from a sprite sheet grid.
    static SpriteAnimation from_sheet(const std::string& name,
                                       u32 cols, u32 rows,
                                       u32 frame_count, float fps,
                                       bool looping = true);

private:
    std::unordered_map<std::string, SpriteAnimation> animations_;
    std::string current_name_;
    u32   frame_index_{0};
    float frame_timer_{0.0f};
    float speed_{1.0f};
    bool  playing_{false};
    bool  finished_{false};
};

} // namespace nexus::anim
