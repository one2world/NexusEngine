#pragma once

#include "nexus/core/types.h"
#include <functional>
#include <string>

namespace nexus::audio {

class AudioEngine;

// ─────────────────────────────────────────────────────────────────────────────
// AudioDeviceConfig — configuration for opening a platform audio device
// ─────────────────────────────────────────────────────────────────────────────

struct AudioDeviceConfig {
    u32 sample_rate{44100};
    u32 channels{2};
    u32 buffer_frames{512};  // frames per callback invocation
};

// ─────────────────────────────────────────────────────────────────────────────
// AudioDevice — cross-platform audio output via miniaudio
//
// Opens a playback device and calls AudioEngine::mix() in the audio callback
// to push samples to the speakers. This is the final link between the custom
// software mixer and actual audio hardware.
// ─────────────────────────────────────────────────────────────────────────────

class AudioDevice {
public:
    AudioDevice();
    ~AudioDevice();

    // Non-copyable, non-movable (owns native device handle)
    AudioDevice(const AudioDevice&) = delete;
    AudioDevice& operator=(const AudioDevice&) = delete;

    /// Open the default playback device and start feeding samples from the engine.
    bool open(AudioEngine* engine, const AudioDeviceConfig& config = {});

    /// Start playback (called automatically by open()).
    bool start();

    /// Stop playback without closing the device.
    void stop();

    /// Close the device and release all resources.
    void close();

    /// Check if the device is currently open and running.
    bool is_open() const { return is_open_; }
    bool is_started() const { return is_started_; }

    /// Get the actual sample rate chosen by the device.
    u32 actual_sample_rate() const { return actual_sample_rate_; }

    /// Get the backend name (e.g. "ALSA", "PulseAudio", "WASAPI", "CoreAudio").
    std::string backend_name() const;

private:
    struct Impl;
    Impl* impl_{nullptr};

    AudioEngine* engine_{nullptr};
    bool is_open_{false};
    bool is_started_{false};
    u32 actual_sample_rate_{0};
};

} // namespace nexus::audio
