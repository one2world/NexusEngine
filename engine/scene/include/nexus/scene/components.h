#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"

#include <string>

namespace nexus {

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
};

// ---------------------------------------------------------------------------
// Transform3DComponent
// ---------------------------------------------------------------------------
struct Transform3DComponent {
    Vec3 position{0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f, 1.0f, 1.0f};

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
};

// ---------------------------------------------------------------------------
// DirectionalLightComponent
// ---------------------------------------------------------------------------
struct DirectionalLightComponent {
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
    enum Type { Static, Dynamic, Kinematic };

    Type  type           = Dynamic;
    float density        = 1.0f;
    float friction       = 0.3f;
    float restitution    = 0.0f;
    bool  fixed_rotation = false;
};

} // namespace nexus
