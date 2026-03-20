#include "nexus/perf/occlusion_culling.h"

#include <algorithm>
#include <cmath>

namespace nexus {

// ---------------------------------------------------------------------------
// Frustum
// ---------------------------------------------------------------------------

Frustum Frustum::from_view_projection(const Mat4& vp) {
    Frustum f;

    // Extract planes from the columns of the view-projection matrix.
    // Each plane: normal.x, normal.y, normal.z, distance
    auto row = [&](int r) -> Vec4 {
        return Vec4(vp[0][r], vp[1][r], vp[2][r], vp[3][r]);
    };

    Vec4 r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);

    auto make_plane = [](Vec4 v) -> FrustumPlane {
        f32 len = glm::length(Vec3(v));
        if (len > 0.0f) v /= len;
        return {Vec3(v), v.w};
    };

    f.planes[0] = make_plane(r3 + r0); // left
    f.planes[1] = make_plane(r3 - r0); // right
    f.planes[2] = make_plane(r3 + r1); // bottom
    f.planes[3] = make_plane(r3 - r1); // top
    f.planes[4] = make_plane(r3 + r2); // near
    f.planes[5] = make_plane(r3 - r2); // far

    return f;
}

bool Frustum::test_aabb(const CullAABB& aabb) const {
    for (int i = 0; i < 6; ++i) {
        const auto& p = planes[i];

        // Find the positive vertex (most in the direction of the normal).
        Vec3 pv;
        pv.x = (p.normal.x >= 0.0f) ? aabb.max.x : aabb.min.x;
        pv.y = (p.normal.y >= 0.0f) ? aabb.max.y : aabb.min.y;
        pv.z = (p.normal.z >= 0.0f) ? aabb.max.z : aabb.min.z;

        if (p.signed_distance(pv) < 0.0f) {
            return false; // entirely outside this plane
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// SoftwareDepthBuffer
// ---------------------------------------------------------------------------

SoftwareDepthBuffer::SoftwareDepthBuffer(u32 width, u32 height)
    : width_(width), height_(height), depth_(static_cast<std::size_t>(width) * height, 1.0f) {}

void SoftwareDepthBuffer::clear() {
    std::fill(depth_.begin(), depth_.end(), 1.0f);
}

bool SoftwareDepthBuffer::project(const Vec3& world, const Mat4& vp,
                                   f32& sx, f32& sy, f32& depth) const {
    Vec4 clip = vp * Vec4(world, 1.0f);
    if (clip.w <= 0.0f) return false;

    Vec3 ndc = Vec3(clip) / clip.w;
    // NDC is [-1, 1], map to [0, w) and [0, h).
    sx    = (ndc.x * 0.5f + 0.5f) * static_cast<f32>(width_);
    sy    = (ndc.y * 0.5f + 0.5f) * static_cast<f32>(height_);
    depth = ndc.z * 0.5f + 0.5f; // map to [0,1]
    return true;
}

void SoftwareDepthBuffer::rasterize_occluder(const CullAABB& aabb, const Mat4& view_proj) {
    if (width_ == 0 || height_ == 0) return;

    // Generate the 8 corners of the AABB.
    Vec3 corners[8] = {
        {aabb.min.x, aabb.min.y, aabb.min.z},
        {aabb.max.x, aabb.min.y, aabb.min.z},
        {aabb.min.x, aabb.max.y, aabb.min.z},
        {aabb.max.x, aabb.max.y, aabb.min.z},
        {aabb.min.x, aabb.min.y, aabb.max.z},
        {aabb.max.x, aabb.min.y, aabb.max.z},
        {aabb.min.x, aabb.max.y, aabb.max.z},
        {aabb.max.x, aabb.max.y, aabb.max.z},
    };

    // Project corners and find screen-space bounding box.
    f32 min_sx = static_cast<f32>(width_), min_sy = static_cast<f32>(height_);
    f32 max_sx = 0.0f, max_sy = 0.0f;
    f32 min_depth = 1.0f;

    int valid_count = 0;
    for (auto& c : corners) {
        f32 sx, sy, d;
        if (project(c, view_proj, sx, sy, d)) {
            min_sx = std::min(min_sx, sx);
            min_sy = std::min(min_sy, sy);
            max_sx = std::max(max_sx, sx);
            max_sy = std::max(max_sy, sy);
            min_depth = std::min(min_depth, d);
            ++valid_count;
        }
    }

    if (valid_count < 3) return;

    // Clamp to buffer bounds and fill with the closest depth.
    auto ix0 = static_cast<u32>(std::max(0.0f, std::floor(min_sx)));
    auto iy0 = static_cast<u32>(std::max(0.0f, std::floor(min_sy)));
    auto ix1 = std::min(width_ - 1, static_cast<u32>(std::ceil(max_sx)));
    auto iy1 = std::min(height_ - 1, static_cast<u32>(std::ceil(max_sy)));

    for (u32 y = iy0; y <= iy1; ++y) {
        for (u32 x = ix0; x <= ix1; ++x) {
            auto idx = static_cast<std::size_t>(y) * width_ + x;
            depth_[idx] = std::min(depth_[idx], min_depth);
        }
    }
}

bool SoftwareDepthBuffer::is_occluded(const CullAABB& aabb, const Mat4& view_proj) const {
    if (width_ == 0 || height_ == 0) return false;

    // Test the 8 corners of the AABB. If all projected points are behind existing depth, it's occluded.
    Vec3 corners[8] = {
        {aabb.min.x, aabb.min.y, aabb.min.z},
        {aabb.max.x, aabb.min.y, aabb.min.z},
        {aabb.min.x, aabb.max.y, aabb.min.z},
        {aabb.max.x, aabb.max.y, aabb.min.z},
        {aabb.min.x, aabb.min.y, aabb.max.z},
        {aabb.max.x, aabb.min.y, aabb.max.z},
        {aabb.min.x, aabb.max.y, aabb.max.z},
        {aabb.max.x, aabb.max.y, aabb.max.z},
    };

    for (auto& c : corners) {
        f32 sx, sy, d;
        if (!project(c, view_proj, sx, sy, d)) continue;

        auto ix = static_cast<i32>(sx);
        auto iy = static_cast<i32>(sy);
        if (ix < 0 || ix >= static_cast<i32>(width_) ||
            iy < 0 || iy >= static_cast<i32>(height_)) {
            return false; // off-screen corner = potentially visible
        }

        auto idx = static_cast<std::size_t>(iy) * width_ + static_cast<std::size_t>(ix);
        if (d <= depth_[idx]) {
            return false; // this corner is in front of the depth buffer
        }
    }

    return true; // all corners are behind
}

// ---------------------------------------------------------------------------
// OcclusionCuller
// ---------------------------------------------------------------------------

OcclusionCuller::OcclusionCuller(u32 depth_width, u32 depth_height)
    : depth_buffer_(depth_width, depth_height) {}

void OcclusionCuller::set_view_projection(const Mat4& vp) {
    view_proj_ = vp;
    frustum_   = Frustum::from_view_projection(vp);
    depth_buffer_.clear();
}

void OcclusionCuller::submit_occluder(const CullAABB& aabb) {
    depth_buffer_.rasterize_occluder(aabb, view_proj_);
}

std::vector<u32> OcclusionCuller::cull(const std::vector<CullObject>& objects) const {
    total_tested_     = static_cast<u32>(objects.size());
    frustum_culled_   = 0;
    occlusion_culled_ = 0;

    std::vector<u32> visible;
    visible.reserve(objects.size());

    for (const auto& obj : objects) {
        if (!frustum_.test_aabb(obj.bounds)) {
            ++frustum_culled_;
            continue;
        }
        if (depth_buffer_.width() > 0 && depth_buffer_.is_occluded(obj.bounds, view_proj_)) {
            ++occlusion_culled_;
            continue;
        }
        visible.push_back(obj.id);
    }
    return visible;
}

std::vector<u32> OcclusionCuller::frustum_cull(const std::vector<CullObject>& objects) const {
    total_tested_     = static_cast<u32>(objects.size());
    frustum_culled_   = 0;
    occlusion_culled_ = 0;

    std::vector<u32> visible;
    visible.reserve(objects.size());

    for (const auto& obj : objects) {
        if (!frustum_.test_aabb(obj.bounds)) {
            ++frustum_culled_;
            continue;
        }
        visible.push_back(obj.id);
    }
    return visible;
}

} // namespace nexus
