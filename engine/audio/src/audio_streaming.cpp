#include "nexus/audio/audio_streaming.h"
#include "nexus/core/log.h"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace nexus::audio {

// ── WAV streaming helpers ──────────────────────────────────────────────────

#pragma pack(push, 1)
struct StreamWavHeader {
    char     riff_tag[4];
    uint32_t riff_size;
    char     wave_tag[4];
};

struct StreamWavChunkHeader {
    char     id[4];
    uint32_t size;
};

struct StreamWavFmtChunk {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
};
#pragma pack(pop)

bool AudioStream::open(const std::string& filepath) {
    close();

    // Determine file type from extension
    std::string ext;
    auto dot = filepath.rfind('.');
    if (dot != std::string::npos) {
        ext = filepath.substr(dot);
        for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    if (ext == ".wav") {
        file_type_ = FileType::WAV;
        return open_wav(filepath);
    } else if (ext == ".ogg") {
        file_type_ = FileType::OGG;
        // For OGG, load the entire file and decode (streaming Vorbis requires full state machine)
        if (!load_ogg(filepath, ogg_buffer_)) {
            NX_ERROR("AudioStream: failed to decode OGG file: {}", filepath);
            return false;
        }
        format_ = ogg_buffer_.format;
        total_frames_ = ogg_buffer_.frame_count;
        current_frame_ = 0;
        at_end_ = false;
        is_open_ = true;
        decode_buffer_.resize(STREAM_BUFFER_FRAMES * format_.channels);
        NX_INFO("AudioStream: opened OGG stream '{}' ({:.2f}s)", filepath,
                static_cast<float>(total_frames_) / static_cast<float>(format_.sample_rate));
        return true;
    }

    NX_ERROR("AudioStream: unsupported format '{}'", ext);
    return false;
}

bool AudioStream::open_wav(const std::string& filepath) {
    file_.open(filepath, std::ios::binary);
    if (!file_.is_open()) {
        NX_ERROR("AudioStream: cannot open WAV file: {}", filepath);
        return false;
    }

    // Read and validate WAV header
    StreamWavHeader header;
    file_.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!file_ || std::memcmp(header.riff_tag, "RIFF", 4) != 0 ||
        std::memcmp(header.wave_tag, "WAVE", 4) != 0) {
        NX_ERROR("AudioStream: invalid WAV header");
        file_.close();
        return false;
    }

    // Find fmt and data chunks
    StreamWavFmtChunk fmt{};
    bool found_fmt = false;

    while (file_) {
        StreamWavChunkHeader chunk;
        file_.read(reinterpret_cast<char*>(&chunk), sizeof(chunk));
        if (!file_) break;

        if (std::memcmp(chunk.id, "fmt ", 4) == 0) {
            file_.read(reinterpret_cast<char*>(&fmt),
                       static_cast<std::streamsize>(std::min(static_cast<size_t>(chunk.size), sizeof(fmt))));
            found_fmt = true;
            // Skip any remaining fmt data
            if (chunk.size > sizeof(fmt)) {
                file_.seekg(static_cast<std::streamoff>(chunk.size - sizeof(fmt)), std::ios::cur);
            }
        } else if (std::memcmp(chunk.id, "data", 4) == 0) {
            data_offset_ = static_cast<size_t>(file_.tellg());
            data_size_ = chunk.size;
            break;
        } else {
            // Skip unknown chunk
            file_.seekg(chunk.size, std::ios::cur);
        }

        // Align to 2-byte boundary
        if (static_cast<size_t>(file_.tellg()) % 2 != 0) {
            file_.seekg(1, std::ios::cur);
        }
    }

    if (!found_fmt || data_size_ == 0) {
        NX_ERROR("AudioStream: WAV missing fmt or data chunk");
        file_.close();
        return false;
    }

    if (fmt.audio_format != 1 && fmt.audio_format != 3) {
        NX_ERROR("AudioStream: unsupported WAV format {}", fmt.audio_format);
        file_.close();
        return false;
    }

    format_.sample_rate = fmt.sample_rate;
    format_.channels = fmt.num_channels;
    if (fmt.audio_format == 3) {
        format_.format = SampleFormat::F32;
    } else {
        switch (fmt.bits_per_sample) {
            case 8:  format_.format = SampleFormat::U8;  break;
            case 16: format_.format = SampleFormat::S16; break;
            case 32: format_.format = SampleFormat::S32; break;
            default:
                NX_ERROR("AudioStream: unsupported bits per sample {}", fmt.bits_per_sample);
                file_.close();
                return false;
        }
    }

    total_frames_ = static_cast<u32>(data_size_ / format_.frame_size());
    current_frame_ = 0;
    at_end_ = false;
    is_open_ = true;

    decode_buffer_.resize(STREAM_BUFFER_FRAMES * format_.channels);

    NX_INFO("AudioStream: opened WAV stream '{}' ({}ch, {}Hz, {:.2f}s)",
            filepath, format_.channels, format_.sample_rate,
            static_cast<float>(total_frames_) / static_cast<float>(format_.sample_rate));
    return true;
}

