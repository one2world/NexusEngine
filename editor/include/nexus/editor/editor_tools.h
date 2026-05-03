#pragma once

#include "nexus/editor/panel.h"
#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include <vector>
#include <string>
#include <functional>

namespace nexus {
class Scene;
class Registry;
struct ParticleEmitterComponent;
}

namespace nexus::editor {

// ============================================================================
// TilemapEditorPanel - paint tiles, auto-tile, collision shapes
// ============================================================================

enum class TilemapBrushMode : u8 {
    Paint,
    Erase,
    Fill,
    Rectangle,
    Pick         // eyedropper
};

class TilemapEditorPanel : public Panel {
public:
    TilemapEditorPanel() : Panel("Tilemap Editor") {}
    void on_render() override;
    const char* type_id() const override { return "TilemapEditorPanel"; }

    void bind_scene(Scene* scene) { scene_ = scene; }
    void set_target_entity(u32 entity) { target_ = entity; has_target_ = true; }
    void clear_target() { has_target_ = false; target_ = 0; }

    TilemapBrushMode brush_mode() const { return brush_mode_; }
    void set_brush_mode(TilemapBrushMode m) { brush_mode_ = m; }

    i32 selected_tile() const { return selected_tile_; }
    void set_selected_tile(i32 tile) { selected_tile_ = tile; }

    u32 brush_size() const { return brush_size_; }
    void set_brush_size(u32 s) { brush_size_ = s; }

    bool show_grid() const { return show_grid_; }
    void set_show_grid(bool v) { show_grid_ = v; }

    /// Apply current brush at tile coordinate (x, y).
    void paint_at(u32 x, u32 y);

    /// Flood fill from (x, y).
    void flood_fill(u32 x, u32 y, i32 new_tile);

private:
    Scene* scene_{nullptr};
    u32 target_{0};
    bool has_target_{false};
    TilemapBrushMode brush_mode_{TilemapBrushMode::Paint};
    i32 selected_tile_{0};
    u32 brush_size_{1};
    bool show_grid_{true};
    [[maybe_unused]] float zoom_{1.0f};
    Vec2 scroll_offset_{0.0f, 0.0f};
};

// ============================================================================
// AnimationEditorPanel - keyframe timeline with curves
// ============================================================================

struct AnimKeyframe {
    float time{0.0f};
    float value{0.0f};
    float in_tangent{0.0f};
    float out_tangent{0.0f};
};

struct AnimTrack {
    std::string property_name;
    std::vector<AnimKeyframe> keyframes;
};

enum class TimelineMode : u8 {
    Dopesheet,
    CurveEditor
};

class AnimationEditorPanel : public Panel {
public:
    AnimationEditorPanel() : Panel("Animation") {}
    void on_render() override;
    const char* type_id() const override { return "AnimationEditorPanel"; }

    void set_tracks(std::vector<AnimTrack> tracks) { tracks_ = std::move(tracks); }
    const std::vector<AnimTrack>& tracks() const { return tracks_; }

    float current_time() const { return current_time_; }
    void set_current_time(float t) { current_time_ = t; }

    float duration() const { return duration_; }
    void set_duration(float d) { duration_ = d; }

    bool is_playing() const { return playing_; }
    void set_playing(bool p) { playing_ = p; }
    void toggle_play() { playing_ = !playing_; }

    TimelineMode mode() const { return mode_; }
    void set_mode(TimelineMode m) { mode_ = m; }

    float zoom() const { return zoom_; }
    void set_zoom(float z) { zoom_ = z; }

    /// Add a keyframe at current_time for the given track.
    void add_keyframe(u32 track_index, float value);

    /// Delete keyframe nearest to current_time on the given track.
    void delete_keyframe(u32 track_index);

    /// Sample track value at time t using cubic interpolation.
    static float sample_track(const AnimTrack& track, float t);

private:
    std::vector<AnimTrack> tracks_;
    float current_time_{0.0f};
    float duration_{1.0f};
    bool playing_{false};
    TimelineMode mode_{TimelineMode::Dopesheet};
    float zoom_{100.0f};  // pixels per second
    [[maybe_unused]] float scroll_x_{0.0f};
    i32 selected_track_{-1};
    [[maybe_unused]] i32 selected_keyframe_{-1};
};

// ============================================================================
// ParticleEditorPanel - real-time preview with presets
// ============================================================================

struct ParticlePreset {
    std::string name;
    Vec3 direction{0, 1, 0};
    float spread{45.0f};
    float min_speed{1.0f};
    float max_speed{5.0f};
    float min_lifetime{1.0f};
    float max_lifetime{3.0f};
    Vec4 start_color{1.0f};
    Vec4 end_color{1.0f, 1.0f, 1.0f, 0.0f};
    float start_size{0.1f};
    float end_size{0.0f};
    float gravity{-9.81f};
    float emission_rate{1000.0f};
    bool additive{true};
};

class ParticleEditorPanel : public Panel {
public:
    ParticleEditorPanel() : Panel("Particle Editor") {}
    void on_render() override;
    const char* type_id() const override { return "ParticleEditorPanel"; }

