#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"

#include <string>
#include <vector>

namespace nexus {

// ---------------------------------------------------------------------------
// ActiveComponent - controls whether an entity is active (updated/rendered)
// ---------------------------------------------------------------------------
struct ActiveComponent {
    bool active{true};
};

// ---------------------------------------------------------------------------
// TagComponent - human-readable name for an entity, plus Unity-style
// category Tag (e.g. "Player", "MainCamera") and Layer index for game-side
// filtering. Defaults preserve scenes that pre-date these fields.
// ---------------------------------------------------------------------------
struct TagComponent {
    std::string name;
    std::string category{"Untagged"};
    i32 layer{0};
};

// ---------------------------------------------------------------------------
// LockedComponent — marker. Editor forbids transform gizmo + inspector edits
// on any entity carrying this component. Matches Unity's "SceneVis Lock"
// behavior. Presence alone is meaningful — no data is stored.
// ---------------------------------------------------------------------------
struct LockedComponent {};

// ---------------------------------------------------------------------------
// Transform2DComponent
// ---------------------------------------------------------------------------
struct Transform2DComponent {
    Vec2  position{0.0f, 0.0f};
    float rotation{0.0f}; // radians
    Vec2  scale{1.0f, 1.0f};

    // World-space transform computed by Hierarchy::propagate_transforms_2d.
    Vec2  world_position{0.0f, 0.0f};
    float world_rotation{0.0f};
    Vec2  world_scale{1.0f, 1.0f};
};

// ---------------------------------------------------------------------------
// Transform3DComponent
// ---------------------------------------------------------------------------
struct Transform3DComponent {
    Vec3 position{0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f, 1.0f, 1.0f};

    // World-space matrix computed by Hierarchy::propagate_transforms_3d.
    Mat4 world_matrix{1.0f};

    [[nodiscard]] Mat4 to_matrix() const {
        Mat4 m(1.0f);
        m = glm::translate(m, position);
        m *= glm::toMat4(rotation);
        m = glm::scale(m, scale);
        return m;
    }
};

// ---------------------------------------------------------------------------
// SpriteRendererComponent
// ---------------------------------------------------------------------------
struct SpriteRendererComponent {
    u32  texture_id  = 0;
    Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    Vec2 size{1.0f, 1.0f};       // sprite size in local units
    Vec2 uv_min{0.0f, 0.0f};
    Vec2 uv_max{1.0f, 1.0f};
    i32  sort_order  = 0;
};

// ---------------------------------------------------------------------------
// MeshRendererComponent — Unity-parity surface for mesh rendering knobs.
//   Lighting:           cast/receive shadows toggles
//   Probes:             light + reflection probe blending modes
//   Additional:         dynamic occlusion (frustum / hi-Z culling)
// All defaults match Unity's "lit mesh" out-of-box behavior so existing
// scenes round-trip without surprise.
// ---------------------------------------------------------------------------
enum class LightProbesMode : u8 {
    Off = 0,
    BlendProbes,
    UseProxyVolume,
    Custom,
};

enum class ReflectionProbesMode : u8 {
    Off = 0,
    BlendProbes,
    BlendProbesAndSkybox,
    Simple,
};

struct MeshRendererComponent {
    u32  mesh_id     = 0;
    u32  material_id = 0;
    Vec4 tint{1.0f, 1.0f, 1.0f, 1.0f};  // multiplied with the material's albedo
    bool cast_shadows{true};
    bool receive_shadows{true};
    LightProbesMode      light_probes{LightProbesMode::BlendProbes};
    ReflectionProbesMode reflection_probes{ReflectionProbesMode::BlendProbes};
    bool dynamic_occlusion{true};
};

// ---------------------------------------------------------------------------
// CameraComponent
// ---------------------------------------------------------------------------
struct CameraComponent {
    bool  is_primary      = false;
    bool  is_orthographic = true;
    float fov             = 45.0f;
    float ortho_size      = 10.0f;
    float near_clip       = 0.1f;
    float far_clip        = 1000.0f;
    Quat  orientation{1.0f, 0.0f, 0.0f, 0.0f};
};

// ---------------------------------------------------------------------------
// DirectionalLightComponent
// ---------------------------------------------------------------------------
struct DirectionalLightComponent {
    Vec3  direction{-0.2f, -1.0f, -0.3f};
    Vec3  color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    // Shadow toggle.  When true the renderer's ShadowSystem includes
    // this light in its CSM depth pass and the main forward shader
    // multiplies its contribution by the shadow factor.  Off-by-default
    // would be the wrong UX (sun = wants shadows); on-by-default mirrors
    // Unity / Unreal.
    bool  cast_shadows = true;
};

// ---------------------------------------------------------------------------
// PointLightComponent
// ---------------------------------------------------------------------------
struct PointLightComponent {
    Vec3  color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float radius    = 10.0f;
    // Shadow toggle.  Off by default because point-light cubemap
    // shadows are 6× the cost of directional CSM and many small
    // accent lights don't visually need them.  Set true on hero lights.
    bool  cast_shadows = false;
};

// ---------------------------------------------------------------------------
// RigidBody2DComponent
// ---------------------------------------------------------------------------
struct RigidBody2DComponent {
    enum Type : u8 { Static, Dynamic, Kinematic };

