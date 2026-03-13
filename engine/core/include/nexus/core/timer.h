#pragma once

#include <chrono>

namespace nexus {

class Timer {
public:
    Timer();

    void reset();
    void tick();

    [[nodiscard]] float delta_time() const { return delta_time_; }
    [[nodiscard]] float elapsed() const { return elapsed_; }
    [[nodiscard]] uint64_t frame_count() const { return frame_count_; }
    [[nodiscard]] float fps() const { return delta_time_ > 0.0f ? 1.0f / delta_time_ : 0.0f; }

private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;

    TimePoint start_;
    TimePoint last_tick_;
    float delta_time_ = 0.0f;
    float elapsed_ = 0.0f;
    uint64_t frame_count_ = 0;
};

// Fixed timestep accumulator for physics
class FixedTimestep {
public:
    explicit FixedTimestep(float step = 1.0f / 60.0f) : step_(step) {}

    void accumulate(float dt) { accumulator_ += dt; }
    bool should_step() { return accumulator_ >= step_; }
    void consume() { accumulator_ -= step_; }
    [[nodiscard]] float step() const { return step_; }
    [[nodiscard]] float alpha() const { return accumulator_ / step_; }

private:
    float step_;
    float accumulator_ = 0.0f;
};

} // namespace nexus
