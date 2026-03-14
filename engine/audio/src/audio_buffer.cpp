#include "nexus/audio/audio_buffer.h"
#include "nexus/core/log.h"
#include <fstream>
#include <cstring>

namespace nexus::audio {

float AudioBuffer::read_sample(u32 frame, u32 channel) const {
    if (frame >= frame_count || channel >= format.channels) return 0.0f;

    u32 sample_index = frame * format.channels + channel;
    u32 byte_offset = sample_index * format.bytes_per_sample();
    if (byte_offset + format.bytes_per_sample() > data.size()) return 0.0f;

    const u8* ptr = data.data() + byte_offset;

    switch (format.format) {
        case SampleFormat::U8:
            return (static_cast<float>(*ptr) - 128.0f) / 128.0f;
        case SampleFormat::S16: {
            int16_t val;
            std::memcpy(&val, ptr, sizeof(val));
            return static_cast<float>(val) / 32768.0f;
        }
        case SampleFormat::S32: {
            int32_t val;
            std::memcpy(&val, ptr, sizeof(val));
            return static_cast<float>(val) / 2147483648.0f;
        }
        case SampleFormat::F32: {
            float val;
            std::memcpy(&val, ptr, sizeof(val));
            return val;
        }
    }
    return 0.0f;
}

// ── WAV loader ──────────────────────────────────────────────────────────────

#pragma pack(push, 1)
struct WavHeader {
    char     riff_tag[4];
    uint32_t riff_size;
    char     wave_tag[4];
};

struct WavChunkHeader {
    char     id[4];
    uint32_t size;
};

struct WavFmtChunk {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
};
#pragma pack(pop)

bool load_wav_from_memory(const u8* data, size_t size, AudioBuffer& out_buffer) {
    if (size < sizeof(WavHeader)) {
        NX_ERROR("WAV: file too small");
        return false;
    }

    const auto* header = reinterpret_cast<const WavHeader*>(data);
    if (std::memcmp(header->riff_tag, "RIFF", 4) != 0 ||
        std::memcmp(header->wave_tag, "WAVE", 4) != 0) {
        NX_ERROR("WAV: invalid header");
        return false;
    }

    size_t offset = sizeof(WavHeader);
    const WavFmtChunk* fmt = nullptr;
    const u8* pcm_data = nullptr;
    uint32_t pcm_size = 0;

    while (offset + sizeof(WavChunkHeader) <= size) {
        const auto* chunk = reinterpret_cast<const WavChunkHeader*>(data + offset);
        size_t chunk_data_offset = offset + sizeof(WavChunkHeader);

        if (std::memcmp(chunk->id, "fmt ", 4) == 0) {
            if (chunk_data_offset + sizeof(WavFmtChunk) <= size) {
                fmt = reinterpret_cast<const WavFmtChunk*>(data + chunk_data_offset);
            }
        } else if (std::memcmp(chunk->id, "data", 4) == 0) {
            pcm_data = data + chunk_data_offset;
            pcm_size = chunk->size;
        }

        offset = chunk_data_offset + chunk->size;
        if (offset % 2 != 0) ++offset; // WAV chunks are 2-byte aligned
    }

    if (!fmt || !pcm_data) {
        NX_ERROR("WAV: missing fmt or data chunk");
        return false;
    }

    // Only support PCM (1) and IEEE float (3)
    if (fmt->audio_format != 1 && fmt->audio_format != 3) {
        NX_ERROR("WAV: unsupported audio format {}", fmt->audio_format);
        return false;
    }

    out_buffer.format.sample_rate = fmt->sample_rate;
    out_buffer.format.channels = fmt->num_channels;

    if (fmt->audio_format == 3) {
        out_buffer.format.format = SampleFormat::F32;
    } else {
        switch (fmt->bits_per_sample) {
            case 8:  out_buffer.format.format = SampleFormat::U8;  break;
            case 16: out_buffer.format.format = SampleFormat::S16; break;
            case 32: out_buffer.format.format = SampleFormat::S32; break;
            default:
                NX_ERROR("WAV: unsupported bits per sample {}", fmt->bits_per_sample);
                return false;
        }
    }

    size_t actual_size = std::min(static_cast<size_t>(pcm_size),
                                  size - static_cast<size_t>(pcm_data - data));
    out_buffer.data.assign(pcm_data, pcm_data + actual_size);
    out_buffer.frame_count = static_cast<u32>(actual_size / out_buffer.format.frame_size());

    return true;
}

bool load_wav(const std::string& filepath, AudioBuffer& out_buffer) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        NX_ERROR("WAV: cannot open file: {}", filepath);
        return false;
    }

    auto file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<u8> file_data(static_cast<size_t>(file_size));
    file.read(reinterpret_cast<char*>(file_data.data()), file_size);

    return load_wav_from_memory(file_data.data(), file_data.size(), out_buffer);
}

} // namespace nexus::audio