    Type  type           = Dynamic;
    float density        = 1.0f;
    float friction       = 0.3f;
    float restitution    = 0.0f;
    float linear_damping = 0.0f;
    float angular_damping = 0.05f;
    float gravity_scale  = 1.0f;
    bool  fixed_rotation = false;
    Vec2  velocity{0.0f, 0.0f};
    float angular_velocity = 0.0f;
};

// ---------------------------------------------------------------------------
// Collider2DComponent - 2D collision shape attached to an entity
// ---------------------------------------------------------------------------
struct Collider2DComponent {
    enum Shape : u8 { Box, Circle, Polygon };

    Shape shape     = Box;
    Vec2  offset{0.0f, 0.0f};     // local offset from entity position
    Vec2  half_size{0.5f, 0.5f};  // for Box
    float radius    = 0.5f;        // for Circle
    bool  is_trigger = false;
    u16   layer     = 1;           // collision layer bitmask
    u16   mask      = 0xFFFF;      // which layers to collide with
};

// ---------------------------------------------------------------------------
// RigidBody3DComponent
// ---------------------------------------------------------------------------
struct RigidBody3DComponent {
    enum Type : u8 { Static, Dynamic, Kinematic };

    Type  type           = Dynamic;
    float mass           = 1.0f;
    float friction       = 0.5f;
    float restitution    = 0.3f;
    float linear_damping = 0.0f;
    float angular_damping = 0.05f;
    float gravity_scale  = 1.0f;
    Vec3  velocity{0.0f};
    Vec3  angular_velocity{0.0f};
};

// ---------------------------------------------------------------------------
// Collider3DComponent - 3D collision shape attached to an entity
// ---------------------------------------------------------------------------
struct Collider3DComponent {
    enum Shape : u8 { Box, Sphere, Capsule };

    Shape shape       = Box;
    Vec3  offset{0.0f};            // local offset
    Vec3  half_extents{0.5f};      // for Box
    float radius      = 0.5f;      // for Sphere/Capsule
    float height      = 1.0f;      // for Capsule (total height)
    bool  is_trigger   = false;
    u16   layer       = 1;
    u16   mask        = 0xFFFF;
};

// ---------------------------------------------------------------------------
// AudioSourceComponent - an entity that emits sound
// ---------------------------------------------------------------------------
struct AudioSourceComponent {
    u32   clip_id{0};          // AudioClipId
    u32   voice_id{0};         // current VoiceId (0 = not playing)
    float volume{1.0f};
    float pitch{1.0f};
    float min_distance{1.0f};
    float max_distance{50.0f};
    bool  looping{false};
    bool  spatial{true};       // 3D positional audio
    bool  play_on_start{false};
    u32   bus{1};              // default: SFX
};

// ---------------------------------------------------------------------------
// AudioListenerComponent - marks entity as the audio listener
// ---------------------------------------------------------------------------
struct AudioListenerComponent {
    bool active{true};
};

// ---------------------------------------------------------------------------
// AnimatorComponent — drives an entity's Transform3D from an AnimationClip
// ---------------------------------------------------------------------------
//
// Bridges the editor's AnimationAssetCache (which holds a stable id per
// .anim path) to the runtime animation system.  When a system tick fires,
// it samples the clip's bone-0 channel at `time` and writes the resulting
// pose into Transform3DComponent — covering Unity's "animate this object's
// transform over time" workflow without requiring a SkeletonComponent.
//
//   • clip_id     — id allocated by AnimationAssetCache; 0 = no clip.
//   • time        — current playhead in seconds.
//   • speed       — playback rate multiplier.  1.0 = real time.
//   • playing     — gate; system only advances when true.
//   • looping     — when true, time wraps modulo duration.
//   • play_on_start — animator system flips `playing=true` once when the
//     scene enters Play mode (Unity convention).
struct AnimatorComponent {
    u32   clip_id{0};
    f32   time{0.0f};
    f32   speed{1.0f};
    bool  playing{false};
    bool  looping{true};
    bool  play_on_start{true};
};

// ---------------------------------------------------------------------------
// SkeletonComponent — wires entity bones to AnimationClip channels (M20)
// ---------------------------------------------------------------------------
//
// AnimatorComponent's single-transform path (bone 0 → entity Transform3D)
// is fine for "animate this object" workflows but can't drive a skeletal
// rig where each bone is its own entity in the hierarchy.  This component
// closes that gap:
//
//   bone_entities[i] = entity id whose Transform3D holds bone i's local
//                      pose.  Index 0 is the root; subsequent indices
//                      address children in any order — the runtime
//                      doesn't assume a specific tree shape.
//
// AnimatorSystem detects this component on the same entity that owns
// AnimatorComponent and, when present, writes each `clip.channels()[i]`
// sample into the matching bone entity's Transform3D rather than
// collapsing everything to the entity's own transform.
//
// Deliberately a thin POD — does NOT own a Skeleton object.  Runtime
// resources (Skeleton, skinning matrices) live on the renderer side and
// are looked up by clip / mesh asset ids elsewhere.
struct SkeletonComponent {
    /// One entity id per bone, indexed 0..bone_count-1.  Empty vector
    /// disables multi-bone driving (runtime falls back to AnimatorComponent's
    /// single-transform path).
    std::vector<u32> bone_entities;
};

// ---------------------------------------------------------------------------
// ParticleEmitterComponent — ECS-driven particle emitter (M21)
// ---------------------------------------------------------------------------
//
// Editor-authored emission parameters that the runtime ParticleEmitterSystem
// (engine/animation) walks each frame to spawn / age / cull particles.
// Lives in engine/scene as a thin POD so scene_serializer can JSON it
// without scene needing to depend on animation (animation depends on
// scene, so the reverse would create a cycle — same constraint that
// drove the M19 extension hook).
//
// Runtime particle storage (positions, velocities, ages) is owned by the
// system, NOT this component — this struct only carries the authoring
// values and a small amount of cross-frame state (time accumulator,
// alive_count read-back) so the data stays small and serialisable.
//
// Defaults model a generic upward "spark" emitter so a fresh component
// produces visible output the moment the user adds it from the Inspector.
struct ParticleEmitterComponent {
    // Emission rate (particles per second).  Negatives clamp to 0 in
    // the runtime system.
    f32  emit_rate{30.0f};