void AudioStream::close() {
    if (file_.is_open()) file_.close();
    ogg_buffer_ = AudioBuffer{};
    decode_buffer_.clear();
    is_open_ = false;
    at_end_ = false;
    current_frame_ = 0;
    buffered_frames_ = 0;
    file_type_ = FileType::Unknown;
}

u32 AudioStream::read_frames(u32 num_frames) {
    if (!is_open_ || at_end_) return 0;

    num_frames = std::min(num_frames, STREAM_BUFFER_FRAMES);

    switch (file_type_) {
        case FileType::WAV: return read_wav_frames(num_frames);
        case FileType::OGG: return read_ogg_frames(num_frames);
        default: return 0;
    }
}

u32 AudioStream::read_wav_frames(u32 num_frames) {
    u32 remaining = total_frames_ - current_frame_;
    u32 to_read = std::min(num_frames, remaining);
    if (to_read == 0) { at_end_ = true; return 0; }

    u32 bytes_per_frame = format_.frame_size();
    std::vector<u8> raw(to_read * bytes_per_frame);

    file_.seekg(static_cast<std::streamoff>(data_offset_ + current_frame_ * bytes_per_frame));
    file_.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
    u32 actually_read = static_cast<u32>(file_.gcount()) / bytes_per_frame;

    // Convert to float
    decode_buffer_.resize(actually_read * format_.channels);
    for (u32 i = 0; i < actually_read * format_.channels; ++i) {
        const u8* ptr = raw.data() + i * format_.bytes_per_sample();
        switch (format_.format) {
            case SampleFormat::U8:
                decode_buffer_[i] = (static_cast<float>(*ptr) - 128.0f) / 128.0f;
                break;
            case SampleFormat::S16: {
                int16_t val;
                std::memcpy(&val, ptr, 2);
                decode_buffer_[i] = static_cast<float>(val) / 32768.0f;
                break;
            }
            case SampleFormat::S32: {
                int32_t val;
                std::memcpy(&val, ptr, 4);
                decode_buffer_[i] = static_cast<float>(val) / 2147483648.0f;
                break;
            }
            case SampleFormat::F32:
                std::memcpy(&decode_buffer_[i], ptr, 4);
                break;
        }
    }

    current_frame_ += actually_read;
    buffered_frames_ = actually_read;
    if (current_frame_ >= total_frames_) at_end_ = true;
    return actually_read;
}

u32 AudioStream::read_ogg_frames(u32 num_frames) {
    u32 remaining = total_frames_ - current_frame_;
    u32 to_read = std::min(num_frames, remaining);
    if (to_read == 0) { at_end_ = true; return 0; }

    // OGG is fully decoded in memory, just copy the relevant slice
    decode_buffer_.resize(to_read * format_.channels);
    u32 byte_offset = current_frame_ * format_.channels * sizeof(float);
    u32 byte_count = to_read * format_.channels * sizeof(float);

    if (byte_offset + byte_count <= ogg_buffer_.data.size()) {
        std::memcpy(decode_buffer_.data(), ogg_buffer_.data.data() + byte_offset, byte_count);
    } else {
        std::fill(decode_buffer_.begin(), decode_buffer_.end(), 0.0f);
    }

    current_frame_ += to_read;
    buffered_frames_ = to_read;
    if (current_frame_ >= total_frames_) at_end_ = true;
    return to_read;
}

bool AudioStream::seek(u32 frame) {
    if (!is_open_) return false;
    if (frame >= total_frames_) frame = total_frames_;
    current_frame_ = frame;
    at_end_ = (frame >= total_frames_);
    buffered_frames_ = 0;
    return true;
}

void AudioStream::rewind() {
    current_frame_ = 0;
    at_end_ = false;
    buffered_frames_ = 0;
}

// ── Doppler effect ─────────────────────────────────────────────────────────

float calculate_doppler_pitch(const DopplerParams& params) {
    if (params.doppler_factor <= 0.0f) return 1.0f;

    Vec3 to_listener = params.listener_position - params.source_position;
    float distance = glm::length(to_listener);
    if (distance < 0.001f) return 1.0f;

    Vec3 direction = to_listener / distance;

    // Radial velocity components (positive = approaching)
    float v_listener = glm::dot(params.listener_velocity, direction);
    float v_source = glm::dot(params.source_velocity, direction);

    float c = params.speed_of_sound;

    // Clamp velocities to prevent division by zero / negative frequencies
    // Source moving toward listener at speed of sound would cause infinite pitch
    float max_v = c * 0.9f;
    v_source = std::clamp(v_source, -max_v, max_v);
    v_listener = std::clamp(v_listener, -max_v, max_v);

    // Doppler formula: f' = f * (c + v_listener) / (c + v_source)
    // Apply doppler_factor as an exponent for artistic control
    float ratio = (c + v_listener * params.doppler_factor)
                / (c + v_source * params.doppler_factor);

    // Clamp to reasonable range
    return std::clamp(ratio, 0.1f, 10.0f);
}

} // namespace nexus::audio
