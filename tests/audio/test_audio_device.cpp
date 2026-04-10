#include <gtest/gtest.h>
#include <nexus/audio/audio_device.h>
#include <nexus/audio/audio_engine.h>
#include <nexus/audio/audio_buffer.h>
#include <cstring>
#include <cmath>

namespace nexus::audio::tests {

// ── Helper: generate a synthetic WAV in memory ──────────────────────────────

static std::vector<u8> make_wav_mono_s16(u32 sample_rate, u32 num_frames, float freq = 440.0f) {
    u32 data_size = num_frames * 2;
    u32 file_size = 44 + data_size;

    std::vector<u8> wav(file_size);
    auto write_u32 = [&](size_t off, uint32_t v) { std::memcpy(&wav[off], &v, 4); };
    auto write_u16 = [&](size_t off, uint16_t v) { std::memcpy(&wav[off], &v, 2); };

    std::memcpy(&wav[0], "RIFF", 4);
    write_u32(4, file_size - 8);
    std::memcpy(&wav[8], "WAVE", 4);
    std::memcpy(&wav[12], "fmt ", 4);
    write_u32(16, 16);
    write_u16(20, 1);
    write_u16(22, 1);
    write_u32(24, sample_rate);
    write_u32(28, sample_rate * 2);
    write_u16(32, 2);
    write_u16(34, 16);
    std::memcpy(&wav[36], "data", 4);
    write_u32(40, data_size);

    for (u32 i = 0; i < num_frames; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(sample_rate);
        float sample = std::sin(2.0f * 3.14159f * freq * t) * 0.5f;
        auto val = static_cast<int16_t>(sample * 32767.0f);
        std::memcpy(&wav[44 + i * 2], &val, 2);
    }

    return wav;
}

// ── AudioDevice unit tests ──────────────────────────────────────────────────

TEST(AudioDevice, DefaultConstructor) {
    AudioDevice device;
    EXPECT_FALSE(device.is_open());
    EXPECT_FALSE(device.is_started());
    EXPECT_EQ(device.actual_sample_rate(), 0u);
    EXPECT_EQ(device.backend_name(), "none");
}

TEST(AudioDevice, OpenWithNullEngineReturnsError) {
    AudioDevice device;
    EXPECT_FALSE(device.open(nullptr));
    EXPECT_FALSE(device.is_open());
}

TEST(AudioDevice, OpenAndClose) {
    AudioEngine engine;
    AudioDevice device;

    AudioDeviceConfig config;
    config.sample_rate = 44100;
    config.channels = 2;
    config.buffer_frames = 256;

    // On CI / headless systems, the device may fail to open (no audio hardware).
    // That's acceptable — we verify the API handles it gracefully.
    bool opened = device.open(&engine, config);
    if (opened) {
        EXPECT_TRUE(device.is_open());
        EXPECT_TRUE(device.is_started()); // auto-starts
        EXPECT_GT(device.actual_sample_rate(), 0u);
        EXPECT_NE(device.backend_name(), "none");

        device.stop();
        EXPECT_FALSE(device.is_started());
        EXPECT_TRUE(device.is_open()); // still open, just stopped

        device.close();
        EXPECT_FALSE(device.is_open());
        EXPECT_FALSE(device.is_started());
    } else {
        // Graceful failure — no crash, no leak
        EXPECT_FALSE(device.is_open());
        EXPECT_FALSE(device.is_started());
    }
}

TEST(AudioDevice, DoubleOpenReturnsFalse) {
    AudioEngine engine;
    AudioDevice device;

    bool first = device.open(&engine);
    if (first) {
        // Second open should return false (already open)
        EXPECT_FALSE(device.open(&engine));
        device.close();
    }
}

TEST(AudioDevice, CloseWithoutOpenIsSafe) {
    AudioDevice device;
    device.close(); // should not crash
    EXPECT_FALSE(device.is_open());
}

TEST(AudioDevice, StopWithoutStartIsSafe) {
    AudioDevice device;
    device.stop(); // should not crash
    EXPECT_FALSE(device.is_started());
}

TEST(AudioDevice, DestructorCleansUp) {
    AudioEngine engine;
    {
        AudioDevice device;
        device.open(&engine); // may or may not succeed on CI
        // destructor should clean up without crash or leak
    }
    // If we get here, destructor worked correctly
    SUCCEED();
}

TEST(AudioDevice, ConfigDefaultValues) {
    AudioDeviceConfig config;
    EXPECT_EQ(config.sample_rate, 44100u);
    EXPECT_EQ(config.channels, 2u);
    EXPECT_EQ(config.buffer_frames, 512u);
}

TEST(AudioDevice, EngineIntegrationWithDevice) {
    // Verify that AudioEngine + AudioDevice work together end-to-end.
    // Load a clip, play it, open a device — the device callback will call mix().
    AudioEngine engine;

    auto wav = make_wav_mono_s16(44100, 44100); // 1 second of 440Hz
    AudioBuffer buf;
    ASSERT_TRUE(load_wav_from_memory(wav.data(), wav.size(), buf));
    auto clip_id = engine.load_clip_from_buffer("tone", std::move(buf));
    ASSERT_NE(clip_id, INVALID_CLIP_ID);

    auto voice_id = engine.play(clip_id);
    EXPECT_TRUE(engine.is_playing(voice_id));

    AudioDevice device;
    bool opened = device.open(&engine);
    if (opened) {
        // Device is running and calling mix() in its audio thread.
        // Verify the engine's sample rate was synced to the device.
        EXPECT_EQ(engine.output_sample_rate(), device.actual_sample_rate());

        // The voice should still be active (1 second of audio).
        EXPECT_TRUE(engine.is_playing(voice_id));

        device.close();
    }

    engine.stop_all();
    engine.update();
}

} // namespace nexus::audio::tests
