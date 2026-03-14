#include <gtest/gtest.h>
#include <nexus/audio/audio_buffer.h>
#include <nexus/audio/audio_engine.h>
#include <cstring>
#include <cmath>

namespace nexus::audio::tests {

// ── Helper: generate a synthetic WAV in memory ──────────────────────────────

static std::vector<u8> make_wav_mono_s16(u32 sample_rate, u32 num_frames, float freq = 440.0f) {
    // Build a minimal WAV file in memory
    u32 data_size = num_frames * 2; // 16-bit mono = 2 bytes/frame
    u32 file_size = 44 + data_size;

    std::vector<u8> wav(file_size);
    auto write_u32 = [&](size_t off, uint32_t v) { std::memcpy(&wav[off], &v, 4); };
    auto write_u16 = [&](size_t off, uint16_t v) { std::memcpy(&wav[off], &v, 2); };

    // RIFF header
    std::memcpy(&wav[0], "RIFF", 4);
    write_u32(4, file_size - 8);
    std::memcpy(&wav[8], "WAVE", 4);

    // fmt chunk
    std::memcpy(&wav[12], "fmt ", 4);
    write_u32(16, 16);             // chunk size
    write_u16(20, 1);              // PCM format
    write_u16(22, 1);              // mono
    write_u32(24, sample_rate);
    write_u32(28, sample_rate * 2); // byte rate
    write_u16(32, 2);              // block align
    write_u16(34, 16);             // bits per sample

    // data chunk
    std::memcpy(&wav[36], "data", 4);
    write_u32(40, data_size);

    // Generate sine wave
    for (u32 i = 0; i < num_frames; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(sample_rate);
        float sample = std::sin(2.0f * 3.14159f * freq * t) * 0.5f;
        auto val = static_cast<int16_t>(sample * 32767.0f);
        std::memcpy(&wav[44 + i * 2], &val, 2);
    }

    return wav;
}

// ── AudioBuffer tests ───────────────────────────────────────────────────────

TEST(AudioBuffer, LoadWavFromMemory) {
    auto wav = make_wav_mono_s16(44100, 4410); // 0.1s

    AudioBuffer buf;
    EXPECT_TRUE(load_wav_from_memory(wav.data(), wav.size(), buf));
    EXPECT_EQ(buf.format.sample_rate, 44100u);
    EXPECT_EQ(buf.format.channels, 1u);
    EXPECT_EQ(buf.frame_count, 4410u);
    EXPECT_NEAR(buf.duration_seconds(), 0.1f, 0.001f);
}

TEST(AudioBuffer, ReadSample) {
    auto wav = make_wav_mono_s16(44100, 100, 440.0f);

    AudioBuffer buf;
    ASSERT_TRUE(load_wav_from_memory(wav.data(), wav.size(), buf));

    // First sample should be near 0 (sin(0))
    float s0 = buf.read_sample(0, 0);
    EXPECT_NEAR(s0, 0.0f, 0.01f);

    // Sample at quarter period should be near +0.5 (0.5 * sin(pi/2))
    u32 quarter_period = 44100 / 440 / 4;
    float sq = buf.read_sample(quarter_period, 0);
    EXPECT_GT(sq, 0.3f);
}

TEST(AudioBuffer, InvalidWavReturnsError) {
    u8 garbage[] = {0, 1, 2, 3, 4, 5, 6, 7};
    AudioBuffer buf;
    EXPECT_FALSE(load_wav_from_memory(garbage, sizeof(garbage), buf));
}

TEST(AudioBuffer, EmptyBuffer) {
    AudioBuffer buf;
    EXPECT_FLOAT_EQ(buf.duration_seconds(), 0.0f);
    EXPECT_FLOAT_EQ(buf.read_sample(0, 0), 0.0f);
}

// ── AudioEngine tests ───────────────────────────────────────────────────────

class AudioEngineTest : public ::testing::Test {
protected:
    AudioEngine engine;

