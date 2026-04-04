#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include "nexus/rhi/rhi.h"
#include <vector>

namespace nexus {

// ============================================================================
// Deferred Decal System - projects textures onto G-buffer geometry
// ============================================================================

struct Decal {
    Mat4 transform{1.0f};           // world transform of decal box
    Mat4 inv_transform{1.0f};       // inverse for projection
    rhi::TextureHandle albedo_tex{rhi::INVALID_HANDLE};
    rhi::TextureHandle normal_tex{rhi::INVALID_HANDLE};
    Vec4 color{1.0f};
    float normal_strength{1.0f};
    float fade_distance{0.5f};      // angle fade at edges
    u32 layer{0};                   // sorting layer
    bool active{true};
};

class DecalSystem {
public:
    void init(rhi::RHI* rhi);
    void shutdown();

    /// Add a decal and return its index.
    u32 add_decal(const Decal& decal);

    /// Remove a decal by index.
    void remove_decal(u32 index);

    /// Render all active decals into the G-buffer.
    /// Requires: depth texture (for projection), G-buffer normal texture.
    void render(rhi::TextureHandle depth_tex, rhi::TextureHandle normal_tex,
                const Mat4& view, const Mat4& projection,
                const Mat4& inv_view_projection);

    /// Access decals for modification.
    Decal& decal(u32 index) { return decals_[index]; }
    const Decal& decal(u32 index) const { return decals_[index]; }
    u32 decal_count() const { return static_cast<u32>(decals_.size()); }

    /// Clear all decals.
    void clear();

private:
    rhi::RHI* rhi_{nullptr};
    std::vector<Decal> decals_;

    // Unit cube for decal volume
    rhi::BufferHandle cube_vbo_{rhi::INVALID_HANDLE};
    rhi::BufferHandle cube_ibo_{rhi::INVALID_HANDLE};
    u32 cube_index_count_{0};

    // Decal projection shader
    rhi::ShaderHandle shader_{rhi::INVALID_HANDLE};
    rhi::PipelineHandle pipeline_{rhi::INVALID_HANDLE};
};

} // namespace nexus
