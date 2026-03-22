#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include <string>
#include <vector>
#include <cstdint>

namespace nexus::audio {

// ─────────────────────────────────────────────────────────────────────────────
// AudioFormat - describes PCM audio data
// ─────────────────────────────────────────────────────────────────────────────

enum class SampleFormat : u8 {
    U8,       // unsigned 8-bit
    S16,      // signed 16-bit (most common)
    S32,      // signed 32-bit
    F32,      // 32-bit float [-1.0, 1.0]
};

struct AudioFormat {
    u32          sample_rate{44100};
    u32          channels{2};       // 1 = mono, 2 = stereo
    SampleFormat format{SampleFormat::S16};

    [[nodiscard]] u32 bytes_per_sample() const {
        switch (format) {
            case SampleFormat::U8:  return 1;
            case SampleFormat::S16: return 2;
            case SampleFormat::S32: return 4;
            case SampleFormat::F32: return 4;
        }
        return 2;
    }

    [[nodiscard]] u32 frame_size() const { return channels * bytes_per_sample(); }
};

// ─────────────────────────────────────────────────────────────────────────────
// AudioBuffer - raw PCM data in memory
// ─────────────────────────────────────────────────────────────────────────────

struct AudioBuffer {
    AudioFormat format;
    std::vector<u8> data;
    u32 frame_count{0};

    [[nodiscard]] float duration_seconds() const {
        if (format.sample_rate == 0) return 0.0f;
        return static_cast<float>(frame_count) / static_cast<float>(format.sample_rate);
    }

    /// Read a sample as float [-1, 1] at the given frame and channel.
    [[nodiscard]] float read_sample(u32 frame, u32 channel) const;
};

// ─────────────────────────────────────────────────────────────────────────────
// AudioClip - a named, loaded audio resource
// ─────────────────────────────────────────────────────────────────────────────

using AudioClipId = u32;
constexpr AudioClipId INVALID_CLIP_ID = 0;

struct AudioClip {
    AudioClipId id{INVALID_CLIP_ID};
    std::string name;
    AudioBuffer buffer;
};

// ─────────────────────────────────────────────────────────────────────────────
// WAV loader
// ─────────────────────────────────────────────────────────────────────────────

bool load_wav(const std::string& filepath, AudioBuffer& out_buffer);
bool load_wav_from_memory(const u8* data, size_t size, AudioBuffer& out_buffer);

// ─────────────────────────────────────────────────────────────────────────────
// OGG Vorbis loader (minimal decoder)
// ─────────────────────────────────────────────────────────────────────────────

bool load_ogg(const std::string& filepath, AudioBuffer& out_buffer);
bool load_ogg_from_memory(const u8* data, size_t size, AudioBuffer& out_buffer);

// ─────────────────────────────────────────────────────────────────────────────
// Generic audio loader (auto-detects format by extension)
// ─────────────────────────────────────────────────────────────────────────────

bool load_audio(const std::string& filepath, AudioBuffer& out_buffer);

} // namespace nexus::audio
