#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include "nexus/rhi/rhi.h"
#include "nexus/animation/skeleton.h"
#include <vector>

namespace nexus {

// ─────────────────────────────────────────────────────────────────────────────
// SkinnedVertex — vertex with bone weights for GPU skinning
// ─────────────────────────────────────────────────────────────────────────────

struct SkinnedVertex {
    float position[3]{};
    float normal[3]{};
    float texcoord[2]{};
    float tangent[4]{};
    u32   bone_indices[4]{0, 0, 0, 0};  // up to 4 bones per vertex
    float bone_weights[4]{0.0f};         // must sum to 1.0
};

// ─────────────────────────────────────────────────────────────────────────────
// SkinnedMeshRenderer — renders meshes deformed by skeleton animation
// ─────────────────────────────────────────────────────────────────────────────

class SkinnedMeshRenderer {
public:
    void init(rhi::RHI* rhi);
    void shutdown();

    /// Upload a skinned mesh to the GPU. Returns a mesh handle.
    u32 upload_mesh(const std::vector<SkinnedVertex>& vertices,
                    const std::vector<u32>& indices);

    /// Begin a skinned rendering pass.
    void begin(const Mat4& view_projection);

    /// Submit a skinned mesh for rendering.
    /// bone_matrices: array of final skin matrices from Skeleton::compute_skin_matrices()
    void draw(u32 mesh_handle, const Mat4& model,
              const Mat4* bone_matrices, u32 bone_count,
              rhi::TextureHandle diffuse_texture = rhi::INVALID_HANDLE,
              Vec4 color = Vec4{1.0f});

    /// Flush all submitted draws.
    void end();

    static constexpr u32 MAX_BONES = 128;

private:
    rhi::RHI* rhi_{nullptr};
    rhi::ShaderHandle shader_{rhi::INVALID_HANDLE};
    rhi::PipelineHandle pipeline_{rhi::INVALID_HANDLE};

    struct MeshGPU {
        rhi::BufferHandle vbo{rhi::INVALID_HANDLE};
        rhi::BufferHandle ibo{rhi::INVALID_HANDLE};
        u32 index_count{0};
        u32 vertex_count{0};
    };
    std::vector<MeshGPU> meshes_;

    Mat4 view_projection_{1.0f};
};

} // namespace nexus
