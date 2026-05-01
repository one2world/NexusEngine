#include "nexus/editor/asset_drop_importer.h"
#include "nexus/editor/editor_panels.h"

#include "nexus/assets/asset_handle.h"
#include "nexus/assets/asset_loader.h"
#include "nexus/assets/asset_registry.h"
#include "nexus/core/log.h"
#include "nexus/renderer/forward_renderer_3d.h"

#include <algorithm>
#include <filesystem>
#include <utility>

namespace nexus::editor {

namespace {

// Importers are stateless and cheap to construct; we keep one per category.
nexus::assets::TextureImporter& texture_importer() {
    static nexus::assets::TextureImporter inst;
    return inst;
}
nexus::assets::MeshImporter& mesh_importer() {
    static nexus::assets::MeshImporter inst;
    return inst;
}

// Pulls (or creates) a stable AssetId for a path so dropped assets survive
// scene save/load round-trips.  The registry lookup is purely organizational
// — the importer's own caches gate decode + GPU upload.
nexus::assets::AssetId ensure_registered(nexus::assets::AssetRegistry* reg,
                                         const std::string& path,
                                         nexus::assets::AssetType type) {
    if (!reg) return nexus::assets::AssetId{0};
    if (auto* existing = reg->find_by_path(path)) {
        return existing->id;
    }
    return reg->register_asset(path, path, type);
}

}  // namespace

AssetDropImporter::~AssetDropImporter() {
    // Release every uploaded GPU texture; engine meshes get freed when the
    // CachedMesh's unique_ptr<Mesh> destructor runs.  ForwardRenderer3D's
    // upload_mesh allocates VBO/IBO/pipeline handles tied to the Mesh — we
    // call destroy_mesh first so the renderer reclaims them on shutdown.
    if (rhi_) {
        for (auto& [path, h] : texture_cache_) {
            if (h != rhi::INVALID_HANDLE) rhi_->destroy_texture(h);
        }
    }
    if (renderer_3d_) {
        for (auto& [path, cm] : mesh_cache_) {
            if (cm.mesh) renderer_3d_->destroy_mesh(*cm.mesh);
        }
    }
}

void AssetDropImporter::add_viewport(ViewportPanel* vp) {
    if (!vp) return;
    if (std::find(viewports_.begin(), viewports_.end(), vp) == viewports_.end()) {
        viewports_.push_back(vp);
    }
}

// ── Texture import ──────────────────────────────────────────────────────────

rhi::TextureHandle AssetDropImporter::import_texture(const std::string& path) {
    if (path.empty()) return rhi::INVALID_HANDLE;

    if (auto it = texture_cache_.find(path); it != texture_cache_.end()) {
        return it->second;
    }
    if (!rhi_) {
        NX_WARN("AssetDropImporter: no RHI bound, can't upload texture '{}'", path);
        return rhi::INVALID_HANDLE;
    }

    nexus::assets::AssetMeta meta;
    meta.path = path;
    auto data = texture_importer().import(path, meta);
    if (!data) {
        NX_ERROR("AssetDropImporter: texture decode failed for '{}'", path);
        texture_cache_[path] = rhi::INVALID_HANDLE;
        return rhi::INVALID_HANDLE;
    }
    auto* tex = static_cast<nexus::assets::TextureData*>(data.get());
    if (tex->pixels.empty() || tex->width == 0 || tex->height == 0) {
        NX_ERROR("AssetDropImporter: texture '{}' decoded with empty bytes", path);
        texture_cache_[path] = rhi::INVALID_HANDLE;
        return rhi::INVALID_HANDLE;
    }

    // The decoder may return RGB8 or RGBA8.  Normalize to RGBA8 so the
    // shader binding path is uniform — pad with 255 alpha for opaque RGB.
    std::vector<u8> rgba;
    const u8* upload_ptr = tex->pixels.data();
    if (tex->channels == 3) {
        const u32 px = tex->width * tex->height;
        rgba.resize(static_cast<size_t>(px) * 4u);
        for (u32 i = 0; i < px; ++i) {
            rgba[i * 4u + 0] = tex->pixels[i * 3u + 0];
            rgba[i * 4u + 1] = tex->pixels[i * 3u + 1];
            rgba[i * 4u + 2] = tex->pixels[i * 3u + 2];
            rgba[i * 4u + 3] = 255u;
        }
        upload_ptr = rgba.data();
    } else if (tex->channels != 4) {
        NX_ERROR("AssetDropImporter: texture '{}' has unsupported channel count {}",
                 path, tex->channels);
        texture_cache_[path] = rhi::INVALID_HANDLE;
        return rhi::INVALID_HANDLE;
    }

    rhi::TextureDesc desc{};
    desc.width            = tex->width;
    desc.height           = tex->height;
    desc.format           = rhi::TextureFormat::RGBA8;
    desc.min_filter       = rhi::TextureFilter::LinearMipmapLinear;
    desc.mag_filter       = rhi::TextureFilter::Linear;
    desc.wrap_s           = rhi::TextureWrap::Repeat;
    desc.wrap_t           = rhi::TextureWrap::Repeat;
    desc.generate_mipmaps = true;
    desc.data             = upload_ptr;
    rhi::TextureHandle h = rhi_->create_texture(desc);
    if (h == rhi::INVALID_HANDLE) {
        NX_ERROR("AssetDropImporter: RHI rejected texture '{}'", path);
        texture_cache_[path] = rhi::INVALID_HANDLE;
        return rhi::INVALID_HANDLE;
    }

    ensure_registered(registry_, path, nexus::assets::AssetType::Texture);
    texture_cache_[path] = h;
    NX_INFO("AssetDropImporter: imported texture '{}' ({}x{}, handle={})",
            path, tex->width, tex->height, h);
    return h;
}

// ── Mesh import ─────────────────────────────────────────────────────────────

u32 AssetDropImporter::import_mesh(const std::string& path) {
    if (path.empty()) return 0u;

    if (auto it = mesh_cache_.find(path); it != mesh_cache_.end()) {
        return it->second.mesh_id;
    }

    nexus::assets::AssetMeta meta;
    meta.path = path;
    auto data = mesh_importer().import(path, meta);
    if (!data) {
        NX_ERROR("AssetDropImporter: mesh decode failed for '{}'", path);
        return 0u;
    }
    auto* md = static_cast<nexus::assets::MeshData*>(data.get());
    if (md->vertices.empty()) {
        NX_ERROR("AssetDropImporter: mesh '{}' has no vertices", path);
        return 0u;
    }

    // Convert MeshData (asset-side struct) → Mesh (renderer-side struct).
    // Index buffer is preserved verbatim; if the asset has no indices, we
    // synthesize a sequential 0..N-1 list so the renderer's indexed-draw
    // path still applies (matches its existing convention).
    auto mesh = std::make_unique<Mesh>();
    mesh->vertices.reserve(md->vertices.size());
    for (const auto& v : md->vertices) {
        MeshVertex mv;
        mv.position = Vec3(v.position[0], v.position[1], v.position[2]);
        mv.normal   = Vec3(v.normal[0],   v.normal[1],   v.normal[2]);
        mv.texcoord = Vec2(v.texcoord[0], v.texcoord[1]);
        mesh->vertices.push_back(mv);
    }
    if (md->indices.empty()) {
        mesh->indices.resize(mesh->vertices.size());
        for (u32 i = 0; i < mesh->indices.size(); ++i) {
            mesh->indices[i] = i;
        }
    } else {
        mesh->indices = md->indices;
    }
    // Upload to GPU when a renderer is bound; tests run without one and rely
    // on the CPU-side mesh + viewport registration paths.  upload_mesh
    // populates VBO/IBO/pipeline handles, which are needed at draw time but
    // not for cache integrity.
    if (renderer_3d_) {
        renderer_3d_->upload_mesh(*mesh);
    }

    const u32 mesh_id = next_mesh_id_++;
    for (auto* vp : viewports_) {
        if (vp) vp->register_mesh(mesh_id, mesh.get());
    }
    ensure_registered(registry_, path, nexus::assets::AssetType::Mesh);

    CachedMesh cm;
    cm.mesh    = std::move(mesh);
    cm.mesh_id = mesh_id;
    mesh_cache_[path] = std::move(cm);

    NX_INFO("AssetDropImporter: imported mesh '{}' (verts={}, tris={}, mesh_id={})",
            path, static_cast<u32>(md->vertices.size()),
            static_cast<u32>(md->indices.size() / 3u), mesh_id);
    return mesh_id;
}

}  // namespace nexus::editor
