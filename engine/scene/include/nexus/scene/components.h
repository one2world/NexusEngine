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
// TagComponent - human-readable name for an entity
// ---------------------------------------------------------------------------
struct TagComponent {
    std::string name;
};

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
// MeshRendererComponent
// ---------------------------------------------------------------------------
struct MeshRendererComponent {
    u32 mesh_id     = 0;
    u32 material_id = 0;
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
};

// ---------------------------------------------------------------------------
// PointLightComponent
// ---------------------------------------------------------------------------
struct PointLightComponent {
    Vec3  color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float radius    = 10.0f;
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
// SpotLightComponent
// ---------------------------------------------------------------------------
struct SpotLightComponent {
    Vec3  color{1.0f, 1.0f, 1.0f};
    float intensity{1.0f};
    float range{20.0f};
    float inner_angle{12.5f};    // degrees (full-bright cone)
    float outer_angle{17.5f};    // degrees (fade-out cone)
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
