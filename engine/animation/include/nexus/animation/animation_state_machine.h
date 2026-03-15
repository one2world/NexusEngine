#pragma once

#include "nexus/core/types.h"
#include "nexus/animation/animation_clip.h"
#include "nexus/animation/skeleton.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace nexus::anim {

// ─────────────────────────────────────────────────────────────────────────────
// AnimationState - a state in the state machine
// ─────────────────────────────────────────────────────────────────────────────

struct AnimationState {
    std::string    name;
    AnimationClip* clip{nullptr};
    float          speed{1.0f};
    bool           looping{true};
};

// ─────────────────────────────────────────────────────────────────────────────
// Transition - describes how to move between states
// ─────────────────────────────────────────────────────────────────────────────

struct Transition {
    std::string from;
    std::string to;
    float       blend_duration{0.2f};   // seconds to crossfade
    std::function<bool()> condition;     // when to trigger
};

// ─────────────────────────────────────────────────────────────────────────────
// AnimationStateMachine - drives skeletal animation via states and transitions
// ─────────────────────────────────────────────────────────────────────────────

class AnimationStateMachine {
public:
    /// Add a state.
    void add_state(const std::string& name, AnimationClip* clip,
                   float speed = 1.0f, bool looping = true);

    /// Add a transition between states.
    void add_transition(const std::string& from, const std::string& to,
                        float blend_duration, std::function<bool()> condition);

    /// Set the initial state.
    void set_state(const std::string& name);

    /// Get the current state name.
    const std::string& current_state() const { return current_name_; }

    /// Check and trigger transitions, advance time, produce output pose.
    void update(float dt, const Skeleton& skeleton, std::vector<BonePose>& out_pose);

    /// Set a parameter (for condition functions to query).
    void set_float(const std::string& name, float value);
    void set_bool(const std::string& name, bool value);
    float get_float(const std::string& name) const;
    bool get_bool(const std::string& name) const;

    /// Is currently blending between states?
    bool is_transitioning() const { return transitioning_; }

private:
    AnimationState* find_state(const std::string& name);

    std::vector<AnimationState> states_;
    std::vector<Transition>     transitions_;

    std::string current_name_;
    float       current_time_{0.0f};

    // Transition state
    bool        transitioning_{false};
    std::string target_name_;
    float       target_time_{0.0f};
    float       blend_elapsed_{0.0f};
    float       blend_duration_{0.2f};

    // Parameters
    std::unordered_map<std::string, float> float_params_;
    std::unordered_map<std::string, bool>  bool_params_;
};

} // namespace nexus::anim
