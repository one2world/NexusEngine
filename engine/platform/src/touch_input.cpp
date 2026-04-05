#include "nexus/platform/touch_input.h"
#include <cmath>
#include <algorithm>

namespace nexus {

TouchPoint* TouchInput::find_touch(u32 id) {
    for (auto& t : touches_) {
        if (t.active && t.id == id) return &t;
    }
    return nullptr;
}

TouchPoint* TouchInput::find_free_slot() {
    for (auto& t : touches_) {
        if (!t.active) return &t;
    }
    return nullptr;
}

void TouchInput::on_touch_begin(u32 id, Vec2 position, f32 pressure) {
    auto* slot = find_free_slot();
    if (!slot) return;

    slot->id = id;
    slot->position = position;
    slot->prev_position = position;
    slot->delta = Vec2(0.0f);
    slot->phase = TouchPhase::Began;
    slot->pressure = pressure;
    slot->active = true;
    ++active_count_;

    if (active_count_ == 1) {
        touch_start_pos_ = position;
    }
}

void TouchInput::on_touch_move(u32 id, Vec2 position, f32 pressure) {
    auto* t = find_touch(id);
    if (!t) return;

    t->prev_position = t->position;
    t->position = position;
    t->delta = position - t->prev_position;
    t->phase = TouchPhase::Moved;
    t->pressure = pressure;
}

void TouchInput::on_touch_end(u32 id, Vec2 position) {
    auto* t = find_touch(id);
    if (!t) return;

    t->prev_position = t->position;
    t->position = position;
    t->delta = position - t->prev_position;
    t->phase = TouchPhase::Ended;
    t->active = false;
    if (active_count_ > 0) --active_count_;
}

void TouchInput::on_touch_cancel(u32 id) {
    auto* t = find_touch(id);
    if (!t) return;

    t->phase = TouchPhase::Cancelled;
    t->active = false;
    if (active_count_ > 0) --active_count_;
}

void TouchInput::update(f64 current_time) {
    // Reset per-frame gesture state
    gesture_.tapped = false;
    gesture_.swiped = false;
    gesture_.pinch_delta = 0.0f;
    gesture_.rotation_delta = 0.0f;
    gesture_.pan_delta = Vec2(0.0f);

    // Mark stationary touches
    for (auto& t : touches_) {
        if (t.active && t.phase == TouchPhase::Moved) {
            if (glm::length(t.delta) < 0.5f) {
                t.phase = TouchPhase::Stationary;
            }
        }
        t.timestamp = current_time;
    }

    detect_gestures(current_time);
}

void TouchInput::detect_gestures(f64 current_time) {
    // Collect active touches and recently-ended ones
    std::vector<TouchPoint*> active;
    std::vector<TouchPoint*> ended;
    for (auto& t : touches_) {
        if (t.active) active.push_back(&t);
        else if (t.phase == TouchPhase::Ended) ended.push_back(&t);
    }

    // Check for tap/swipe on ended touches
    for (auto* t : ended) {
        f32 dist = glm::length(t->position - touch_start_pos_);
        f64 duration = current_time - touch_start_time_;

        if (dist < tap_threshold_ && duration < 0.3) {
            gesture_.tapped = true;
            gesture_.tap_position = t->position;
            if (current_time - last_tap_time_ < 0.3) {
                gesture_.tap_count = 2;
            } else {
                gesture_.tap_count = 1;
            }
            last_tap_time_ = current_time;
        }

        f32 velocity = (duration > 0.001) ? dist / static_cast<f32>(duration) : 0.0f;
        if (velocity > swipe_threshold_ && dist > tap_threshold_ * 2.0f) {
            gesture_.swiped = true;
            Vec2 dir = t->position - touch_start_pos_;
            f32 len = glm::length(dir);
            gesture_.swipe_direction = (len > 0.001f) ? dir / len : Vec2(0.0f);
            gesture_.swipe_velocity = velocity;
        }

        // Clear ended phase so we don't re-detect next frame
        t->phase = TouchPhase::Cancelled;
    }

    // Single touch gestures
    if (active.size() == 1) {
        auto* t = active[0];
        gesture_.panning = (t->phase == TouchPhase::Moved);
        if (gesture_.panning) {
            gesture_.pan_position = t->position;
            gesture_.pan_delta = t->delta;
        }

        // Tap detection on end (active touch ending this frame)
        if (t->phase == TouchPhase::Ended) {
            f32 dist = glm::length(t->position - touch_start_pos_);
            f64 duration = current_time - touch_start_time_;
            if (dist < tap_threshold_ && duration < 0.3) {
                gesture_.tapped = true;
                gesture_.tap_position = t->position;

                // Double-tap detection
                if (current_time - last_tap_time_ < 0.3) {
                    gesture_.tap_count = 2;
                } else {
                    gesture_.tap_count = 1;
                }
                last_tap_time_ = current_time;
            }

            // Swipe detection
            f32 velocity = (duration > 0.001) ? dist / static_cast<f32>(duration) : 0.0f;
            if (velocity > swipe_threshold_ && dist > tap_threshold_ * 2.0f) {
                gesture_.swiped = true;
                Vec2 dir = t->position - touch_start_pos_;
                f32 len = glm::length(dir);
                gesture_.swipe_direction = (len > 0.001f) ? dir / len : Vec2(0.0f);
                gesture_.swipe_velocity = velocity;
            }
        }

        if (t->phase == TouchPhase::Began) {
            touch_start_time_ = current_time;
            touch_start_pos_ = t->position;
        }
    }

    // Two-finger gestures: pinch + rotate
    if (active.size() >= 2) {
        auto* t0 = active[0];
        auto* t1 = active[1];

        Vec2 diff = t1->position - t0->position;
        f32 distance = glm::length(diff);
        f32 angle = std::atan2(diff.y, diff.x);

        gesture_.pinch_center = (t0->position + t1->position) * 0.5f;

        // Pinch
        if (prev_pinch_distance_ > 0.001f) {
            gesture_.pinching = true;
            gesture_.pinch_delta = distance - prev_pinch_distance_;
            gesture_.pinch_scale = distance / prev_pinch_distance_;
        }
        prev_pinch_distance_ = distance;

        // Rotation
        if (std::abs(prev_rotation_angle_) > 0.001f || std::abs(angle) > 0.001f) {
            f32 delta = angle - prev_rotation_angle_;
            // Wrap to [-PI, PI]
            while (delta > 3.14159f) delta -= 6.28318f;
            while (delta < -3.14159f) delta += 6.28318f;
            gesture_.rotating = (std::abs(delta) > 0.001f);
            gesture_.rotation_delta = delta;
            gesture_.rotation_angle += delta;
        }
        prev_rotation_angle_ = angle;

        // Two-finger pan
        gesture_.panning = true;
        gesture_.pan_position = gesture_.pinch_center;
        gesture_.pan_delta = (t0->delta + t1->delta) * 0.5f;
    } else {
        gesture_.pinching = false;
        gesture_.rotating = false;
        prev_pinch_distance_ = 0.0f;
        prev_rotation_angle_ = 0.0f;
    }
}

} // namespace nexus
