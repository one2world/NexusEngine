#pragma once

#include "nexus/core/types.h"
#include "nexus/audio/audio_buffer.h"
#include <string>
#include <vector>
#include <memory>
#include <fstream>

namespace nexus::audio {

// ============================================================================
// AudioStream - streams audio from disk in chunks (avoids loading entire file)
// ============================================================================

class AudioStream {
public:
    static constexpr u32 STREAM_BUFFER_FRAMES = 4096;

    AudioStream() = default;
    ~AudioStream() = default;

    /// Open an audio file for streaming. Returns false if the file can't be opened
    /// or the format is unsupported.
    bool open(const std::string& filepath);

    /// Close the stream and release resources.
    void close();

    /// Read the next chunk of PCM frames into the internal buffer.
    /// Returns the number of frames actually read (0 at end of file).
    u32 read_frames(u32 num_frames);

    /// Seek to a specific frame position.
    bool seek(u32 frame);

    /// Reset to the beginning (for looping).
    void rewind();

    /// Check if the stream has reached the end.
    bool at_end() const { return at_end_; }

    /// Check if the stream is open and ready.
    bool is_open() const { return is_open_; }

    /// Get the audio format.
    const AudioFormat& format() const { return format_; }

    /// Get the total frame count (if known, 0 if unknown).
    u32 total_frames() const { return total_frames_; }

    /// Get the internal buffer containing the most recently read frames.
    const float* buffer() const { return decode_buffer_.data(); }
    u32 buffer_frame_count() const { return buffered_frames_; }

    /// Get the current playback position in frames.
    u32 position() const { return current_frame_; }

private:
    bool is_open_{false};
    bool at_end_{false};
    AudioFormat format_;
    u32 total_frames_{0};
    u32 current_frame_{0};
    u32 buffered_frames_{0};

    // For WAV streaming: file handle + data offset
    std::ifstream file_;
    size_t data_offset_{0};
    size_t data_size_{0};

    // Decoded float buffer (interleaved channels)
    std::vector<float> decode_buffer_;

    // File type
    enum class FileType { Unknown, WAV, OGG };
    FileType file_type_{FileType::Unknown};

    // For OGG: store full decoded buffer (streaming OGG requires full Vorbis state)
    AudioBuffer ogg_buffer_;

    bool open_wav(const std::string& filepath);
    u32 read_wav_frames(u32 num_frames);
    u32 read_ogg_frames(u32 num_frames);
};

// ============================================================================
// Doppler effect calculator
// ============================================================================

struct DopplerParams {
    Vec3 source_position{0.0f};
    Vec3 source_velocity{0.0f};
    Vec3 listener_position{0.0f};
    Vec3 listener_velocity{0.0f};
    float speed_of_sound{343.0f};   // m/s
    float doppler_factor{1.0f};      // 0 = disabled, 1 = realistic, >1 = exaggerated
};

/// Calculate the pitch multiplier due to the Doppler effect.
/// Returns a value around 1.0: >1 = higher pitch (approaching), <1 = lower (receding).
float calculate_doppler_pitch(const DopplerParams& params);

} // namespace nexus::audio
