#pragma once

#include <nexus/core/types.h>
#include <nexus/core/math.h>
#include <nexus/rhi/rhi.h>
#include <nexus/renderer/surface_material.h>
#include <nexus/renderer/camera.h>
#include <nexus/renderer/shadow_map.h>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

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
    /// blended by surface normal.y.  Scene-level fill that every material
    /// receives in proportion to its `ambient_response` field.  Hosts
    /// override the defaults to match a sunset / overcast / night mood.
    void set_ambient_sky(Vec3 color)    { ambient_sky_    = color; }
    void set_ambient_ground(Vec3 color) { ambient_ground_ = color; }
    Vec3 ambient_sky()    const { return ambient_sky_;    }
    Vec3 ambient_ground() const { return ambient_ground_; }

    // ── Material table (M-mat) ─────────────────────────────────────────
    //
    // The renderer owns a u32 → SurfaceMaterial map.  MeshRendererComponent
    // references entries by `material_id`; on draw_mesh() the renderer
    // looks up the material and pushes its fields into u_Material_*
    // uniforms.  material_id == 0 falls back to a built-in default
    // (neutral white, mild specular, no wrap) so legacy call sites and
    // freshly-created entities keep rendering.
    //
    // Materials are stored by value (POD struct) — registering a new
    // material with the same id replaces the previous one, which makes
    // hot-tweaking from the editor / scripts trivial: just call
    // upload_material() again with the new struct.
    void upload_material(u32 id, const SurfaceMaterial& material);

    /// Lookup; returns the bound material or the default if id is unknown.
    const SurfaceMaterial& get_material(u32 id) const;

    /// Replace the default material applied when material_id == 0.
    /// Lets a host set a project-wide neutral surface look without
    /// forcing every entity to carry an explicit material_id.
    void set_default_material(const SurfaceMaterial& m) { default_material_ = m; }
    const SurfaceMaterial& default_material() const { return default_material_; }

    /// Number of entries currently in the material table (excludes
    /// the implicit default at id 0).  Diagnostic only.
    u32 material_count() const;

    void upload_mesh(Mesh& mesh);
    void destroy_mesh(Mesh& mesh);

    /// Material-aware draw entry point.  `material_id` selects an entry
    /// uploaded via upload_material; 0 falls back to default_material_.
    /// `tint` is multiplied per-fragment over the material's albedo
    /// (legacy MeshRendererComponent.tint behaviour, kept for cheap
    /// instance variation).  Material's albedo provides the base
    /// surface colour, lighting parameters (specular weight, shininess,
    /// wrap, ambient response, emissive) come from the material.
    void draw_mesh(const Mesh& mesh, const Mat4& transform,
                   u32 material_id, Vec4 tint = Vec4{1.0f},
                   rhi::TextureHandle texture = rhi::INVALID_HANDLE);

    /// Legacy overload — material_id defaults to 0 (default material).
    /// Kept so existing call sites compile while we migrate them to
    /// pass MeshRendererComponent.material_id.
    void draw_mesh(const Mesh& mesh, const Mat4& transform,
                   Vec4 tint = Vec4{1.0f},
                   rhi::TextureHandle texture = rhi::INVALID_HANDLE) {
        draw_mesh(mesh, transform, 0u, tint, texture);
    }

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

    // Material table.  Indexed by MeshRendererComponent.material_id.
    // material_id 0 falls back to default_material_; entries here
    // override the default for any explicit id.
    std::unordered_map<u32, SurfaceMaterial> material_table_;
    SurfaceMaterial                          default_material_{};

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
