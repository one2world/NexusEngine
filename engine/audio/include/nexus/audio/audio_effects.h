#pragma once

#include "nexus/core/types.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace nexus::audio {

// ─────────────────────────────────────────────────────────────────────────────
// AudioEffect — base class for DSP effects applied to audio buffers
// ─────────────────────────────────────────────────────────────────────────────

class AudioEffect {
public:
    virtual ~AudioEffect() = default;

    /// Process stereo interleaved float samples in-place.
    virtual void process(float* samples, u32 frame_count, u32 sample_rate) = 0;

    /// Reset internal state (e.g. on seek, scene change).
    virtual void reset() = 0;

    bool enabled{true};
    float mix{1.0f};   // 0 = dry, 1 = fully wet
};

// ─────────────────────────────────────────────────────────────────────────────
// LowPassFilter — simple first-order RC low-pass filter
// ─────────────────────────────────────────────────────────────────────────────

class LowPassFilter : public AudioEffect {
public:
    explicit LowPassFilter(float cutoff_hz = 5000.0f)
        : cutoff_hz_(cutoff_hz) {}

    void set_cutoff(float hz) { cutoff_hz_ = std::max(hz, 20.0f); }
    float cutoff() const { return cutoff_hz_; }

    void process(float* samples, u32 frame_count, u32 sample_rate) override;
    void reset() override { prev_left_ = 0.0f; prev_right_ = 0.0f; }

private:
    float cutoff_hz_;
    float prev_left_{0.0f};
    float prev_right_{0.0f};
};

// ─────────────────────────────────────────────────────────────────────────────
// HighPassFilter — first-order high-pass filter
// ─────────────────────────────────────────────────────────────────────────────

class HighPassFilter : public AudioEffect {
public:
    explicit HighPassFilter(float cutoff_hz = 200.0f)
        : cutoff_hz_(cutoff_hz) {}

    void set_cutoff(float hz) { cutoff_hz_ = std::max(hz, 20.0f); }
    float cutoff() const { return cutoff_hz_; }

    void process(float* samples, u32 frame_count, u32 sample_rate) override;
    void reset() override;

private:
    float cutoff_hz_;
    float prev_in_left_{0.0f};
    float prev_in_right_{0.0f};
    float prev_out_left_{0.0f};
    float prev_out_right_{0.0f};
};

// ─────────────────────────────────────────────────────────────────────────────
// ReverbEffect — simple Schroeder reverb using comb + allpass filters
// ─────────────────────────────────────────────────────────────────────────────

class ReverbEffect : public AudioEffect {
public:
    struct Config {
        float room_size{0.8f};    // 0..1 — controls feedback
        float damping{0.5f};      // 0..1 — high-frequency damping
        float wet{0.3f};          // wet mix level
        float dry{0.7f};          // dry mix level
    };

    ReverbEffect();
    explicit ReverbEffect(const Config& config);

    void set_config(const Config& config) { config_ = config; update_params(); }
    const Config& config() const { return config_; }

    void process(float* samples, u32 frame_count, u32 sample_rate) override;
    void reset() override;

private:
    static constexpr u32 NUM_COMBS = 4;
    static constexpr u32 NUM_ALLPASS = 2;

    struct CombFilter {
        std::vector<float> buffer;
        u32 index{0};
        float feedback{0.0f};
        float damp1{0.0f};
        float damp2{0.0f};
        float filter_store{0.0f};

        void init(u32 size);
        float process(float input);
    };

    struct AllpassFilter {
        std::vector<float> buffer;
        u32 index{0};
        float feedback{0.5f};

        void init(u32 size);
        float process(float input);
    };

    void update_params();
    void init_filters(u32 sample_rate);

    Config config_;
    CombFilter combs_left_[NUM_COMBS];
    CombFilter combs_right_[NUM_COMBS];
    AllpassFilter allpass_left_[NUM_ALLPASS];
    AllpassFilter allpass_right_[NUM_ALLPASS];
    bool initialized_{false};
    u32 last_sample_rate_{0};
};

// ─────────────────────────────────────────────────────────────────────────────
// AudioEffectChain — ordered chain of effects
// ─────────────────────────────────────────────────────────────────────────────

class AudioEffectChain {
public:
    void add_effect(std::unique_ptr<AudioEffect> effect);
    void remove_effect(u32 index);
    void clear();

    /// Process samples through all enabled effects in order.
    void process(float* samples, u32 frame_count, u32 sample_rate);

    /// Reset all effects.
    void reset();

    u32 effect_count() const { return static_cast<u32>(effects_.size()); }
    AudioEffect* effect(u32 index) { return effects_[index].get(); }
    const AudioEffect* effect(u32 index) const { return effects_[index].get(); }

private:
    std::vector<std::unique_ptr<AudioEffect>> effects_;
};

} // namespace nexus::audio
