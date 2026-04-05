#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include <array>
#include <vector>

namespace nexus {

static constexpr u32 MAX_TOUCH_POINTS = 10;

enum class TouchPhase : u8 {
    Began,
    Moved,
    Stationary,
    Ended,
    Cancelled
};

struct TouchPoint {
    u32 id{0};
    Vec2 position{0.0f};
    Vec2 prev_position{0.0f};
    Vec2 delta{0.0f};
    TouchPhase phase{TouchPhase::Ended};
    f32 pressure{1.0f};
    f64 timestamp{0.0};
    bool active{false};
};

struct GestureState {
    // Pinch
    bool pinching{false};
    f32 pinch_scale{1.0f};
    f32 pinch_delta{0.0f};
    Vec2 pinch_center{0.0f};

    // Rotation
    bool rotating{false};
    f32 rotation_angle{0.0f};
    f32 rotation_delta{0.0f};

    // Pan
    bool panning{false};
    Vec2 pan_position{0.0f};
    Vec2 pan_delta{0.0f};

    // Tap
    bool tapped{false};
    u32 tap_count{0};
    Vec2 tap_position{0.0f};

    // Swipe
    bool swiped{false};
    Vec2 swipe_direction{0.0f};
    f32 swipe_velocity{0.0f};
};

class TouchInput {
public:
    TouchInput() = default;

    void update(f64 current_time);

    void on_touch_begin(u32 id, Vec2 position, f32 pressure = 1.0f);
    void on_touch_move(u32 id, Vec2 position, f32 pressure = 1.0f);
    void on_touch_end(u32 id, Vec2 position);
    void on_touch_cancel(u32 id);

    const TouchPoint& touch(u32 index) const { return touches_[index]; }
    u32 active_touch_count() const { return active_count_; }
    const GestureState& gestures() const { return gesture_; }

    bool is_touching() const { return active_count_ > 0; }

    void set_tap_threshold(f32 distance) { tap_threshold_ = distance; }
    void set_swipe_threshold(f32 velocity) { swipe_threshold_ = velocity; }

private:
    TouchPoint* find_touch(u32 id);
    TouchPoint* find_free_slot();
    void detect_gestures(f64 current_time);

    std::array<TouchPoint, MAX_TOUCH_POINTS> touches_{};
    u32 active_count_{0};
    GestureState gesture_{};
    f32 tap_threshold_{10.0f};
    f32 swipe_threshold_{100.0f};
    f64 last_tap_time_{0.0};
    Vec2 touch_start_pos_{0.0f};
    f64 touch_start_time_{0.0};
    f32 prev_pinch_distance_{0.0f};
    f32 prev_rotation_angle_{0.0f};
};

} // namespace nexus
