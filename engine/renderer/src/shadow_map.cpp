#include "nexus/renderer/shadow_map.h"
#include "nexus/core/log.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace nexus {

// ── CascadedShadowMap ───────────────────────────────────────────────────────

void CascadedShadowMap::init(rhi::RHI* rhi, const Config& config) {
    rhi_ = rhi;
    config_ = config;
    splits_.resize(config_.num_cascades + 1);

    for (u32 i = 0; i < config_.num_cascades; ++i) {
        rhi::TextureDesc tex_desc;
        tex_desc.width = config_.resolution;
        tex_desc.height = config_.resolution;
        tex_desc.format = rhi::TextureFormat::Depth32F;
        tex_desc.min_filter = rhi::TextureFilter::Nearest;
        tex_desc.mag_filter = rhi::TextureFilter::Nearest;
        tex_desc.wrap_s = rhi::TextureWrap::ClampToEdge;
        tex_desc.wrap_t = rhi::TextureWrap::ClampToEdge;
        depth_textures_[i] = rhi_->create_texture(tex_desc);

        rhi::FramebufferDesc fb_desc;
        fb_desc.width = config_.resolution;
        fb_desc.height = config_.resolution;
        fb_desc.has_depth = true;
        framebuffers_[i] = rhi_->create_framebuffer(fb_desc);
    }

    NX_INFO("CascadedShadowMap initialized: {} cascades, {}x{}",
            config_.num_cascades, config_.resolution, config_.resolution);
}

void CascadedShadowMap::shutdown() {
    if (!rhi_) return;
    for (u32 i = 0; i < config_.num_cascades; ++i) {
        rhi_->destroy_framebuffer(framebuffers_[i]);
        rhi_->destroy_texture(depth_textures_[i]);
    }
    rhi_ = nullptr;
}

void CascadedShadowMap::compute_cascade_splits(float near, float far) {
    float lambda = config_.cascade_split_lambda;
    float range = far - near;
    float ratio = far / near;

    splits_[0] = near;
    for (u32 i = 1; i <= config_.num_cascades; ++i) {
        float p = static_cast<float>(i) / static_cast<float>(config_.num_cascades);
        float log_split = near * std::pow(ratio, p);
        float uniform_split = near + range * p;
        splits_[i] = math::lerp(uniform_split, log_split, lambda);
    }
    splits_[config_.num_cascades] = far;
}

static std::vector<Vec4> get_frustum_corners(const Mat4& view_proj) {
    Mat4 inv = glm::inverse(view_proj);
    std::vector<Vec4> corners;
    for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 2; ++y) {
            for (int z = 0; z < 2; ++z) {
                Vec4 pt = inv * Vec4(
                    2.0f * static_cast<float>(x) - 1.0f,
                    2.0f * static_cast<float>(y) - 1.0f,
                    2.0f * static_cast<float>(z) - 1.0f,
                    1.0f);
                corners.push_back(pt / pt.w);
            }
        }
    }
    return corners;
}

Mat4 CascadedShadowMap::compute_light_matrix(Vec3 light_dir,
                                              const std::vector<Vec4>& frustum_corners) const {
    Vec3 center(0.0f);
    for (const auto& c : frustum_corners) {
        center += Vec3(c);
    }
    center /= static_cast<float>(frustum_corners.size());

    Vec3 dir_norm = glm::normalize(light_dir);
    // Choose an up vector that isn't parallel to the light direction
    Vec3 up = Vec3(0, 1, 0);
    if (std::abs(glm::dot(dir_norm, up)) > 0.99f) {
        up = Vec3(0, 0, 1);
    }
    Mat4 light_view = glm::lookAt(center - dir_norm, center, up);

    float min_x = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float min_y = std::numeric_limits<float>::max();
    float max_y = std::numeric_limits<float>::lowest();
    float min_z = std::numeric_limits<float>::max();
    float max_z = std::numeric_limits<float>::lowest();

    for (const auto& c : frustum_corners) {
        Vec4 lc = light_view * c;
        min_x = std::min(min_x, lc.x);
        max_x = std::max(max_x, lc.x);
        min_y = std::min(min_y, lc.y);
        max_y = std::max(max_y, lc.y);
        min_z = std::min(min_z, lc.z);
        max_z = std::max(max_z, lc.z);
    }

    // Extend Z range to capture shadow casters behind the frustum
    float z_mult = 10.0f;
    if (min_z < 0) min_z *= z_mult;
    else min_z /= z_mult;
    if (max_z < 0) max_z /= z_mult;
    else max_z *= z_mult;

    Mat4 light_proj = glm::ortho(min_x, max_x, min_y, max_y, min_z, max_z);
    return light_proj * light_view;
}

