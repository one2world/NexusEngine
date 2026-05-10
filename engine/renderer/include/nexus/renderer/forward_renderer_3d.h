#pragma once

#include <nexus/core/types.h>
#include <nexus/core/math.h>
#include <nexus/rhi/rhi.h>
#include <nexus/renderer/camera.h>
#include <nexus/renderer/shadow_map.h>
#include <vector>
#include <memory>
#include <functional>

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

    // Local-space axis-aligned bounding box.  Computed automatically by
    // ForwardRenderer3D::upload_mesh from `vertices`.  Frustum culling in
    // draw_mesh transforms the eight corners into world space and tests the
    // resulting bounding sphere against the camera frustum.  Leaving the
    // bounds empty (min == max) disables culling for that mesh, so a mesh
    // that is built and submitted without going through upload_mesh still
    // renders correctly (just unculled).
    Vec3 local_aabb_min{0.0f};
    Vec3 local_aabb_max{0.0f};
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

    /// Hemispheric ambient — sky-tinted on top, ground-tinted underneath,
    /// blended by surface normal.y.  Replaces a flat "ambient = 0.1 *
    /// lightColor" baseline that produced near-black back-hemispheres
    /// and a hard light/dark border on smooth surfaces.  Hosts can
    /// override the defaults to match a sunset / overcast / night-time
    /// mood.  Applied once per fragment in the shader's main().
    void set_ambient_sky(Vec3 color)    { ambient_sky_    = color; }
    void set_ambient_ground(Vec3 color) { ambient_ground_ = color; }
    Vec3 ambient_sky()    const { return ambient_sky_;    }
    Vec3 ambient_ground() const { return ambient_ground_; }

    void upload_mesh(Mesh& mesh);
    void destroy_mesh(Mesh& mesh);

    void draw_mesh(const Mesh& mesh, const Mat4& transform,
                   Vec4 color = Vec4{1.0f}, rhi::TextureHandle texture = rhi::INVALID_HANDLE);

    /// Simple frustum culling check against a bounding sphere.
    [[nodiscard]] bool is_visible(Vec3 center, float radius) const;

    // ── Shadow mapping ──────────────────────────────────────────────────
    /// Enable cascaded shadow mapping for the directional light.
    void enable_shadows(const CascadedShadowMap::Config& config = {});

    /// Disable shadow mapping.
    void disable_shadows();

    /// Get shadow map (nullptr if disabled).
    CascadedShadowMap* shadow_map() { return shadow_map_.get(); }
    const CascadedShadowMap* shadow_map() const { return shadow_map_.get(); }

    /// Render the shadow depth pass. Call after begin_frame(), before draw_mesh() calls.
    /// Provide a callback that submits geometry for each cascade.
    using ShadowGeometryCallback = std::function<void(u32 cascade)>;
    void render_shadow_pass(Vec3 light_direction, ShadowGeometryCallback submit_geometry);

private:
    rhi::RHI*         rhi_{nullptr};
    rhi::ShaderHandle shader_{rhi::INVALID_HANDLE};
    rhi::TextureHandle white_texture_{rhi::INVALID_HANDLE};

    DirectionalLight dir_light_;
    std::vector<PointLight> point_lights_;
    std::vector<SpotLight>  spot_lights_;

    // Hemispheric ambient defaults — overcast-sky preset.  Magnitudes
    // chosen so the back-hemisphere reads as dim-but-illuminated rather
    // than near-black.  Sandbox / editor / future scene formats may
    // override per-frame.
    Vec3 ambient_sky_   {0.32f, 0.36f, 0.42f};
    Vec3 ambient_ground_{0.18f, 0.16f, 0.14f};

    Mat4 view_projection_{1.0f};
    Vec3 camera_position_{0.0f};
    bool in_frame_{false};

    // Frustum planes for culling (extracted from view-projection matrix)
    Vec4 frustum_planes_[6]{};
    void extract_frustum_planes();

    // Shadow mapping
    std::unique_ptr<CascadedShadowMap> shadow_map_;
    Camera3D current_camera_;
};

} // namespace nexus
