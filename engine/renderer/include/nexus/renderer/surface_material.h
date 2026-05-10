#pragma once

#include <nexus/core/types.h>
#include <nexus/core/math.h>

namespace nexus {

// ─────────────────────────────────────────────────────────────────────────────
// SurfaceMaterial — runtime parameter set consumed by the forward shader.
// ─────────────────────────────────────────────────────────────────────────────
//
// This is the in-memory POD the renderer reads to populate `u_Material_*`
// uniforms each draw call.  It is intentionally a flat struct (no
// shared_ptrs, no heap), so a draw can copy it cheaply and a scene can
// store a small material table without indirection.
//
// Why a strong-typed struct rather than `Material`'s property map:
//   • Compile-time field validation — typos in uniform names become
//     compile errors, not silent shader-state drifts.
//   • One-to-one with shader uniforms — adding a field is a deliberate
//     act in three places (struct, serializer, shader) instead of a
//     stringly-typed property that may or may not exist.
//   • Serialization and editor inspectors enumerate concrete fields,
//     not a heterogenous map.
//
// Field semantics map directly to the Blinn-Phong + hemispheric ambient
// model in forward_renderer_3d.cpp.  PBR fields (metallic / roughness)
// are present so MaterialData can already hold them and a future
// PBR shader can pick them up without changing the struct layout.
struct SurfaceMaterial {
    // Base colour multiplied per-fragment.  Final albedo seen by the
    // shader is `albedo * MeshRendererComponent.tint`, so the component
    // tint stays useful for instance variations.
    Vec4 albedo{1.0f, 1.0f, 1.0f, 1.0f};

    // Specular reflection model (Blinn-Phong) parameters.
    Vec3  specular_color   {1.0f, 1.0f, 1.0f};
    float specular_strength{0.5f};   // 0 = no specular, 1 = full specular
    float shininess        {32.0f};  // Blinn-Phong exponent; higher = tighter highlight

    // Diffuse softening.  0 = canonical Lambert (hard terminator).
    // ~0.25 = soft area-light look used by no-IBL realtime renderers.
    // Per-material so a polished plastic sphere can sit next to a matte
    // chalk wall without one wrap value flattening both.
    float diffuse_wrap{0.0f};

    // How much hemispheric ambient this surface accepts.  0 = no GI,
    // 1 = full sky/ground bounce.  Lets emissive / dark surfaces sit
    // quietly without absorbing too much fill light.
    float ambient_response{1.0f};

    // Self-illumination.  Emissive bypasses lighting entirely.
    Vec3  emissive          {0.0f, 0.0f, 0.0f};
    float emissive_strength {0.0f};  // multiplier on `emissive` colour

    // PBR data — held now for serialization parity with assets::MaterialData.
    // The current forward shader is Blinn-Phong and ignores these; a
    // future PBR shader will read them from the same uniform block so
    // existing materials light up "for free".
    float metallic {0.0f};
    float roughness{1.0f};

    // Returns the canonical "default" material — neutral white, soft-but-
    // realistic specular, no wrap (Lambert), full ambient response.  Used
    // by ForwardRenderer3D when MeshRendererComponent.material_id == 0
    // so legacy / unset call sites keep rendering identically.
    static SurfaceMaterial defaults() { return {}; }
};

} // namespace nexus
