#include "nexus/editor/editor_panels.h"
#include "nexus/scene/scene.h"
#include "nexus/scene/components.h"
#include "nexus/scene/registry.h"
#include "nexus/renderer/forward_renderer_3d.h"
#include "nexus/renderer/batch_renderer_2d.h"
#include "nexus/core/log.h"
#include <algorithm>
#include <cmath>

namespace nexus::editor {

// ── ViewportPanel ──────────────────────────────────────────────────────────

void ViewportPanel::on_render() {
    if (!scene_) return;

    auto& registry = scene_->registry();

    // ── 3D rendering pass ──────────────────────────────────────────────
    if (renderer_3d_) {
        // Find camera
        Camera3D cam;
        bool found_camera = false;
        registry.each_with<CameraComponent, Transform3DComponent>(
            [&](u32 /*entity*/, const CameraComponent& cc, const Transform3DComponent& tc) {
                if (found_camera) return;
                cam.fov = cc.fov;
                cam.near_clip = cc.near_clip;
                cam.far_clip = cc.far_clip;
                // Convert quaternion to yaw/pitch
                Quat q = cc.orientation;
                float sinp = 2.0f * (q.w * q.x - q.z * q.y);
                cam.pitch = std::abs(sinp) >= 1.0f
                    ? std::copysign(90.0f, sinp)
                    : static_cast<float>(std::asin(sinp) * 180.0 / 3.14159265358979);
                cam.yaw = static_cast<float>(std::atan2(
                    2.0f * (q.w * q.y + q.x * q.z),
                    1.0f - 2.0f * (q.x * q.x + q.y * q.y)) * 180.0 / 3.14159265358979);
                cam.position = tc.world_matrix[3];
                float aspect = (height_ > 0) ? static_cast<float>(width_) / static_cast<float>(height_) : 16.0f / 9.0f;
                cam.set_perspective(aspect);
                found_camera = true;
            });

        if (found_camera) {
            renderer_3d_->begin_frame(cam);

            // Set directional lights
            registry.each_with<DirectionalLightComponent>(
                [&](u32 /*entity*/, const DirectionalLightComponent& dl) {
                    renderer::DirectionalLight light;
                    light.direction = dl.direction;
                    light.color = dl.color;
                    light.intensity = dl.intensity;
                    renderer_3d_->set_directional_light(light);
                });

            // Add point lights
            registry.each_with<PointLightComponent, Transform3DComponent>(
                [&](u32 /*entity*/, const PointLightComponent& pl, const Transform3DComponent& tc) {
                    renderer::PointLight light;
                    light.position = Vec3(tc.world_matrix[3]);
                    light.color = pl.color;
                    light.intensity = pl.intensity;
                    light.radius = pl.radius;
                    renderer_3d_->add_point_light(light);
                });

            // Draw meshes
            registry.each_with<MeshRendererComponent, Transform3DComponent>(
                [&](u32 /*entity*/, const MeshRendererComponent& mr, const Transform3DComponent& tc) {
                    if (mr.mesh_id != 0) {
                        // Mesh rendering would use the cached meshes
                        (void)tc;
                    }
                });

            renderer_3d_->end_frame();
        }
    }

    // ── 2D rendering pass ──────────────────────────────────────────────
    if (renderer_2d_) {
        renderer_2d_->begin_batch();
        registry.each_with<SpriteRendererComponent, Transform2DComponent>(
            [&](u32 /*entity*/, const SpriteRendererComponent& sr, const Transform2DComponent& tc) {
                renderer_2d_->draw_quad(
                    {tc.world_position.x, tc.world_position.y},
                    {tc.world_scale.x * sr.size.x, tc.world_scale.y * sr.size.y},
                    tc.world_rotation,
                    sr.color,
                    sr.texture_id
                );
            });
        renderer_2d_->end_batch();
    }
}

// ── HierarchyPanel ─────────────────────────────────────────────────────────

void HierarchyPanel::add_to_selection(u32 entity) {
    if (!is_multi_selected(entity)) {
        multi_selection_.push_back(entity);
    }
}

void HierarchyPanel::remove_from_selection(u32 entity) {
    multi_selection_.erase(
        std::remove(multi_selection_.begin(), multi_selection_.end(), entity),
        multi_selection_.end());
}

bool HierarchyPanel::is_multi_selected(u32 entity) const {
    return std::find(multi_selection_.begin(), multi_selection_.end(), entity)
           != multi_selection_.end();
}

// ── InspectorPanel ──────────────────────────────────────────────────────────

std::vector<PropertyEdit> InspectorPanel::drain_edits() {
    std::vector<PropertyEdit> result;
    std::swap(result, pending_edits_);
    return result;
}

// ── ConsolePanel ────────────────────────────────────────────────────────────

void ConsolePanel::add_message(const std::string& text, LogLevel level) {
    ConsoleMessage msg;
    msg.text = text;
    msg.level = level;
    messages_.push_back(std::move(msg));

    // Prune if exceeding max
    while (messages_.size() > max_messages_) {
        messages_.erase(messages_.begin());
    }
}

void ConsolePanel::clear() {
    messages_.clear();
}

void ConsolePanel::set_level_filter(LogLevel level, bool show) {
    switch (level) {
        case LogLevel::Info:    show_info_ = show; break;
        case LogLevel::Warning: show_warning_ = show; break;
        case LogLevel::Error:   show_error_ = show; break;
        case LogLevel::Debug:   show_debug_ = show; break;
    }
}

bool ConsolePanel::is_level_shown(LogLevel level) const {
    switch (level) {
        case LogLevel::Info:    return show_info_;
        case LogLevel::Warning: return show_warning_;
        case LogLevel::Error:   return show_error_;
        case LogLevel::Debug:   return show_debug_;
    }
    return true;
}

// ── AssetBrowserPanel ───────────────────────────────────────────────────────

void AssetBrowserPanel::navigate_to(const std::string& path) {
    // Trim history forward if we navigated back then go somewhere new
    if (history_index_ >= 0 &&
        history_index_ + 1 < static_cast<i32>(history_.size())) {
        history_.erase(history_.begin() + history_index_ + 1, history_.end());
    }

    history_.push_back(path);
    history_index_ = static_cast<i32>(history_.size()) - 1;
    current_path_ = path;
}

void AssetBrowserPanel::navigate_up() {
    auto pos = current_path_.find_last_of('/');
    if (pos != std::string::npos && pos > 0) {
        navigate_to(current_path_.substr(0, pos));
    } else if (!current_path_.empty()) {
        navigate_to("");
    }
}

void AssetBrowserPanel::go_back() {
    if (can_go_back()) {
        history_index_--;
        current_path_ = history_[static_cast<size_t>(history_index_)];
    }
}

void AssetBrowserPanel::go_forward() {
    if (can_go_forward()) {
        history_index_++;
        current_path_ = history_[static_cast<size_t>(history_index_)];
    }
}

} // namespace nexus::editor
