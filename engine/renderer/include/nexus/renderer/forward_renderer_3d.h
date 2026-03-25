#pragma once

#include <nexus/core/types.h>
#include <nexus/core/math.h>
#include <nexus/rhi/rhi.h>
#include <nexus/renderer/camera.h>
#include <vector>

namespace nexus {

// ─────────────────────────────────────────────────────────────────────────────
// Mesh – CPU-side mesh data for uploading to GPU
// ─────────────────────────────────────────────────────────────────────────────

struct MeshVertex {
    Vec3 position;
    Vec3 normal;
    Vec2 texcoord;
};

struct Mesh {
    std::vector<MeshVertex> vertices;
    std::vector<u32>        indices;

    rhi::BufferHandle   vbo{rhi::INVALID_HANDLE};
    rhi::BufferHandle   ibo{rhi::INVALID_HANDLE};
    rhi::PipelineHandle pipeline{rhi::INVALID_HANDLE};
};

// ─────────────────────────────────────────────────────────────────────────────
// Primitive mesh generators
// ─────────────────────────────────────────────────────────────────────────────

Mesh create_cube_mesh();
Mesh create_plane_mesh(float size = 10.0f, u32 subdivisions = 1);
Mesh create_sphere_mesh(float radius = 1.0f, u32 rings = 16, u32 sectors = 32);

// ─────────────────────────────────────────────────────────────────────────────
// DirectionalLight – simple directional light data
// ─────────────────────────────────────────────────────────────────────────────

struct DirectionalLight {
    Vec3  direction{-0.2f, -1.0f, -0.3f};
    Vec3  color{1.0f, 1.0f, 1.0f};
    float intensity{1.0f};
};

struct PointLight {
    Vec3  position{0.0f};
    Vec3  color{1.0f};
    float intensity{1.0f};
    float radius{10.0f};
};

struct SpotLight {
    Vec3  position{0.0f};
    Vec3  direction{0.0f, -1.0f, 0.0f};
    Vec3  color{1.0f};
    float intensity{1.0f};
    float range{20.0f};
    float inner_cos{0.0f};  // cos(inner_angle)
    float outer_cos{0.0f};  // cos(outer_angle)
};

// ─────────────────────────────────────────────────────────────────────────────
// ForwardRenderer3D – multi-light forward rendering
// ─────────────────────────────────────────────────────────────────────────────

class ForwardRenderer3D {
public:
    static constexpr u32 MAX_POINT_LIGHTS = 8;
    static constexpr u32 MAX_SPOT_LIGHTS  = 4;

    ForwardRenderer3D() = default;
    ~ForwardRenderer3D() { shutdown(); }

    // Non-copyable, movable
    ForwardRenderer3D(const ForwardRenderer3D&) = delete;
    ForwardRenderer3D& operator=(const ForwardRenderer3D&) = delete;
    ForwardRenderer3D(ForwardRenderer3D&& other) noexcept;
    ForwardRenderer3D& operator=(ForwardRenderer3D&& other) noexcept;

    void init(rhi::RHI* rhi);
    void shutdown();

    void begin_frame(const Camera3D& camera);
    void end_frame();

    // Aliases for backward compatibility
    void begin(const Camera3D& camera) { begin_frame(camera); }
    void end() { end_frame(); }

    void set_directional_light(const DirectionalLight& light);
    void add_point_light(const PointLight& light);
    void add_spot_light(const SpotLight& light);

    void upload_mesh(Mesh& mesh);
    void destroy_mesh(Mesh& mesh);

    void draw_mesh(const Mesh& mesh, const Mat4& transform,
                   Vec4 color = Vec4{1.0f}, rhi::TextureHandle texture = rhi::INVALID_HANDLE);

    /// Simple frustum culling check against a bounding sphere.
    [[nodiscard]] bool is_visible(Vec3 center, float radius) const;

private:
    rhi::RHI*         rhi_{nullptr};
    rhi::ShaderHandle shader_{rhi::INVALID_HANDLE};
    rhi::TextureHandle white_texture_{rhi::INVALID_HANDLE};

    DirectionalLight dir_light_;
    std::vector<PointLight> point_lights_;
    std::vector<SpotLight>  spot_lights_;

    Mat4 view_projection_{1.0f};
    Vec3 camera_position_{0.0f};
    bool in_frame_{false};

    // Frustum planes for culling (extracted from view-projection matrix)
    Vec4 frustum_planes_[6]{};
    void extract_frustum_planes();
};

} // namespace nexus
