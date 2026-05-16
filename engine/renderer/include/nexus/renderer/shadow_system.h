#pragma once

#include <nexus/core/types.h>
#include <nexus/core/math.h>
#include <nexus/scene/registry.h>
#include <nexus/scene/components.h>
#include <nexus/renderer/forward_renderer_3d.h>

#include <functional>

namespace nexus {

struct Mesh;

// ─────────────────────────────────────────────────────────────────────────────
// ShadowSystem — orchestrates the shadow depth passes per frame.
// ─────────────────────────────────────────────────────────────────────────────
//
// The shadow infrastructure (CascadedShadowMap depth FBOs, the depth
// shader, PCF sampling helpers) lives in the renderer module.  What
// was missing is the *orchestration*: collecting the active shadow-
// casting lights from a scene, gathering caster meshes, driving the
// per-light depth passes, and pushing the resulting cascade matrices
// to the main forward shader.  ShadowSystem is that glue.
//
// Usage (per frame, after begin_frame):
//
//     ShadowSystem shadows;
//     shadows.set_mesh_resolver([&](u32 id) {
//         return mesh_table.find(id) != ... ? mesh_table[id] : nullptr;
//     });
//     shadows.render(forward_renderer, registry);
//
// Behaviour:
//   - Iterates DirectionalLightComponent entries.  The first one with
//     cast_shadows == true drives the CSM.  Multiple directional
//     lights are valid in the scene (e.g. moon + sun) but only one
//     casts at a time — additional ones still light fragments via
//     diffuse but produce no shadow.  This matches Unity's "main
//     directional light" convention.
//   - For every entity carrying a MeshRenderer + Transform3D with
//     cast_shadows == true, submits the mesh's vbo / ibo / model
//     matrix to the cascade depth pass.
//   - After the depth pass, calls forward_renderer.bind_shadow_data()
//     so the main shader's u_ShadowMap[N], u_LightVP[N], etc. are
//     populated for the upcoming colour pass.
//
// Scope (this iteration):
//   ✓ Directional CSM end-to-end
//   ✗ Point-light cubemap shadows  (deferred to next sprint)
//   ✗ Spot-light shadow maps       (deferred to next sprint)
class ShadowSystem {
public:
    /// Resolves a MeshRendererComponent.mesh_id to a Mesh pointer.
    /// Hosts know the mesh registry; ShadowSystem doesn't.
    using MeshResolver = std::function<const Mesh*(u32 mesh_id)>;
    void set_mesh_resolver(MeshResolver r) { resolver_ = std::move(r); }

    /// Render every active shadow caster's depth pass and push the
    /// resulting cascade data to the renderer for the upcoming colour
    /// pass.  Idempotent if no shadow-casting light is present —
    /// the renderer's u_NumCascades stays at 0 and the main shader
    /// short-circuits to "fully lit".
    ///
    /// Must be called between forward_renderer.begin_frame() and the
    /// first colour-pass draw_mesh, while the camera matrices in the
    /// renderer are valid (CSM split bounds depend on them).
    void render(ForwardRenderer3D& renderer, Registry& registry);

private:
    /// Walk the registry once to find the active shadow-casting
    /// directional light.  Returns INVALID_ENTITY when none exists.
    Entity find_directional_caster(Registry& registry) const;

    MeshResolver resolver_;
};

} // namespace nexus
