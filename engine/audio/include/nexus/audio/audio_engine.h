#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include "nexus/audio/audio_buffer.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>

namespace nexus::audio {

// ─────────────────────────────────────────────────────────────────────────────
// Voice - a single playing instance of an audio clip
// ─────────────────────────────────────────────────────────────────────────────

using VoiceId = u32;
constexpr VoiceId INVALID_VOICE_ID = 0;

struct Voice {
    VoiceId      id{INVALID_VOICE_ID};
    AudioClipId  clip_id{INVALID_CLIP_ID};
    float        volume{1.0f};
    float        pitch{1.0f};
    float        pan{0.0f};        // -1 = left, 0 = center, 1 = right
    bool         looping{false};
    bool         paused{false};
    bool         spatial{false};
    Vec3         position{0.0f};   // for spatial audio
    float        min_distance{1.0f};
    float        max_distance{50.0f};
    float        rolloff{1.0f};

    // Internal playback state
    double       cursor{0.0};      // fractional frame position
    bool         finished{false};
    u32          bus_index{0};      // which bus this voice belongs to
};

// ─────────────────────────────────────────────────────────────────────────────
// AudioBus - a mixing channel (master, SFX, music, voice)
// ─────────────────────────────────────────────────────────────────────────────

struct AudioBus {
    std::string name;
    float       volume{1.0f};
    bool        muted{false};
};

// ─────────────────────────────────────────────────────────────────────────────
// AudioEvent - a named trigger that plays one or more sounds
// ─────────────────────────────────────────────────────────────────────────────

struct AudioEvent {
    std::string name;
    AudioClipId clip_id{INVALID_CLIP_ID};
    float       volume{1.0f};
    float       pitch{1.0f};
    bool        looping{false};
    u32         bus_index{1};       // default: SFX bus
};

// ─────────────────────────────────────────────────────────────────────────────
// AudioEngine - the central audio system
// ─────────────────────────────────────────────────────────────────────────────

class AudioEngine {
public:
    static constexpr u32 BUS_MASTER = 0;
    static constexpr u32 BUS_SFX    = 1;
    static constexpr u32 BUS_MUSIC  = 2;
    static constexpr u32 BUS_VOICE  = 3;

    AudioEngine();
    ~AudioEngine() = default;

    // ── Clip management ─────────────────────────────────────────────────────

    /// Load a WAV file and return its clip ID.
    AudioClipId load_clip(const std::string& name, const std::string& filepath);

    /// Load from an in-memory buffer.
    AudioClipId load_clip_from_buffer(const std::string& name, AudioBuffer buffer);

    /// Get a clip by ID.
    const AudioClip* get_clip(AudioClipId id) const;

    /// Get a clip by name.
    const AudioClip* get_clip_by_name(const std::string& name) const;

    /// Unload a clip.
    void unload_clip(AudioClipId id);

    // ── Playback ────────────────────────────────────────────────────────────

    /// Play a clip and return a voice handle.
    VoiceId play(AudioClipId clip, float volume = 1.0f, float pitch = 1.0f,
                 bool looping = false, u32 bus = BUS_SFX);

    /// Play a clip with 3D spatial positioning.
    VoiceId play_spatial(AudioClipId clip, Vec3 position, float volume = 1.0f,
                         float min_dist = 1.0f, float max_dist = 50.0f,
                         bool looping = false, u32 bus = BUS_SFX);

    /// Pause/unpause a voice.
    void pause(VoiceId id);
    void resume(VoiceId id);

    /// Stop a voice immediately.
    void stop(VoiceId id);

    /// Stop all voices.
    void stop_all();

    /// Set voice properties.
    void set_volume(VoiceId id, float volume);
    void set_pitch(VoiceId id, float pitch);
    void set_pan(VoiceId id, float pan);
    void set_position(VoiceId id, Vec3 position);
    void set_looping(VoiceId id, bool looping);

    /// Check if a voice is still playing.
    bool is_playing(VoiceId id) const;

    // ── Bus control ─────────────────────────────────────────────────────────

    void set_bus_volume(u32 bus, float volume);
    float get_bus_volume(u32 bus) const;
    void set_bus_muted(u32 bus, bool muted);
    bool is_bus_muted(u32 bus) const;
    void set_master_volume(float volume);
    float master_volume() const;

    // ── Listener (for spatial audio) ────────────────────────────────────────

    void set_listener_position(Vec3 pos) { listener_position_ = pos; }
    void set_listener_forward(Vec3 fwd) { listener_forward_ = fwd; }
    Vec3 listener_position() const { return listener_position_; }

    // ── Event system ────────────────────────────────────────────────────────

    void register_event(const std::string& name, AudioEvent event);
    VoiceId fire_event(const std::string& name);
    VoiceId fire_event_at(const std::string& name, Vec3 position);

    // ── Mixing / update ─────────────────────────────────────────────────────

    /// Mix all active voices into a stereo F32 output buffer.
    /// frames = number of stereo frames to produce.
    void mix(float* output, u32 frames);

    /// Advance playback and clean up finished voices. Call once per frame.
    void update();

    /// Number of currently active voices.
    u32 active_voice_count() const;

    /// Set the output sample rate (default 44100).
    void set_output_sample_rate(u32 rate) { output_sample_rate_ = rate; }
    u32 output_sample_rate() const { return output_sample_rate_; }

private:
    float compute_spatial_gain(const Voice& v) const;
    float compute_spatial_pan(const Voice& v) const;

    std::vector<AudioClip> clips_;
    std::vector<Voice>     voices_;
    std::vector<AudioBus>  buses_;
    std::unordered_map<std::string, AudioEvent> events_;
    std::unordered_map<std::string, AudioClipId> clip_name_map_;

    Vec3 listener_position_{0.0f};
    Vec3 listener_forward_{0.0f, 0.0f, -1.0f};

    u32 output_sample_rate_{44100};
    AudioClipId next_clip_id_{1};
    VoiceId     next_voice_id_{1};
    mutable std::mutex mutex_;
};

} // namespace nexus::audio
