#include <nexus/renderer/shadow_system.h>
#include <nexus/renderer/shadow_map.h>
#include <nexus/renderer/forward_renderer_3d.h>
#include <nexus/scene/registry.h>
#include <nexus/scene/components.h>
#include <nexus/core/log.h>

namespace nexus {

Entity ShadowSystem::find_directional_caster(Registry& registry) const {
    Entity result = INVALID_ENTITY;
    registry.each<DirectionalLightComponent>(
        [&result](Entity e, DirectionalLightComponent& dl) {
            // First-match wins: matches Unity's "main directional light"
            // convention.  Multiple directional lights still contribute
            // diffuse light, but only the first shadow-caster drives the
            // CSM depth pass.
            if (result != INVALID_ENTITY) return;
            if (dl.cast_shadows) result = e;
        });
    return result;
}

// Helper: restore the main-pass render target after the depth FBOs
// clobbered it.  Pulls the RHI from the renderer; if `target.fbo`
// is INVALID_HANDLE we go back to the default framebuffer (host
// owns clearing it).  Always re-issues set_viewport because
// bind_framebuffer auto-sets viewport to FBO dimensions — those
// may differ from the host's intended viewport (e.g. editor panel
// rendering into a 1280×720 FBO at a 1280×720 viewport happens to
// match, but the contract holds either way).
static void restore_main_pass(rhi::RHI* rhi,
                               const ShadowSystem::MainPassTarget& t) {
    if (rhi == nullptr) return;
    if (t.fbo == rhi::INVALID_HANDLE) {
        rhi->unbind_framebuffer();
    } else {
        rhi->bind_framebuffer(t.fbo);
    }
    if (t.viewport_w > 0 && t.viewport_h > 0) {
        rhi->set_viewport(0, 0, t.viewport_w, t.viewport_h);
    }
}

void ShadowSystem::render(ForwardRenderer3D& renderer, Registry& registry,
                          const MainPassTarget& target) {
    // 1. Pick the directional light that drives the CSM.  Without one
    //    we skip the entire shadow pass — the renderer's
    //    push_shadow_uniforms() left u_NumCascades=0 from begin_frame,
    //    so the main shader treats every fragment as fully lit.
    Entity sun_entity = find_directional_caster(registry);
    if (sun_entity == INVALID_ENTITY) return;

    auto* csm = renderer.shadow_map();
    if (csm == nullptr) return;  // host hasn't called enable_shadows yet

    // 2. Update CSM split bounds + light view-projection matrices.
    //    The renderer's begin_frame already cached the camera, so we
    //    read it back instead of forcing every host to plumb the
    //    camera through ShadowSystem.
    auto& dl = registry.get_component<DirectionalLightComponent>(sun_entity);
    csm->update(renderer.current_camera(), dl.direction);

    // 3. Per-cascade depth pass — for each cascade, render every
    //    shadow-casting MeshRenderer's geometry into the depth FBO.
    //    The depth-only pipeline lives inside CascadedShadowMap.
    //    Each begin_pass binds a shadow FBO and sets the viewport to
    //    the CSM resolution — restored below.
    for (u32 c = 0; c < csm->num_cascades(); ++c) {
        csm->begin_pass(c);
        registry.each<MeshRendererComponent, Transform3DComponent>(
            [this, csm](Entity, MeshRendererComponent& mr,
                        Transform3DComponent& tc) {
                if (!mr.cast_shadows) return;
                if (mr.mesh_id == 0)  return;
                if (!resolver_)       return;
                const Mesh* mesh = resolver_(mr.mesh_id);
                if (mesh == nullptr)              return;
                if (mesh->vbo == rhi::INVALID_HANDLE) return;
                csm->submit_geometry(mesh->vbo, mesh->ibo,
                                      static_cast<u32>(mesh->indices.size()),
                                      tc.world_matrix);
            });
        csm->end_pass();
    }

    // 4. Restore the host's render target *before* binding shadow
    //    samplers and pushing uniforms — push_shadow_uniforms binds
    //    texture units which need the main-pass shader / FBO active
    //    to take effect cleanly.  Without this restore the colour
    //    pass would inherit the shadow FBO's viewport (e.g. 2048²)
    //    and visibly clip / pop entities in and out of frame as the
    //    camera moves.
    restore_main_pass(renderer.rhi(), target);

    // 5. Push the freshly-computed cascade matrices, split depths,
    //    bias / texel size, and bind the depth textures to the main
    //    shader's sampler units.  Subsequent draw_mesh calls in the
    //    colour pass sample from these.
    renderer.bind_shadow_data();
}

} // namespace nexus
