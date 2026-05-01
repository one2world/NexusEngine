#pragma once

#include "nexus/core/types.h"
#include "nexus/rhi/rhi.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace nexus {
struct Mesh;
class ForwardRenderer3D;
namespace assets { class AssetRegistry; }
}

namespace nexus::editor {

class ViewportPanel;

// ─────────────────────────────────────────────────────────────────────────────
// AssetDropImporter — convert dropped files into live engine resources
// ─────────────────────────────────────────────────────────────────────────────
//
// Drag-and-drop in the editor needs more than just an `AssetId` — to display
// the asset the engine needs:
//   • Textures: decoded RGBA8 + uploaded RHI texture handle, surfaced to the
//     SpriteRenderer / MeshRenderer so the next frame samples the bytes.
//   • Meshes:   decoded vertex / index buffers + an in-engine `nexus::Mesh`
//     uploaded via ForwardRenderer3D, then registered with each ViewportPanel
//     so MeshRendererComponent.mesh_id resolves to real geometry.
//
// This importer owns the decoded resources for the editor session.  It uses
// the engine's AssetLoader for format-specific decoding (.obj/.gltf/.glb via
// MeshImporter; .png/.jpg/... via TextureImporter + stb_image), so format
// support stays centralized.  The importer caches results by absolute path
// — re-dropping the same file is a free lookup, no re-decode.
//
// Lifetime:
//   The importer holds shared_ptr<MeshData>/TextureData from the loader plus
//   the live `Mesh` storage.  Owns one viewport-registered Mesh per imported
//   model file.  All resources are released in destructor (RHI texture
//   destroy + clear caches).
class AssetDropImporter {
public:
    AssetDropImporter() = default;
    ~AssetDropImporter();

    AssetDropImporter(const AssetDropImporter&)            = delete;
    AssetDropImporter& operator=(const AssetDropImporter&) = delete;

    // Late binding so the editor can construct the importer before RHI /
    // renderer are alive, and wire them in once GL / GLFW are up.
    void set_rhi(rhi::RHI* r)                       { rhi_ = r; }
    void set_renderer_3d(ForwardRenderer3D* r)      { renderer_3d_ = r; }
    void set_asset_registry(assets::AssetRegistry* reg) { registry_ = reg; }

    // Add a viewport that should receive register_mesh() calls when meshes
    // are imported.  Multiple viewports (Scene, Game) can subscribe — they
    // all see the same mesh_id pointing at the same engine Mesh.
    void add_viewport(ViewportPanel* vp);

    // Returns the engine resource id (texture handle for images, mesh_id
    // for models) suitable for plugging into a Renderer component.  Returns
    // 0 / INVALID_HANDLE on decode failure.  Idempotent — second call with
    // the same path returns the cached id.
    rhi::TextureHandle import_texture(const std::string& path);
    u32                import_mesh(const std::string& path);

    // Test introspection.
    u32 texture_count() const { return static_cast<u32>(texture_cache_.size()); }
    u32 mesh_count()    const { return static_cast<u32>(mesh_cache_.size()); }

private:
    struct CachedMesh {
        std::unique_ptr<Mesh> mesh;
        u32 mesh_id{0};
    };

    rhi::RHI*               rhi_{nullptr};
    ForwardRenderer3D*      renderer_3d_{nullptr};
    assets::AssetRegistry*  registry_{nullptr};
    std::vector<ViewportPanel*> viewports_;

    std::unordered_map<std::string, rhi::TextureHandle> texture_cache_;
    std::unordered_map<std::string, CachedMesh>          mesh_cache_;

    // Mesh ids for imported assets start above the primitive range to avoid
    // colliding with built-in cube/plane/sphere ids (1..3).
    u32 next_mesh_id_{1024};
};

}  // namespace nexus::editor
