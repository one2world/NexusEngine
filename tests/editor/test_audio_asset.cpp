#include <gtest/gtest.h>

#include "nexus/editor/audio_asset.h"
#include "nexus/audio/audio_engine.h"

#include <cstdio>
#include <filesystem>
#include <vector>

namespace nexus::editor::tests {

namespace fs = std::filesystem;

namespace {

fs::path tmp_path(const char* name) {
    return fs::temp_directory_path() / name;
}

// Write a tiny but valid WAV file (44.1 kHz mono, 8 samples of silence).
// Just enough header + payload that miniaudio / nexus-audio decoders
// accept it as a real clip.  The test only cares that load_clip succeeds,
// not the audio content itself.
bool write_test_wav(const fs::path& path) {
    std::FILE* f = std::fopen(path.string().c_str(), "wb");
    if (!f) return false;
    const u32 sample_rate = 44100;
    const u16 channels = 1;
    const u16 bits = 16;
    const u32 sample_count = 8;
    const u32 data_size = sample_count * channels * (bits / 8);
    const u32 byte_rate = sample_rate * channels * (bits / 8);
    const u16 block_align = channels * (bits / 8);
    const u32 file_size = 36 + data_size;

    auto write_u32 = [&](u32 v) {
        u8 b[4] = {static_cast<u8>(v & 0xFF),
                   static_cast<u8>((v >> 8) & 0xFF),
                   static_cast<u8>((v >> 16) & 0xFF),
                   static_cast<u8>((v >> 24) & 0xFF)};
        std::fwrite(b, 1, 4, f);
    };
    auto write_u16 = [&](u16 v) {
        u8 b[2] = {static_cast<u8>(v & 0xFF),
                   static_cast<u8>((v >> 8) & 0xFF)};
        std::fwrite(b, 1, 2, f);
    };

    std::fwrite("RIFF", 1, 4, f);
    write_u32(file_size);
    std::fwrite("WAVE", 1, 4, f);
    std::fwrite("fmt ", 1, 4, f);
    write_u32(16);                  // PCM fmt chunk size
    write_u16(1);                   // PCM
    write_u16(channels);
    write_u32(sample_rate);
    write_u32(byte_rate);
    write_u16(block_align);
    write_u16(bits);
    std::fwrite("data", 1, 4, f);
    write_u32(data_size);
    std::vector<u8> silence(data_size, 0);
    std::fwrite(silence.data(), 1, data_size, f);
    std::fclose(f);
    return true;
}

}  // namespace

// ── No-op without engine ────────────────────────────────────────────────────

TEST(AudioAssetCache, ImportWithoutEngineReturnsInvalid) {
    AudioAssetCache cache;
    EXPECT_EQ(cache.import("anywhere.wav"), INVALID_CLIP_ID);
    EXPECT_EQ(cache.size(), 0u);
}

TEST(AudioAssetCache, ImportEmptyPathReturnsInvalid) {
    nexus::audio::AudioEngine engine;
    AudioAssetCache cache;
    cache.set_audio_engine(&engine);
    EXPECT_EQ(cache.import(""), INVALID_CLIP_ID);
}

// ── Real WAV import + caching ───────────────────────────────────────────────

TEST(AudioAssetCache, ImportValidWAVProducesClipId) {
    const fs::path p = tmp_path("nexus_audio_test.wav");
    ASSERT_TRUE(write_test_wav(p));

    nexus::audio::AudioEngine engine;
    AudioAssetCache cache;
    cache.set_audio_engine(&engine);

    const auto id = cache.import(p.string());
    EXPECT_NE(id, INVALID_CLIP_ID);
    EXPECT_EQ(cache.size(), 1u);
    EXPECT_TRUE(cache.contains(p.string()));
    EXPECT_EQ(cache.path_for(id), p.string());

    fs::remove(p);
}

TEST(AudioAssetCache, RepeatedImportIsCachedSameId) {
    const fs::path p = tmp_path("nexus_audio_cache.wav");
    ASSERT_TRUE(write_test_wav(p));

    nexus::audio::AudioEngine engine;
    AudioAssetCache cache;
    cache.set_audio_engine(&engine);

    const auto a = cache.import(p.string());
    const auto b = cache.import(p.string());
    EXPECT_EQ(a, b);
    EXPECT_EQ(cache.size(), 1u);

    fs::remove(p);
}

// ── Failure paths ───────────────────────────────────────────────────────────

TEST(AudioAssetCache, ImportMissingFileReturnsInvalid) {
    nexus::audio::AudioEngine engine;
    AudioAssetCache cache;
    cache.set_audio_engine(&engine);
    EXPECT_EQ(cache.import("/nope/missing.wav"), INVALID_CLIP_ID);
}

TEST(AudioAssetCache, ImportFailureIsRememberedSuppressingRetries) {
    // Failed-import path should write an INVALID_CLIP_ID into the cache so
    // the next call doesn't re-hit AudioEngine.  Verifying via size() —
    // failed paths still occupy a slot.
    nexus::audio::AudioEngine engine;
    AudioAssetCache cache;
    cache.set_audio_engine(&engine);
    EXPECT_EQ(cache.import("/no/file/here.wav"), INVALID_CLIP_ID);
    EXPECT_TRUE(cache.contains("/no/file/here.wav"));
    EXPECT_EQ(cache.id_for("/no/file/here.wav"), INVALID_CLIP_ID);
}

// ── id_for / path_for queries ───────────────────────────────────────────────

TEST(AudioAssetCache, IdForUnimportedPathReturnsInvalid) {
    AudioAssetCache cache;
    EXPECT_EQ(cache.id_for("/proj/never.wav"), INVALID_CLIP_ID);
}

TEST(AudioAssetCache, PathForUnknownIdReturnsEmpty) {
    AudioAssetCache cache;
    EXPECT_EQ(cache.path_for(0xDEADu), "");
    EXPECT_EQ(cache.path_for(INVALID_CLIP_ID), "");
}

}  // namespace nexus::editor::tests
