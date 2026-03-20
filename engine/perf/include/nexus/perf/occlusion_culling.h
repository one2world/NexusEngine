#pragma once

#include <nexus/core/types.h>
#include <nexus/core/math.h>

#include <vector>

namespace nexus {

// ---------------------------------------------------------------------------
// BoundingVolume — axis-aligned bounding box for culling
// ---------------------------------------------------------------------------
struct CullAABB {
    Vec3 min{0.0f};
    Vec3 max{0.0f};

    [[nodiscard]] Vec3 center() const { return (min + max) * 0.5f; }
    [[nodiscard]] Vec3 extents() const { return (max - min) * 0.5f; }
};

// ---------------------------------------------------------------------------
// CullObject — a renderable to test for visibility
// ---------------------------------------------------------------------------
struct CullObject {
    u32      id = 0;
    CullAABB bounds;
};

// ---------------------------------------------------------------------------
// FrustumPlane — one plane of the view frustum
// ---------------------------------------------------------------------------
struct FrustumPlane {
    Vec3 normal{0.0f, 0.0f, 1.0f};
    f32  distance = 0.0f;

    [[nodiscard]] f32 signed_distance(const Vec3& point) const {
        return glm::dot(normal, point) + distance;
    }
};

// ---------------------------------------------------------------------------
// Frustum — 6 planes extracted from a view-projection matrix
// ---------------------------------------------------------------------------
struct Frustum {
    FrustumPlane planes[6]; // left, right, bottom, top, near, far

    /// Extract planes from a view-projection matrix.
    static Frustum from_view_projection(const Mat4& vp);

    /// Test if an AABB is at least partially inside the frustum.
    [[nodiscard]] bool test_aabb(const CullAABB& aabb) const;
};

// ---------------------------------------------------------------------------
// SoftwareDepthBuffer — simplified software rasterizer for occlusion
// ---------------------------------------------------------------------------
class SoftwareDepthBuffer {
public:
    SoftwareDepthBuffer() = default;
    SoftwareDepthBuffer(u32 width, u32 height);

    /// Clear to far depth.
    void clear();

    /// Rasterize an AABB as an occluder (write depth).
    void rasterize_occluder(const CullAABB& aabb, const Mat4& view_proj);

    /// Test if an AABB is occluded (all sample points behind existing depth).
    [[nodiscard]] bool is_occluded(const CullAABB& aabb, const Mat4& view_proj) const;

    [[nodiscard]] u32 width() const { return width_; }
    [[nodiscard]] u32 height() const { return height_; }
    [[nodiscard]] const std::vector<f32>& buffer() const { return depth_; }

private:
    /// Project a world-space point to screen-space [0, w) x [0, h), returns depth.
    bool project(const Vec3& world, const Mat4& vp, f32& sx, f32& sy, f32& depth) const;

    u32 width_  = 0;
    u32 height_ = 0;
    std::vector<f32> depth_;
};

// ---------------------------------------------------------------------------
// OcclusionCuller — frustum + occlusion culling pipeline
// ---------------------------------------------------------------------------
class OcclusionCuller {
public:
    OcclusionCuller() = default;
    explicit OcclusionCuller(u32 depth_width, u32 depth_height);

    /// Set the view-projection matrix for this frame.
    void set_view_projection(const Mat4& vp);

    /// Submit an occluder (large object whose depth blocks things behind it).
    void submit_occluder(const CullAABB& aabb);

    /// Perform culling on the given objects, returning visible object IDs.
    [[nodiscard]] std::vector<u32> cull(const std::vector<CullObject>& objects) const;

    /// Just frustum culling, no occlusion.
    [[nodiscard]] std::vector<u32> frustum_cull(const std::vector<CullObject>& objects) const;

    /// Stats from the last cull pass.
    [[nodiscard]] u32 total_tested() const { return total_tested_; }
    [[nodiscard]] u32 frustum_culled() const { return frustum_culled_; }
    [[nodiscard]] u32 occlusion_culled() const { return occlusion_culled_; }

private:
    Mat4               view_proj_{1.0f};
    Frustum            frustum_;
    SoftwareDepthBuffer depth_buffer_;

    mutable u32 total_tested_     = 0;
    mutable u32 frustum_culled_   = 0;
    mutable u32 occlusion_culled_ = 0;
};

} // namespace nexus
