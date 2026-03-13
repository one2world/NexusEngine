#include "nexus/core/timer.h"

namespace nexus {

Timer::Timer()
    : start_(Clock::now())
    , last_tick_(start_) {
}

void Timer::reset() {
    start_ = Clock::now();
    last_tick_ = start_;
    delta_time_ = 0.0f;
    elapsed_ = 0.0f;
    frame_count_ = 0;
}

void Timer::tick() {
    auto now = Clock::now();
    delta_time_ = std::chrono::duration<float>(now - last_tick_).count();
    elapsed_ = std::chrono::duration<float>(now - start_).count();
    last_tick_ = now;
    frame_count_++;
}

} // namespace nexus