void CascadedShadowMap::update(const Camera3D& camera, Vec3 light_direction) {
    float near = camera.near_clip;
    float far = std::min(camera.far_clip, config_.shadow_distance);
    compute_cascade_splits(near, far);

    // Get actual camera aspect ratio for correct frustum corners
    Mat4 cam_proj = camera.get_projection_matrix();
    // Extract aspect from projection matrix: P[1][1] / P[0][0]
    float aspect = (cam_proj[0][0] != 0.0f) ? cam_proj[1][1] / cam_proj[0][0] : 16.0f / 9.0f;

    for (u32 i = 0; i < config_.num_cascades; ++i) {
        // Build sub-frustum projection for this cascade with correct aspect
        Mat4 proj = glm::perspective(
            glm::radians(camera.fov),
            aspect,
            splits_[i], splits_[i + 1]);
        Mat4 view = camera.get_view_matrix();

        auto corners = get_frustum_corners(proj * view);
        cascades_[i].light_view_projection = compute_light_matrix(light_direction, corners);
        cascades_[i].split_depth = splits_[i + 1];
    }
}

rhi::FramebufferHandle CascadedShadowMap::framebuffer(u32 cascade) const {
    return (cascade < config_.num_cascades) ? framebuffers_[cascade] : rhi::INVALID_HANDLE;
}

rhi::TextureHandle CascadedShadowMap::depth_texture(u32 cascade) const {
    return (cascade < config_.num_cascades) ? depth_textures_[cascade] : rhi::INVALID_HANDLE;
}

// ── PointLightShadow ────────────────────────────────────────────────────────

void PointLightShadow::init(rhi::RHI* rhi, const Config& config) {
    rhi_ = rhi;
    config_ = config;

    rhi::TextureDesc tex_desc;
    tex_desc.width = config_.resolution;
    tex_desc.height = config_.resolution;
    tex_desc.format = rhi::TextureFormat::Depth32F;
    tex_desc.min_filter = rhi::TextureFilter::Nearest;
    tex_desc.mag_filter = rhi::TextureFilter::Nearest;
    tex_desc.wrap_s = rhi::TextureWrap::ClampToEdge;
    tex_desc.wrap_t = rhi::TextureWrap::ClampToEdge;
    depth_texture_ = rhi_->create_texture(tex_desc);

    rhi::FramebufferDesc fb_desc;
    fb_desc.width = config_.resolution;
    fb_desc.height = config_.resolution;
    fb_desc.has_depth = true;
    framebuffer_ = rhi_->create_framebuffer(fb_desc);
}

void PointLightShadow::shutdown() {
    if (!rhi_) return;
    rhi_->destroy_framebuffer(framebuffer_);
    rhi_->destroy_texture(depth_texture_);
    rhi_ = nullptr;
}

void PointLightShadow::update(Vec3 pos) {
    float aspect = 1.0f;
    Mat4 proj = glm::perspective(glm::radians(90.0f), aspect,
                                  config_.near_plane, config_.far_plane);

    // +X, -X, +Y, -Y, +Z, -Z
    face_matrices_[0] = proj * glm::lookAt(pos, pos + Vec3(1, 0, 0), Vec3(0, -1, 0));
    face_matrices_[1] = proj * glm::lookAt(pos, pos + Vec3(-1, 0, 0), Vec3(0, -1, 0));
    face_matrices_[2] = proj * glm::lookAt(pos, pos + Vec3(0, 1, 0), Vec3(0, 0, 1));
    face_matrices_[3] = proj * glm::lookAt(pos, pos + Vec3(0, -1, 0), Vec3(0, 0, -1));
    face_matrices_[4] = proj * glm::lookAt(pos, pos + Vec3(0, 0, 1), Vec3(0, -1, 0));
    face_matrices_[5] = proj * glm::lookAt(pos, pos + Vec3(0, 0, -1), Vec3(0, -1, 0));
}

} // namespace nexus