    // Per-particle initial speed window.  System samples uniformly in
    // [min, max] when a new particle is born.
    f32  speed_min{1.0f};
    f32  speed_max{3.0f};

    // Lifetime window in seconds.
    f32  lifetime_min{1.0f};
    f32  lifetime_max{2.0f};

    // Colour gradient — start at birth, end at lifetime expiry.
    Vec4 color_start{1.0f, 1.0f, 1.0f, 1.0f};
    Vec4 color_end{1.0f, 1.0f, 1.0f, 0.0f};

    // Size in world units; lerps from start to end across lifetime.
    f32  size_start{0.10f};
    f32  size_end{0.00f};

    // Constant downward acceleration — simple gravity proxy.
    f32  gravity{-9.81f};

    // Soft cap on alive particles to keep memory bounded.
    u32  max_particles{500};

    // Runtime gates — `emitting` is the user-controlled play/pause; the
    // system flips `emitting` to true on Play-mode entry when
    // `play_on_start` is set (Unity convention).
    bool emitting{true};
    bool play_on_start{true};

    // Cross-frame state (NOT serialised).  emit_accumulator_ tracks the
    // fractional particle owed since last spawn; alive_count_ surfaces
    // the live population for the Inspector / Stats overlay.
    f32  emit_accumulator{0.0f};
    u32  alive_count{0};
};

// ---------------------------------------------------------------------------
// SpotLightComponent
// ---------------------------------------------------------------------------
struct SpotLightComponent {
    Vec3  color{1.0f, 1.0f, 1.0f};
    float intensity{1.0f};
    float range{20.0f};
    float inner_angle{12.5f};    // degrees (full-bright cone)
    float outer_angle{17.5f};    // degrees (fade-out cone)
    // Shadow toggle.  Off by default: spot-light shadows need their
    // own per-light depth FBO and most accent spots don't visually
    // require them.  Enable on key dramatic spots (flashlights, etc).
    bool  cast_shadows{false};
};

// ---------------------------------------------------------------------------
// TilemapComponent - 2D grid of tile indices for efficient tile rendering
// ---------------------------------------------------------------------------
struct TilemapComponent {
    u32 width{0};
    u32 height{0};
    float tile_size{1.0f};
    u32 texture_id{0};
    u32 tiles_per_row{16};        // tiles in texture atlas row
    u32 tiles_per_col{16};
    std::vector<i32> tiles;       // -1 = empty, otherwise tile index

    i32 get_tile(u32 x, u32 y) const {
        if (x >= width || y >= height) return -1;
        return tiles[y * width + x];
    }
    void set_tile(u32 x, u32 y, i32 tile_id) {
        if (x < width && y < height)
            tiles[y * width + x] = tile_id;
    }
    void resize(u32 w, u32 h, i32 fill = -1) {
        width = w; height = h;
        tiles.assign(static_cast<size_t>(w) * h, fill);
    }
};

} // namespace nexus