    AudioClipId load_test_clip(u32 frames = 4410) {
        auto wav = make_wav_mono_s16(44100, frames);
        AudioBuffer buf;
        load_wav_from_memory(wav.data(), wav.size(), buf);
        return engine.load_clip_from_buffer("test", std::move(buf));
    }
};

TEST_F(AudioEngineTest, LoadClip) {
    auto id = load_test_clip();
    EXPECT_NE(id, INVALID_CLIP_ID);

    auto* clip = engine.get_clip(id);
    ASSERT_NE(clip, nullptr);
    EXPECT_EQ(clip->name, "test");
}

TEST_F(AudioEngineTest, GetClipByName) {
    load_test_clip();
    auto* clip = engine.get_clip_by_name("test");
    ASSERT_NE(clip, nullptr);
    EXPECT_EQ(clip->name, "test");

    EXPECT_EQ(engine.get_clip_by_name("nonexistent"), nullptr);
}

TEST_F(AudioEngineTest, PlayAndStop) {
    auto clip_id = load_test_clip();
    auto voice_id = engine.play(clip_id);

    EXPECT_NE(voice_id, INVALID_VOICE_ID);
    EXPECT_TRUE(engine.is_playing(voice_id));
    EXPECT_EQ(engine.active_voice_count(), 1u);

    engine.stop(voice_id);
    EXPECT_FALSE(engine.is_playing(voice_id));
}

TEST_F(AudioEngineTest, PauseAndResume) {
    auto clip_id = load_test_clip();
    auto voice_id = engine.play(clip_id);

    engine.pause(voice_id);
    EXPECT_FALSE(engine.is_playing(voice_id));

    engine.resume(voice_id);
    EXPECT_TRUE(engine.is_playing(voice_id));
}

TEST_F(AudioEngineTest, PlayInvalidClip) {
    auto voice_id = engine.play(999);
    EXPECT_EQ(voice_id, INVALID_VOICE_ID);
}

TEST_F(AudioEngineTest, StopAll) {
    auto clip_id = load_test_clip();
    engine.play(clip_id);
    engine.play(clip_id);
    engine.play(clip_id);
    EXPECT_EQ(engine.active_voice_count(), 3u);

    engine.stop_all();
    engine.update();
    EXPECT_EQ(engine.active_voice_count(), 0u);
}

TEST_F(AudioEngineTest, UnloadClip) {
    auto clip_id = load_test_clip();
    engine.play(clip_id);
    engine.unload_clip(clip_id);

    EXPECT_EQ(engine.get_clip(clip_id), nullptr);
}

TEST_F(AudioEngineTest, BusControl) {
    engine.set_bus_volume(AudioEngine::BUS_SFX, 0.5f);
    EXPECT_FLOAT_EQ(engine.get_bus_volume(AudioEngine::BUS_SFX), 0.5f);

    engine.set_bus_muted(AudioEngine::BUS_MUSIC, true);
    EXPECT_TRUE(engine.is_bus_muted(AudioEngine::BUS_MUSIC));
}

TEST_F(AudioEngineTest, MasterVolume) {
    engine.set_master_volume(0.75f);
    EXPECT_FLOAT_EQ(engine.master_volume(), 0.75f);
}

TEST_F(AudioEngineTest, SpatialPlay) {
    auto clip_id = load_test_clip();
    auto voice_id = engine.play_spatial(clip_id, {10.0f, 0.0f, 0.0f});

    EXPECT_NE(voice_id, INVALID_VOICE_ID);
    EXPECT_TRUE(engine.is_playing(voice_id));
}

TEST_F(AudioEngineTest, MixProducesOutput) {
    auto clip_id = load_test_clip(44100); // 1 second
    engine.play(clip_id, 1.0f, 1.0f, false, AudioEngine::BUS_SFX);

    std::vector<float> output(512 * 2, 0.0f); // 512 frames stereo
    engine.mix(output.data(), 512);

    // Check that we got non-zero output
    float max_val = 0.0f;
    for (float s : output) max_val = std::max(max_val, std::abs(s));
    EXPECT_GT(max_val, 0.0f);
}

TEST_F(AudioEngineTest, MutedBusProducesSilence) {
    auto clip_id = load_test_clip(44100);
    engine.play(clip_id, 1.0f, 1.0f, false, AudioEngine::BUS_SFX);
    engine.set_bus_muted(AudioEngine::BUS_SFX, true);

    std::vector<float> output(512 * 2, 0.0f);
    engine.mix(output.data(), 512);

    float max_val = 0.0f;
    for (float s : output) max_val = std::max(max_val, std::abs(s));
    EXPECT_FLOAT_EQ(max_val, 0.0f);
}

TEST_F(AudioEngineTest, VoiceFinishesWhenClipEnds) {
    auto clip_id = load_test_clip(100); // very short
    auto voice_id = engine.play(clip_id, 1.0f, 1.0f, false);

    // Mix enough frames to exhaust the clip
    std::vector<float> output(256 * 2, 0.0f);
    engine.mix(output.data(), 256);
    engine.update();

    EXPECT_FALSE(engine.is_playing(voice_id));
}

TEST_F(AudioEngineTest, LoopingVoiceContinues) {
    auto clip_id = load_test_clip(100);
    auto voice_id = engine.play(clip_id, 1.0f, 1.0f, true);

    std::vector<float> output(256 * 2, 0.0f);
    engine.mix(output.data(), 256);
    engine.update();

    EXPECT_TRUE(engine.is_playing(voice_id));
}

TEST_F(AudioEngineTest, EventSystem) {
    auto clip_id = load_test_clip();

    AudioEvent event;
    event.clip_id = clip_id;
    event.volume = 0.8f;
    event.bus_index = AudioEngine::BUS_SFX;
    engine.register_event("explosion", event);

    auto voice_id = engine.fire_event("explosion");
    EXPECT_NE(voice_id, INVALID_VOICE_ID);
    EXPECT_TRUE(engine.is_playing(voice_id));
}

TEST_F(AudioEngineTest, FireUnknownEventReturnsInvalid) {
    auto voice_id = engine.fire_event("nonexistent");
    EXPECT_EQ(voice_id, INVALID_VOICE_ID);
}

TEST_F(AudioEngineTest, SpatialEvent) {
    auto clip_id = load_test_clip();

    AudioEvent event;
    event.clip_id = clip_id;
    engine.register_event("footstep", event);

    auto voice_id = engine.fire_event_at("footstep", {5.0f, 0.0f, 0.0f});
    EXPECT_NE(voice_id, INVALID_VOICE_ID);
}

TEST_F(AudioEngineTest, SpatialAttenuationReducesVolume) {
    auto clip_id = load_test_clip(44100);

    // Play close
    engine.play_spatial(clip_id, {1.0f, 0.0f, 0.0f}, 1.0f, 1.0f, 50.0f);
    std::vector<float> output_close(512 * 2, 0.0f);
    engine.mix(output_close.data(), 512);

    float max_close = 0.0f;
    for (float s : output_close) max_close = std::max(max_close, std::abs(s));

    engine.stop_all();
    engine.update();

    // Play far
    engine.play_spatial(clip_id, {45.0f, 0.0f, 0.0f}, 1.0f, 1.0f, 50.0f);
    std::vector<float> output_far(512 * 2, 0.0f);
    engine.mix(output_far.data(), 512);

    float max_far = 0.0f;
    for (float s : output_far) max_far = std::max(max_far, std::abs(s));

    EXPECT_GT(max_close, max_far);
}

TEST_F(AudioEngineTest, SetVoiceProperties) {
    auto clip_id = load_test_clip();
    auto voice_id = engine.play(clip_id);

    engine.set_volume(voice_id, 0.5f);
    engine.set_pitch(voice_id, 1.5f);
    engine.set_pan(voice_id, -0.5f);
    engine.set_looping(voice_id, true);
    engine.set_position(voice_id, {1.0f, 2.0f, 3.0f});

    // No crash, voice still active
    EXPECT_TRUE(engine.is_playing(voice_id));
}

TEST_F(AudioEngineTest, ListenerPosition) {
    engine.set_listener_position({10.0f, 0.0f, 0.0f});
    engine.set_listener_forward({0.0f, 0.0f, -1.0f});
    EXPECT_FLOAT_EQ(engine.listener_position().x, 10.0f);
}

} // namespace nexus::audio::tests
