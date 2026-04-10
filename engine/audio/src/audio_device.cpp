// Define miniaudio implementation in this translation unit only.
// Must come before including miniaudio.h.
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING       // We don't need encoding (write to file)
#define MA_NO_GENERATION     // We don't need waveform/noise generation

// Suppress warnings from third-party header
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
#endif

#include "miniaudio.h"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include "nexus/audio/audio_device.h"
#include "nexus/audio/audio_engine.h"
#include "nexus/core/log.h"
#include <cstring>

namespace nexus::audio {

// ── Pimpl for miniaudio device ─────────────────────────────────────────────

struct AudioDevice::Impl {
    ma_device device{};
    ma_device_config device_config{};
    bool device_initialized{false};
};

// ── Audio callback — called by miniaudio's audio thread ────────────────────

static void audio_data_callback(ma_device* device, void* output, const void* /*input*/,
                                 ma_uint32 frame_count) {
    auto* engine = static_cast<AudioEngine*>(device->pUserData);
    if (engine) {
        engine->mix(static_cast<float*>(output), static_cast<u32>(frame_count));
    } else {
        std::memset(output, 0, frame_count * device->playback.channels * sizeof(float));
    }
}

// ── AudioDevice implementation ─────────────────────────────────────────────

AudioDevice::AudioDevice()
    : impl_(new Impl()) {
}

AudioDevice::~AudioDevice() {
    close();
    delete impl_;
}

bool AudioDevice::open(AudioEngine* engine, const AudioDeviceConfig& config) {
    if (is_open_) {
        NX_WARN("AudioDevice: already open, close first");
        return false;
    }

    if (!engine) {
        NX_ERROR("AudioDevice: null engine pointer");
        return false;
    }

    engine_ = engine;

    // Configure the device
    impl_->device_config = ma_device_config_init(ma_device_type_playback);
    impl_->device_config.playback.format   = ma_format_f32;
    impl_->device_config.playback.channels = config.channels;
    impl_->device_config.sampleRate        = config.sample_rate;
    impl_->device_config.periodSizeInFrames = config.buffer_frames;
    impl_->device_config.dataCallback      = audio_data_callback;
    impl_->device_config.pUserData         = engine_;

    ma_result result = ma_device_init(nullptr, &impl_->device_config, &impl_->device);
    if (result != MA_SUCCESS) {
        NX_ERROR("AudioDevice: failed to initialize device (error {})", static_cast<int>(result));
        return false;
    }

    impl_->device_initialized = true;
    is_open_ = true;
    actual_sample_rate_ = impl_->device.sampleRate;

    // Sync the engine's output sample rate to what the device actually uses
    engine_->set_output_sample_rate(actual_sample_rate_);

    NX_INFO("AudioDevice: opened — backend={}, sample_rate={}, channels={}, buffer={}",
            backend_name(), actual_sample_rate_,
            impl_->device.playback.channels,
            impl_->device_config.periodSizeInFrames);

    // Auto-start
    return start();
}

bool AudioDevice::start() {
    if (!is_open_ || !impl_->device_initialized) {
        NX_ERROR("AudioDevice: cannot start — device not open");
        return false;
    }

    if (is_started_) return true;

    ma_result result = ma_device_start(&impl_->device);
    if (result != MA_SUCCESS) {
        NX_ERROR("AudioDevice: failed to start device (error {})", static_cast<int>(result));
        return false;
    }

    is_started_ = true;
    NX_INFO("AudioDevice: started");
    return true;
}

void AudioDevice::stop() {
    if (!is_started_ || !impl_->device_initialized) return;

    ma_device_stop(&impl_->device);
    is_started_ = false;
    NX_INFO("AudioDevice: stopped");
}

void AudioDevice::close() {
    if (!is_open_) return;

    if (is_started_) {
        stop();
    }

    if (impl_->device_initialized) {
        ma_device_uninit(&impl_->device);
        impl_->device_initialized = false;
    }

    is_open_ = false;
    engine_ = nullptr;
    actual_sample_rate_ = 0;
    NX_INFO("AudioDevice: closed");
}

std::string AudioDevice::backend_name() const {
    if (!impl_->device_initialized) return "none";

    ma_backend backend = impl_->device.pContext->backend;
    switch (backend) {
        case ma_backend_wasapi:      return "WASAPI";
        case ma_backend_dsound:      return "DirectSound";
        case ma_backend_winmm:       return "WinMM";
        case ma_backend_coreaudio:   return "CoreAudio";
        case ma_backend_alsa:        return "ALSA";
        case ma_backend_pulseaudio:  return "PulseAudio";
        case ma_backend_jack:        return "JACK";
        case ma_backend_oss:         return "OSS";
        case ma_backend_aaudio:      return "AAudio";
        case ma_backend_opensl:      return "OpenSL|ES";
        case ma_backend_webaudio:    return "WebAudio";
        case ma_backend_null:        return "Null";
        default:                     return "Unknown";
    }
}

} // namespace nexus::audio