    ParticlePreset& current_preset() { return current_; }
    const ParticlePreset& current_preset() const { return current_; }
    void set_current_preset(const ParticlePreset& p) { current_ = p; }

    void add_preset(const ParticlePreset& p) { presets_.push_back(p); }
    const std::vector<ParticlePreset>& presets() const { return presets_; }
    void clear_presets() { presets_.clear(); selected_preset_ = -1; }
    u32 preset_count() const { return static_cast<u32>(presets_.size()); }
    void load_preset(u32 index);
    i32 selected_preset_index() const { return selected_preset_; }

    bool preview_active() const { return preview_active_; }
    void set_preview_active(bool v) { preview_active_ = v; }

    u32 alive_count() const { return alive_count_; }
    void set_alive_count(u32 c) { alive_count_ = c; }

    // ── Persistence (M18) ────────────────────────────────────────────────
    /// Serialise every preset (and the current working preset) into a
    /// JSON document.  Round-trip-safe: load_from_json() reconstructs the
    /// exact same state.
    std::string save_to_json() const;
    /// Replace the panel's preset list + current preset with the contents
    /// of a JSON document.  Atomic: returns false on parse failure with
    /// no partial mutation.
    bool load_from_json(const std::string& json);
    bool save_to_file(const std::string& path) const;
    bool load_from_file(const std::string& path);

    // ── Pure helpers (test-friendly) ─────────────────────────────────────
    /// Validate + repair a preset in-place.  Negatives clamp to 0; min/max
    /// pairs swap when reversed; emission_rate < 0 → 0.  Idempotent.
    static void sanitize(ParticlePreset& p);

    /// Return a vector of canonical built-in presets (Fire, Smoke, Sparks,
    /// Magic).  Used by `seed_builtin_presets` and by tests verifying
    /// that the catalogue stays consistent.
    static std::vector<ParticlePreset> builtin_presets();

    /// Replace the current preset list with the canonical built-ins.
    /// Useful when the panel is first registered and the user has no
    /// saved JSON yet.
    void seed_builtin_presets() { presets_ = builtin_presets(); }

    /// Pure helper that converts an editor-side ParticlePreset into the
    /// engine-side ParticleEmitterComponent the runtime
    /// ParticleEmitterSystem expects.  Used by both "Apply to Selected
    /// Entity" and tests so the field mapping stays in one place.
    /// Direction / spread / additive don't have a 1:1 mapping in the
    /// component yet (component drives a fixed +Y cone with stochastic
    /// X/Z jitter); they're reserved for a future emitter shape pass.
    static ParticleEmitterComponent preset_to_component(const ParticlePreset& p);

    /// Apply the panel's currently-active preset to a host-supplied
    /// entity.  The host wires this via `set_apply_to_entity_callback`
    /// when the user clicks "Apply to Selected Entity"; if the
    /// callback isn't bound the button is hidden so users don't get
    /// silent no-ops.  Returns true when the callback fires.
    using ApplyCallback =
        std::function<bool(const ParticlePreset& preset)>;
    void set_apply_to_entity_callback(ApplyCallback cb) {
        on_apply_to_entity_ = std::move(cb);
    }
    bool has_apply_to_entity_callback() const {
        return static_cast<bool>(on_apply_to_entity_);
    }

private:
    ParticlePreset current_;
    std::vector<ParticlePreset> presets_;
    bool preview_active_{true};
    u32 alive_count_{0};
    i32 selected_preset_{-1};
    ApplyCallback on_apply_to_entity_;
};

// ============================================================================
// MaterialEditorPanel - property panel with live preview
// ============================================================================

struct MaterialProperty {
    enum Type : u8 { Float, Vec2, Vec3, Vec4, Color, Texture, Bool };

    std::string name;
    Type type{Float};
    float float_val{0.0f};
    nexus::Vec2 vec2_val{0.0f};
    nexus::Vec3 vec3_val{0.0f};
    nexus::Vec4 vec4_val{1.0f};
    std::string texture_path;
    bool bool_val{false};
};

class MaterialEditorPanel : public Panel {
public:
    MaterialEditorPanel() : Panel("Material Editor") {}
    void on_render() override;
    const char* type_id() const override { return "MaterialEditorPanel"; }

    void set_material_name(const std::string& name) { material_name_ = name; }
    const std::string& material_name() const { return material_name_; }

    void set_shader_name(const std::string& name) { shader_name_ = name; }
    const std::string& shader_name() const { return shader_name_; }

    void set_properties(std::vector<MaterialProperty> props) { properties_ = std::move(props); }
    std::vector<MaterialProperty>& properties() { return properties_; }
    const std::vector<MaterialProperty>& properties() const { return properties_; }

    bool is_modified() const { return modified_; }
    void clear_modified() { modified_ = false; }

    using SaveCallback = std::function<void(const std::vector<MaterialProperty>&)>;
    void set_on_save(SaveCallback cb) { on_save_ = std::move(cb); }

private:
    std::string material_name_;
    std::string shader_name_;
    std::vector<MaterialProperty> properties_;
    bool modified_{false};
    SaveCallback on_save_;
};

} // namespace nexus::editor
