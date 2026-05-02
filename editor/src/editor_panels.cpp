#include "nexus/editor/editor_panels.h"
#include "nexus/editor/imgui_layer.h"
#include "nexus/editor/editor_state.h"
#include "nexus/editor/undo_redo.h"
#include "nexus/editor/component_registry.h"
#include "nexus/animation/animation_clip.h"
#include "nexus/scripting/script_component.h"
#include "nexus/scene/scene.h"
#include "nexus/scene/components.h"
#include "nexus/scene/registry.h"
#include "nexus/scene/hierarchy.h"
#include "nexus/renderer/forward_renderer_3d.h"
#include "nexus/renderer/batch_renderer_2d.h"
#include "nexus/renderer/debug_renderer.h"
#include "nexus/renderer/camera.h"
#include "nexus/platform/input.h"
#include "nexus/rhi/rhi.h"
#include "nexus/core/log.h"
#include "nexus/perf/profiler.h"
#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
// GLM_ENABLE_EXPERIMENTAL is set globally by cmake/CompilerFlags.cmake.
#include <glm/gtx/matrix_decompose.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <filesystem>

namespace nexus::editor {

// Per-entity icon glyph — picks a meaningful prefix based on the dominant
// component on the entity.  Order matters: the first matching component wins,
// matching Unity's Hierarchy icon priority (Camera > Light > Mesh > Sprite >
// Audio > Empty).  Pure ASCII so we don't depend on icon-font assets.
static const char* hierarchy_icon(const Registry& reg, Entity e) {
    if (reg.has_component<CameraComponent>(e))            return "[C]";
    if (reg.has_component<DirectionalLightComponent>(e))  return "[D]";
    if (reg.has_component<PointLightComponent>(e))        return "[P]";
    if (reg.has_component<SpotLightComponent>(e))         return "[S]";
    if (reg.has_component<MeshRendererComponent>(e))      return "[M]";
    if (reg.has_component<SpriteRendererComponent>(e))    return "[~]";
    if (reg.has_component<AudioSourceComponent>(e) ||
        reg.has_component<AudioListenerComponent>(e))     return "[A]";
    return "[ ]";
}

// True if `entity` itself carries a LockedComponent or inherits one via any
// ancestor along its HierarchyComponent chain.  Matches Unity's SceneVis
// semantics where locking a parent locks every descendant. Walks up at most
// once per call so the cost is O(depth).
static bool is_entity_effectively_locked(const Registry& reg, Entity entity) {
    if (!reg.alive(entity)) return false;
    Entity cursor = entity;
    while (cursor != INVALID_ENTITY && reg.alive(cursor)) {
        if (reg.has_component<LockedComponent>(cursor)) return true;
        if (!reg.has_component<HierarchyComponent>(cursor)) break;
        cursor = reg.get_component<HierarchyComponent>(cursor).parent;
    }
    return false;
}

// ── ViewportPanel ──────────────────────────────────────────────────────────

ViewportPanel::~ViewportPanel() {
    release_framebuffer();
}

void ViewportPanel::release_framebuffer() {
    if (rhi_ && fbo_ != nexus::rhi::INVALID_HANDLE) {
        rhi_->destroy_framebuffer(fbo_);
    }
    fbo_ = nexus::rhi::INVALID_HANDLE;
    fbo_w_ = 0;
    fbo_h_ = 0;
}

void ViewportPanel::ensure_framebuffer(u32 w, u32 h) {
    if (!rhi_ || w == 0 || h == 0) return;
    // Hysteresis: avoid destroy+recreate on 1-pixel oscillations from dock
    // layout rounding — those thrash the driver and caused visible flicker
    // as the ImGui::Image texture was swapped every frame.
    if (fbo_ != nexus::rhi::INVALID_HANDLE) {
        const u32 dw = (w > fbo_w_) ? (w - fbo_w_) : (fbo_w_ - w);
        const u32 dh = (h > fbo_h_) ? (h - fbo_h_) : (fbo_h_ - h);
        if (dw <= 2 && dh <= 2) return;
    }
    release_framebuffer();
    nexus::rhi::FramebufferDesc desc;
    desc.width = w;
    desc.height = h;
    desc.color_attachments = { nexus::rhi::TextureFormat::RGBA8 };
    desc.has_depth = true;
    fbo_ = rhi_->create_framebuffer(desc);
    fbo_w_ = w;
    fbo_h_ = h;
}

// ── SceneView preset helpers ───────────────────────────────────────────────

bool ViewportPanel::preset_to_yaw_pitch(SceneCameraPreset p,
                                        f32& out_yaw, f32& out_pitch) {
    // Yaw is rotation around +Y (CCW from +Z), pitch around +X.  Values
    // here mirror Unity's "right-hand orbit" convention so the editor
    // camera ends up looking down each axis exactly.
    switch (p) {
        case SceneCameraPreset::Top:    out_yaw = 0.0f;   out_pitch =  -89.9f; return true;
        case SceneCameraPreset::Bottom: out_yaw = 0.0f;   out_pitch =   89.9f; return true;
        case SceneCameraPreset::Front:  out_yaw = 90.0f;  out_pitch =   0.0f;  return true;
        case SceneCameraPreset::Back:   out_yaw = 270.0f; out_pitch =   0.0f;  return true;
        case SceneCameraPreset::Right:  out_yaw = 180.0f; out_pitch =   0.0f;  return true;
        case SceneCameraPreset::Left:   out_yaw = 0.0f;   out_pitch =   0.0f;  return true;
        case SceneCameraPreset::Iso:    out_yaw = 45.0f;  out_pitch = -25.0f;  return true;
        case SceneCameraPreset::Free:
        default:                        return false;
    }
}

void ViewportPanel::apply_camera_preset(SceneCameraPreset p) {
    f32 yaw = 0.0f, pitch = 0.0f;
    if (!preset_to_yaw_pitch(p, yaw, pitch)) return;
    editor_cam_yaw_deg_   = yaw;
    editor_cam_pitch_deg_ = pitch;
}

void ViewportPanel::render_scene_to_fbo() {
    if (!scene_ || !rhi_ || fbo_ == nexus::rhi::INVALID_HANDLE) return;
    auto& registry = scene_->registry();

    rhi_->bind_framebuffer(fbo_);
    rhi_->set_viewport(0, 0, static_cast<i32>(fbo_w_), static_cast<i32>(fbo_h_));
    rhi_->clear({0.10f, 0.11f, 0.13f, 1.0f}, 1.0f);

    // Apply shading mode polygon fill for SceneView; GameView always renders
    // shaded so users see what the runtime camera sees.
    const bool wireframe_pass =
        camera_mode_ == ViewportCameraMode::SceneView &&
        (shading_mode_ == SceneShadingMode::Wireframe ||
         shading_mode_ == SceneShadingMode::ShadedWireframe);
    rhi_->set_polygon_mode(wireframe_pass
                               ? nexus::rhi::PolygonMode::Line
                               : nexus::rhi::PolygonMode::Fill);

    last_cam_ready_ = false;

    // Reset per-frame stats; populated as we walk the registry.  We count at
    // the panel layer rather than the renderer because the renderer's
    // public API (draw_mesh / draw_quad) is what actually issues GPU work
    // here, and counting here keeps the renderer change-set tight.
    ViewportRenderStats stats{};
    if (renderer_3d_) {
        Camera3D cam;
        bool ready = false;

        auto decode_camera = [&](CameraComponent& cc, Transform3DComponent& tc) {
            cam.fov = cc.fov;
            cam.near_clip = cc.near_clip;
            cam.far_clip  = cc.far_clip;
            Quat q = cc.orientation;
            float sinp = 2.0f * (q.w * q.x - q.z * q.y);
            cam.pitch = std::abs(sinp) >= 1.0f
                ? std::copysign(90.0f, sinp)
                : static_cast<float>(std::asin(sinp) * 180.0 / 3.14159265358979);
            cam.yaw = static_cast<float>(std::atan2(
                2.0f * (q.w * q.y + q.x * q.z),
                1.0f - 2.0f * (q.x * q.x + q.y * q.y)) * 180.0 / 3.14159265358979);
            cam.position = tc.world_matrix[3];
            cam.set_perspective(static_cast<float>(fbo_w_) /
                                static_cast<float>(fbo_h_));
        };

        if (camera_mode_ == ViewportCameraMode::SceneView) {
            // Free-orbit editor camera.
            cam.fov   = 60.0f;
            cam.near_clip = 0.1f;
            cam.far_clip  = 500.0f;
            // Compute forward in Camera3D's convention so the camera position
            // (derived here) agrees with the view matrix built later.
            const float yaw_r   = editor_cam_yaw_deg_   * 3.14159265358979f / 180.0f;
            const float pitch_r = editor_cam_pitch_deg_ * 3.14159265358979f / 180.0f;
            const Vec3 forward(std::cos(yaw_r) * std::cos(pitch_r),
                               std::sin(pitch_r),
                               std::sin(yaw_r) * std::cos(pitch_r));
            cam.position = editor_cam_target_ - forward * editor_cam_distance_;
            cam.yaw   = editor_cam_yaw_deg_;
            cam.pitch = editor_cam_pitch_deg_;
            cam.set_perspective(static_cast<float>(fbo_w_) /
                                static_cast<float>(fbo_h_));
            ready = true;
        } else {
            // GameView: prefer the camera flagged `is_primary`.  Matches the
            // runtime's selection, and lets the user designate a "main camera"
            // the same way Unity does.  Fall back to the first CameraComponent
            // if none are primary so a freshly-created scene still renders.
            registry.each<CameraComponent, Transform3DComponent>(
                [&](u32, CameraComponent& cc, Transform3DComponent& tc) {
                    if (ready || !cc.is_primary) return;
                    decode_camera(cc, tc);
                    ready = true;
                });
            if (!ready) {
                registry.each<CameraComponent, Transform3DComponent>(
                    [&](u32, CameraComponent& cc, Transform3DComponent& tc) {
                        if (ready) return;
                        decode_camera(cc, tc);
                        ready = true;
                    });
            }
        }

        if (ready) {
            last_view_ = cam.get_view_matrix();
            last_proj_ = cam.get_projection_matrix();
            last_cam_ready_ = true;
            renderer_3d_->begin_frame(cam);
            registry.each<DirectionalLightComponent>(
                [&](u32, DirectionalLightComponent& dl) {
                    DirectionalLight light;
                    light.direction = dl.direction;
                    light.color = dl.color;
                    light.intensity = dl.intensity;
                    renderer_3d_->set_directional_light(light);
                });
            registry.each<PointLightComponent, Transform3DComponent>(
                [&](u32, PointLightComponent& pl, Transform3DComponent& tc) {
                    PointLight light;
                    light.position = Vec3(tc.world_matrix[3]);
                    light.color = pl.color;
                    light.intensity = pl.intensity;
                    light.radius = pl.radius;
                    renderer_3d_->add_point_light(light);
                });
            registry.each<MeshRendererComponent, Transform3DComponent>(
                [&](u32, MeshRendererComponent& mr, Transform3DComponent& tc) {
                    if (mr.mesh_id == 0) return;
                    auto it = mesh_registry_.find(mr.mesh_id);
                    if (it == mesh_registry_.end() || !it->second) return;
                    renderer_3d_->draw_mesh(*it->second, tc.world_matrix, mr.tint);
                    ++stats.mesh_draw_calls;
                    ++stats.draw_calls;
                    const auto& m = *it->second;
                    stats.vertices  += static_cast<u32>(m.vertices.size());
                    stats.triangles += static_cast<u32>(m.indices.size() / 3u);
                });
            renderer_3d_->end_frame();
            // ForwardRenderer3D rebinds the lit pipeline once per frame for
            // begin / end, plus one shadow-pass setup when shadows are on.
            // Conservative estimate ≥ 1; if shadows are enabled count both.
            stats.set_pass_calls += renderer_3d_->shadow_map() ? 2u : 1u;
        }
    }

    if (renderer_2d_) {
        Camera2D cam2d;
        cam2d.set_projection(static_cast<float>(fbo_w_),
                             static_cast<float>(fbo_h_));
        renderer_2d_->begin(cam2d);
        registry.each<SpriteRendererComponent, Transform2DComponent>(
            [&](u32, SpriteRendererComponent& sr, Transform2DComponent& tc) {
                renderer_2d_->draw_quad(
                    {tc.world_position.x, tc.world_position.y},
                    {tc.world_scale.x * sr.size.x, tc.world_scale.y * sr.size.y},
                    tc.world_rotation,
                    sr.color);
                ++stats.sprite_draw_calls;
                ++stats.draw_calls;
                stats.vertices  += 4u;  // quad
                stats.triangles += 2u;
            });
        renderer_2d_->end();
        // BatchRenderer2D batches into a single draw call but rebinds its
        // sprite pipeline at begin/end — count as 1 set-pass.
        if (stats.sprite_draw_calls > 0) ++stats.set_pass_calls;
    }

    // SceneView overlays — grid, world axes, selected entity outline.
    // SceneView draws them when show_scene_gizmos_ is on (Unity Gizmos
    // toggle); GameView only draws when the user has ticked the dedicated
    // Gizmos toolbar toggle.
    const bool overlays_on =
        last_cam_ready_ && debug_renderer_ &&
        ((camera_mode_ == ViewportCameraMode::SceneView && show_scene_gizmos_) ||
         (camera_mode_ == ViewportCameraMode::GameView  && show_game_gizmos_));
    if (overlays_on) {
        Camera3D cam;
        cam.yaw       = editor_cam_yaw_deg_;
        cam.pitch     = editor_cam_pitch_deg_;
        cam.fov       = 60.0f;
        cam.near_clip = 0.1f;
        cam.far_clip  = 500.0f;
        const float yaw_r   = editor_cam_yaw_deg_   * 3.14159265358979f / 180.0f;
        const float pitch_r = editor_cam_pitch_deg_ * 3.14159265358979f / 180.0f;
        const Vec3 forward(std::cos(yaw_r) * std::cos(pitch_r),
                           std::sin(pitch_r),
                           std::sin(yaw_r) * std::cos(pitch_r));
        cam.position = editor_cam_target_ - forward * editor_cam_distance_;
        cam.set_perspective(static_cast<float>(fbo_w_) /
                            static_cast<float>(fbo_h_));

        debug_renderer_->begin(cam);
        if (show_grid_) {
            debug_renderer_->draw_grid(20.0f, 1.0f, Vec4(0.35f, 0.35f, 0.40f, 1.0f));
        }
        if (show_axes_) {
            debug_renderer_->draw_axis(Vec3(0.0f), 1.0f);
        }
        debug_renderer_->end();
    }

    // Always restore Fill before unbinding so other panels (or ImGui's own
    // draw pass) don't accidentally inherit Line mode and render their UI
    // as wireframes.
    rhi_->set_polygon_mode(nexus::rhi::PolygonMode::Fill);

    rhi_->unbind_framebuffer();

    // Publish this frame's stats so the Stats overlay reads honest numbers
    // next on_render() pass.
    last_stats_ = stats;
}

void ViewportPanel::on_render() {
    if (!visible_) return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    bool open = visible_;
    if (!ImGui::Begin(title_.c_str(), &open,
                      ImGuiWindowFlags_NoScrollbar |
                      ImGuiWindowFlags_NoScrollWithMouse |
                      ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        ImGui::PopStyleVar();
        visible_ = open;
        return;
    }
    visible_ = open;
    focused_ = ImGui::IsWindowFocused();

    // ── Toolbar ─────────────────────────────────────────────────────────
    // Gizmo mode + overlay toggles live in a compact menu bar above the
    // render image so the viewport stays interactive even when the
    // gizmo/grid aren't needed.
    if (ImGui::BeginMenuBar()) {
        if (camera_mode_ == ViewportCameraMode::SceneView) {
            // ── Shading mode dropdown ────────────────────────────────────
            // Mirrors Unity's "Shaded" picker.  Backend-side this drives
            // RHI::set_polygon_mode for Wireframe / ShadedWireframe; debug
            // views (Albedo/Normals/Overdraw) are slot-reserved and treated
            // as Shaded today (renderer-level work to follow).
            const char* shading_label = "Shaded";
            switch (shading_mode_) {
                case SceneShadingMode::Shaded:           shading_label = "Shaded"; break;
                case SceneShadingMode::Wireframe:        shading_label = "Wireframe"; break;
                case SceneShadingMode::ShadedWireframe:  shading_label = "Shaded Wireframe"; break;
                case SceneShadingMode::Albedo:           shading_label = "Albedo"; break;
                case SceneShadingMode::Normals:          shading_label = "Normals"; break;
                case SceneShadingMode::Overdraw:         shading_label = "Overdraw"; break;
            }
            if (ImGui::BeginMenu(shading_label)) {
                auto entry = [&](const char* label, SceneShadingMode m) {
                    if (ImGui::MenuItem(label, nullptr, shading_mode_ == m)) {
                        shading_mode_ = m;
                    }
                };
                entry("Shaded",            SceneShadingMode::Shaded);
                entry("Wireframe",         SceneShadingMode::Wireframe);
                entry("Shaded Wireframe",  SceneShadingMode::ShadedWireframe);
                ImGui::Separator();
                ImGui::TextDisabled("Debug Views");
                entry("Albedo",   SceneShadingMode::Albedo);
                entry("Normals",  SceneShadingMode::Normals);
                entry("Overdraw", SceneShadingMode::Overdraw);
                ImGui::EndMenu();
            }
            ImGui::Separator();
            // 2D / 3D toggle — Unity's "2D" pill at the top-left of the
            // Scene tab.  When enabled, the editor camera locks to a top-
            // down ortho-style framing on the XY plane.
            if (ImGui::MenuItem(view_2d_ ? "2D" : "3D", nullptr, view_2d_)) {
                view_2d_ = !view_2d_;
                if (view_2d_) {
                    // Snap to a clean top-down framing so the toggle has
                    // visible effect without forcing an additional click.
                    apply_camera_preset(SceneCameraPreset::Top);
                }
            }
            ImGui::Separator();
            // Gizmos toggle — controls visibility of the SceneView's grid
            // and world-axes overlays.  Selected-entity ImGuizmo handles
            // remain on regardless so the user never loses transform
            // control.
            ImGui::MenuItem("Gizmos", nullptr, &show_scene_gizmos_);
            ImGui::Separator();
            // Camera preset dropdown — Unity's "ViewCube" / preset menu.
            // Picking one snaps the editor camera onto that orientation.
            if (ImGui::BeginMenu("Camera")) {
                auto preset = [&](const char* label, SceneCameraPreset p) {
                    if (ImGui::MenuItem(label)) apply_camera_preset(p);
                };
                preset("Top",    SceneCameraPreset::Top);
                preset("Bottom", SceneCameraPreset::Bottom);
                preset("Front",  SceneCameraPreset::Front);
                preset("Back",   SceneCameraPreset::Back);
                preset("Left",   SceneCameraPreset::Left);
                preset("Right",  SceneCameraPreset::Right);
                ImGui::Separator();
                preset("Iso",    SceneCameraPreset::Iso);
                ImGui::EndMenu();
            }
            ImGui::Separator();
            const bool trans = gizmo_mode_ == GizmoMode::Translate;
            const bool rot   = gizmo_mode_ == GizmoMode::Rotate;
            const bool scl   = gizmo_mode_ == GizmoMode::Scale;
            if (ImGui::MenuItem("T", nullptr, trans)) gizmo_mode_ = GizmoMode::Translate;
            if (ImGui::MenuItem("R", nullptr, rot))   gizmo_mode_ = GizmoMode::Rotate;
            if (ImGui::MenuItem("S", nullptr, scl))   gizmo_mode_ = GizmoMode::Scale;
            ImGui::Separator();
            bool local = gizmo_space_ == GizmoSpace::Local;
            if (ImGui::MenuItem(local ? "Local" : "World")) toggle_gizmo_space();
            ImGui::Separator();
            // Snap: toggle + step size for the active gizmo operation.  This
            // matches the Unity overlay's snap affordance and feeds the value
            // into ImGuizmo::Manipulate on this frame.
            ImGui::MenuItem("Snap", nullptr, &snap_enabled_);
            if (snap_enabled_) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(60.0f);
                switch (gizmo_mode_) {
                    case GizmoMode::Translate:
                        ImGui::DragFloat("##SnapT", &snap_translate_,
                                         0.01f, 0.001f, 100.0f, "%.3f");
                        break;
                    case GizmoMode::Rotate:
                        ImGui::DragFloat("##SnapR", &snap_rotate_deg_,
                                         0.5f, 0.1f, 180.0f, "%.1f°");
                        break;
                    case GizmoMode::Scale:
                        ImGui::DragFloat("##SnapS", &snap_scale_,
                                         0.01f, 0.001f, 10.0f, "%.3f");
                        break;
                    default: break;
                }
            }
            ImGui::Separator();
            ImGui::MenuItem("Grid", nullptr, &show_grid_);
            ImGui::MenuItem("Axes", nullptr, &show_axes_);
        } else {
            // ── GameView toolbar: Display / Aspect / Scale / Gizmos / Stats
            // Unity's Game tab exposes these as dropdowns on the top strip.  We
            // keep them compact so the render image has maximum breathing room.
            char display_label[32];
            std::snprintf(display_label, sizeof(display_label),
                          "Display %u", static_cast<unsigned>(display_index_) + 1u);
            if (ImGui::BeginMenu(display_label)) {
                for (u8 i = 0; i < 4; ++i) {
                    char item[32];
                    std::snprintf(item, sizeof(item), "Display %u",
                                  static_cast<unsigned>(i) + 1u);
                    if (ImGui::MenuItem(item, nullptr, display_index_ == i)) {
                        display_index_ = i;
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();

            const char* aspect_label = "Free Aspect";
            switch (game_aspect_) {
                case GameAspect::Free:   aspect_label = "Free Aspect"; break;
                case GameAspect::R16x9:  aspect_label = "16:9"; break;
                case GameAspect::R16x10: aspect_label = "16:10"; break;
                case GameAspect::R4x3:   aspect_label = "4:3"; break;
                case GameAspect::R1x1:   aspect_label = "1:1"; break;
            }
            if (ImGui::BeginMenu(aspect_label)) {
                if (ImGui::MenuItem("Free Aspect", nullptr,
                                    game_aspect_ == GameAspect::Free)) {
                    game_aspect_ = GameAspect::Free;
                }
                if (ImGui::MenuItem("16:9",  nullptr, game_aspect_ == GameAspect::R16x9))
                    game_aspect_ = GameAspect::R16x9;
                if (ImGui::MenuItem("16:10", nullptr, game_aspect_ == GameAspect::R16x10))
                    game_aspect_ = GameAspect::R16x10;
                if (ImGui::MenuItem("4:3",   nullptr, game_aspect_ == GameAspect::R4x3))
                    game_aspect_ = GameAspect::R4x3;
                if (ImGui::MenuItem("1:1",   nullptr, game_aspect_ == GameAspect::R1x1))
                    game_aspect_ = GameAspect::R1x1;
                ImGui::EndMenu();
            }
            ImGui::Separator();

            // Scale slider — pixel-perfect zoom into the rendered framebuffer
            // for inspecting low-resolution output (Unity's "Scale").
            ImGui::SetNextItemWidth(80.0f);
            ImGui::SliderFloat("Scale##GameScale", &game_scale_,
                               1.0f, 5.0f, "%.1fx");
            ImGui::Separator();

            ImGui::MenuItem("Gizmos", nullptr, &show_game_gizmos_);
            ImGui::Separator();
            ImGui::MenuItem("Stats", nullptr, &show_stats_);
        }
        ImGui::EndMenuBar();
    }

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float avail_w = std::max(1.0f, avail.x);
    const float avail_h = std::max(1.0f, avail.y);

    // Aspect letterboxing (GameView only).  Delegated to the static helper
    // so unit tests can exercise the same math without an ImGui context.
    float fit_w = avail_w;
    float fit_h = avail_h;
    if (camera_mode_ == ViewportCameraMode::GameView) {
        ViewportPanel::compute_letterbox(avail_w, avail_h, game_aspect_,
                                         game_scale_, fit_w, fit_h);
    }

    const u32 w = static_cast<u32>(std::max(1.0f, fit_w));
    const u32 h = static_cast<u32>(std::max(1.0f, fit_h));
    width_ = w;
    height_ = h;

    if (rhi_) {
        ensure_framebuffer(w, h);
        render_scene_to_fbo();

        if (fbo_ != nexus::rhi::INVALID_HANDLE) {
            const u64 native = rhi_->framebuffer_color_native(fbo_, 0);
            // Center letterboxed image inside available region so sides
            // appear as pillarboxes/letterboxes matching Unity's Game tab.
            if (fit_w < avail_w) {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                                     (avail_w - fit_w) * 0.5f);
            }
            if (fit_h < avail_h) {
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() +
                                     (avail_h - fit_h) * 0.5f);
            }
            const ImVec2 image_pos = ImGui::GetCursorScreenPos();
            // Texture-origin convention varies per backend: GL/WebGL produce
            // bottom-up textures and need V flipped for ImGui's top-down UV
            // space; Metal/Vulkan are already top-down.  Ask the RHI rather
            // than hard-coding the GL convention.
            const bool flip_v = rhi_->textures_are_bottom_up();
            const ImVec2 uv0 = flip_v ? ImVec2(0.0f, 1.0f) : ImVec2(0.0f, 0.0f);
            const ImVec2 uv1 = flip_v ? ImVec2(1.0f, 0.0f) : ImVec2(1.0f, 1.0f);
            ImGui::Image(static_cast<ImTextureID>(native),
                         ImVec2(static_cast<float>(w), static_cast<float>(h)),
                         uv0, uv1);
            hovered_ = ImGui::IsItemHovered();

            // Cross-panel asset drop on the viewport image — accepts the same
            // payload the AssetBrowserPanel emits.  Routing decisions live in
            // the host callback so this layer stays free of asset-format
            // knowledge (scene loading, texture binding, etc.).
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* p =
                        ImGui::AcceptDragDropPayload("NEXUS_ASSET_PATH")) {
                    if (on_asset_drop_) {
                        const char* path = static_cast<const char*>(p->Data);
                        on_asset_drop_(std::string(path));
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // ── Stats HUD (GameView only) ────────────────────────────────
            // Mirrors Unity's Game tab Stats window.  All numbers are honest:
            // FPS from ImGui IO, render size from the fitted framebuffer,
            // draw / triangle / vertex / set-pass counts from
            // ViewportRenderStats populated by render_scene_to_fbo().
            if (camera_mode_ == ViewportCameraMode::GameView && show_stats_) {
                const ImVec2 stats_pos(image_pos.x + 8.0f, image_pos.y + 8.0f);
                ImGui::GetWindowDrawList()->AddRectFilled(
                    stats_pos,
                    ImVec2(stats_pos.x + 220.0f, stats_pos.y + 132.0f),
                    IM_COL32(0, 0, 0, 170), 4.0f);
                ImGui::SetCursorScreenPos(
                    ImVec2(stats_pos.x + 8.0f, stats_pos.y + 4.0f));
                ImGui::BeginGroup();
                const float fps = ImGui::GetIO().Framerate;
                ImGui::Text("FPS: %.1f", fps);
                ImGui::Text("Frame: %.2f ms",
                            1000.0f / std::max(1.0f, fps));
                ImGui::Text("Render: %ux%u", w, h);
                ImGui::Text("Display: %u    Scale: %.1fx",
                            static_cast<unsigned>(display_index_) + 1u,
                            static_cast<double>(game_scale_));
                ImGui::Text("Draw Calls: %u (M:%u S:%u)",
                            last_stats_.draw_calls,
                            last_stats_.mesh_draw_calls,
                            last_stats_.sprite_draw_calls);
                ImGui::Text("Triangles: %u    Verts: %u",
                            last_stats_.triangles, last_stats_.vertices);
                ImGui::Text("Set Pass Calls: %u", last_stats_.set_pass_calls);
                if (scene_) {
                    ImGui::Text("Entities: %zu", scene_->registry().size());
                }
                ImGui::EndGroup();
            }

            // ── ImGuizmo overlay ─────────────────────────────────────────
            // Only draw in SceneView mode and when an entity is selected — the
            // GameView shows what the runtime camera sees and shouldn't be
            // editable.  Manipulate writes the transform back into the entity's
            // Transform3DComponent so changes are immediate and undoable via
            // hierarchy drag-and-drop or hierarchy → undo (separate system).
            if (camera_mode_ == ViewportCameraMode::SceneView &&
                last_cam_ready_ && hierarchy_ && hierarchy_->has_selection() &&
                scene_ && gizmo_mode_ != GizmoMode::None) {
                ImGuizmo::SetOrthographic(false);
                ImGuizmo::SetDrawlist();
                ImGuizmo::SetRect(image_pos.x, image_pos.y,
                                  static_cast<float>(w),
                                  static_cast<float>(h));

                Entity sel = static_cast<Entity>(hierarchy_->selected_entity());
                auto& reg = scene_->registry();
                // Unity-style: a locked ancestor also locks the child, so use
                // the inherited check rather than a direct component test.
                const bool locked = is_entity_effectively_locked(reg, sel);
                if (!locked && reg.alive(sel) &&
                    reg.has_component<Transform3DComponent>(sel)) {
                    auto& tc = reg.get_component<Transform3DComponent>(sel);
                    Mat4 model = tc.to_matrix();

                    ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
                    if (gizmo_mode_ == GizmoMode::Rotate)    op = ImGuizmo::ROTATE;
                    else if (gizmo_mode_ == GizmoMode::Scale) op = ImGuizmo::SCALE;
                    ImGuizmo::MODE mode = (gizmo_space_ == GizmoSpace::Local)
                                             ? ImGuizmo::LOCAL
                                             : ImGuizmo::WORLD;

                    // ImGuizmo expects a snap vector (x,y,z) matched to the
                    // active operation.  Passing nullptr disables snapping.
                    float snap_vec[3] = {0, 0, 0};
                    const float* snap_ptr = nullptr;
                    if (snap_enabled_) {
                        const float v = (gizmo_mode_ == GizmoMode::Rotate)
                                            ? snap_rotate_deg_
                                            : (gizmo_mode_ == GizmoMode::Scale)
                                                  ? snap_scale_
                                                  : snap_translate_;
                        snap_vec[0] = snap_vec[1] = snap_vec[2] = v;
                        snap_ptr = snap_vec;
                    }

                    if (ImGuizmo::Manipulate(glm::value_ptr(last_view_),
                                             glm::value_ptr(last_proj_),
                                             op, mode,
                                             glm::value_ptr(model),
                                             /*deltaMatrix*/ nullptr,
                                             snap_ptr)) {
                        Vec3 t, s;
                        Quat r;
                        Vec3 skew;
                        Vec4 persp;
                        if (glm::decompose(model, s, r, t, skew, persp)) {
                            tc.position = t;
                            tc.rotation = glm::normalize(r);
                            tc.scale    = s;
                        }
                    }
                    gizmo_active_ = ImGuizmo::IsUsing();
                } else {
                    gizmo_active_ = false;
                }
            } else {
                gizmo_active_ = false;
            }
        } else {
            ImGui::TextDisabled("(viewport framebuffer unavailable)");
            hovered_ = false;
        }
    } else {
        ImGui::TextDisabled("(no RHI bound — call bind_rhi())");
        hovered_ = false;
    }

    // ── SceneView camera navigation ─────────────────────────────────────
    // Runs only when the mouse is over the render image — otherwise global
    // shortcuts (WASD) would steal input from the rest of the editor.  This
    // mirrors Unity's Scene view: RMB orbits + WASD fly; MMB pans; scroll
    // zooms.  Target motion keeps the orbit distance steady.
    if (hovered_ && camera_mode_ == ViewportCameraMode::SceneView) {
        const Vec2 md = Input::mouse_delta();
        const float scroll = Input::scroll_delta();
        const bool rmb = Input::mouse_down(MouseButton::Right);
        const bool mmb = Input::mouse_down(MouseButton::Middle);

        if (rmb) {
            editor_cam_yaw_deg_   += md.x * 0.25f;
            editor_cam_pitch_deg_ -= md.y * 0.25f;
            if (editor_cam_pitch_deg_ >  89.0f) editor_cam_pitch_deg_ =  89.0f;
            if (editor_cam_pitch_deg_ < -89.0f) editor_cam_pitch_deg_ = -89.0f;

            // WASD fly in camera space while RMB is held.  Forward must use the
            // same convention as Camera3D (yaw=-90 → -Z) or movement won't agree
            // with the picture: forward = (cos(yaw)*cos(pitch), sin(pitch),
            // sin(yaw)*cos(pitch)).
            const float yaw_r   = editor_cam_yaw_deg_   * 3.14159265358979f / 180.0f;
            const float pitch_r = editor_cam_pitch_deg_ * 3.14159265358979f / 180.0f;
            const Vec3 forward(std::cos(yaw_r) * std::cos(pitch_r),
                               std::sin(pitch_r),
                               std::sin(yaw_r) * std::cos(pitch_r));
            const Vec3 right = glm::normalize(glm::cross(forward, Vec3(0, 1, 0)));
            const float speed = Input::key_down(Key::LeftShift) ? 0.25f : 0.08f;
            if (Input::key_down(Key::W)) editor_cam_target_ += forward * speed;
            if (Input::key_down(Key::S)) editor_cam_target_ -= forward * speed;
            if (Input::key_down(Key::D)) editor_cam_target_ += right   * speed;
            if (Input::key_down(Key::A)) editor_cam_target_ -= right   * speed;
            if (Input::key_down(Key::E)) editor_cam_target_.y += speed;
            if (Input::key_down(Key::Q)) editor_cam_target_.y -= speed;
        }
        if (mmb) {
            // Pan along camera basis — drag the target in the screen plane.
            const float yaw_r   = editor_cam_yaw_deg_   * 3.14159265358979f / 180.0f;
            const float pitch_r = editor_cam_pitch_deg_ * 3.14159265358979f / 180.0f;
            const Vec3 forward(std::cos(yaw_r) * std::cos(pitch_r),
                               std::sin(pitch_r),
                               std::sin(yaw_r) * std::cos(pitch_r));
            const Vec3 right = glm::normalize(glm::cross(forward, Vec3(0, 1, 0)));
            const Vec3 up    = glm::normalize(glm::cross(right, forward));
            const float pan = 0.005f * editor_cam_distance_;
            editor_cam_target_ -= right * md.x * pan;
            editor_cam_target_ += up    * md.y * pan;
        }
        if (scroll != 0.0f) {
            editor_cam_distance_ *= std::pow(0.9f, scroll);
            if (editor_cam_distance_ < 0.1f)    editor_cam_distance_ = 0.1f;
            if (editor_cam_distance_ > 1000.0f) editor_cam_distance_ = 1000.0f;
        }

        // F — frame the selected entity (Unity / Blender shortcut).  Reads the
        // entity's world-space position from its Transform3DComponent so the
        // editor camera orbits around the actual on-screen pivot, not the local
        // position which can be off when parented.
        if (Input::key_pressed(Key::F) && hierarchy_ && hierarchy_->has_selection() &&
            scene_) {
            Entity sel = static_cast<Entity>(hierarchy_->selected_entity());
            auto& reg = scene_->registry();
            if (reg.alive(sel) &&
                reg.has_component<Transform3DComponent>(sel)) {
                const auto& tc = reg.get_component<Transform3DComponent>(sel);
                focus_on(Vec3(tc.world_matrix[3]), 6.0f);
            }
        }

        // Number keys 1/2/3 swap gizmo mode like Unity.
        if (Input::key_pressed(Key::Num1)) gizmo_mode_ = GizmoMode::Translate;
        if (Input::key_pressed(Key::Num2)) gizmo_mode_ = GizmoMode::Rotate;
        if (Input::key_pressed(Key::Num3)) gizmo_mode_ = GizmoMode::Scale;
    }

    // Tiny camera-mode badge top-left, doesn't intrude on the picture.
    ImGui::SetCursorPos(ImVec2(8.0f, 48.0f));
    ImGui::TextColored(ImVec4(0.7f, 0.85f, 1.0f, 0.9f), "%s",
                       camera_mode_ == ViewportCameraMode::SceneView
                           ? "Scene"
                           : "Game");

    ImGui::End();
    ImGui::PopStyleVar();
}

// ── HierarchyPanel ─────────────────────────────────────────────────────────

namespace {

/// Build a new entity from a primitive preset so the Create menu can drop a
/// cube/sphere/plane/light/camera with sensible defaults.  Keeping the
/// factory here rather than in the Hierarchy class lets the panel stay free
/// of engine-internal mesh handles.
Entity spawn_primitive(Scene& scene, const char* kind,
                       u32 cube_id, u32 plane_id, u32 sphere_id,
                       Entity parent = INVALID_ENTITY) {
    Entity e = INVALID_ENTITY;
    auto& reg = scene.registry();
    if (std::strcmp(kind, "Empty") == 0) {
        e = scene.create_entity("Empty");
    } else if (std::strcmp(kind, "Cube") == 0) {
        e = scene.create_entity_3d("Cube");
        auto& mr = reg.add_component<MeshRendererComponent>(e, MeshRendererComponent{});
        mr.mesh_id = cube_id;
        mr.tint    = Vec4(0.8f, 0.8f, 0.85f, 1.0f);
    } else if (std::strcmp(kind, "Plane") == 0) {
        e = scene.create_entity_3d("Plane");
        auto& mr = reg.add_component<MeshRendererComponent>(e, MeshRendererComponent{});
        mr.mesh_id = plane_id;
        mr.tint    = Vec4(0.5f, 0.5f, 0.55f, 1.0f);
    } else if (std::strcmp(kind, "Sphere") == 0) {
        e = scene.create_entity_3d("Sphere");
        auto& mr = reg.add_component<MeshRendererComponent>(e, MeshRendererComponent{});
        mr.mesh_id = sphere_id;
        mr.tint    = Vec4(0.7f, 0.8f, 0.9f, 1.0f);
    } else if (std::strcmp(kind, "Camera") == 0) {
        e = scene.create_entity_3d("Camera");
        auto& cc = reg.add_component<CameraComponent>(e, CameraComponent{});
        cc.is_orthographic = false;
        cc.fov             = 60.0f;
        cc.near_clip       = 0.1f;
        cc.far_clip        = 1000.0f;
    } else if (std::strcmp(kind, "Directional Light") == 0) {
        e = scene.create_entity_3d("Directional Light");
        reg.add_component<DirectionalLightComponent>(e, DirectionalLightComponent{});
    } else if (std::strcmp(kind, "Point Light") == 0) {
        e = scene.create_entity_3d("Point Light");
        reg.add_component<PointLightComponent>(e, PointLightComponent{});
    } else if (std::strcmp(kind, "Spot Light") == 0) {
        e = scene.create_entity_3d("Spot Light");
        reg.add_component<SpotLightComponent>(e, SpotLightComponent{});
    } else if (std::strcmp(kind, "Sprite") == 0) {
        e = scene.create_entity_2d("Sprite");
        reg.add_component<SpriteRendererComponent>(e, SpriteRendererComponent{});
    }
    if (e != INVALID_ENTITY && parent != INVALID_ENTITY) {
        Hierarchy::set_parent(reg, e, parent);
    }
    return e;
}

} // namespace (anonymous)

void HierarchyPanel::on_render() {
    namespace ui = nexus::editor::imgui;

    ui::begin_window("Hierarchy");

    if (!scene_) {
        ui::text("No scene loaded");
        ui::end_window();
        return;
    }

    auto& registry = scene_->registry();

    // F2 starts inline rename of the currently-selected entity (only when
    // the hierarchy window is the focused one — otherwise typing F2 in the
    // viewport would unexpectedly steal focus).  `IsWindowFocused` covers
    // the panel and any of its child windows.
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        !is_renaming_ && has_selection() &&
        ImGui::IsKeyPressed(ImGuiKey_F2, /*repeat*/ false)) {
        begin_rename(selected_entity());
    }

    // ── Toolbar: Create menu + entity count ─────────────────────────────
    if (ImGui::Button("+ Create")) {
        ImGui::OpenPopup("HierarchyCreatePopup");
    }
    if (ImGui::BeginPopup("HierarchyCreatePopup")) {
        const char* items[] = {
            "Empty", "Cube", "Sphere", "Plane",
            "Camera", "Directional Light", "Point Light", "Spot Light",
            "Sprite",
        };
        for (const char* kind : items) {
            if (ImGui::MenuItem(kind)) {
                Entity e = spawn_primitive(*scene_, kind,
                                           primitive_cube_id_,
                                           primitive_plane_id_,
                                           primitive_sphere_id_);
                if (e != INVALID_ENTITY) {
                    selected_ = static_cast<u32>(e);
                    has_selection_ = true;
                }
            }
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::TextDisabled("%zu entities", registry.size());

    // ── Search filter ───────────────────────────────────────────────────
    std::string filter_buf = filter_;
    if (ui::input_text("##Filter", filter_buf)) {
        filter_ = filter_buf;
    }
    ui::separator();

    // Collect root entities (no parent or no HierarchyComponent)
    std::vector<Entity> roots;
    registry.each<TagComponent>([&](Entity e, TagComponent& tag) {
        // Filter by name if filter is set
        if (!filter_.empty()) {
            if (tag.name.find(filter_) == std::string::npos) return;
        }
        // Check if root (no parent)
        if (!registry.has_component<HierarchyComponent>(e) ||
            registry.get_component<HierarchyComponent>(e).parent == INVALID_ENTITY) {
            roots.push_back(e);
        }
    });

    // Render entity tree recursively
    std::function<void(Entity)> render_entity = [&](Entity entity) {
        if (!registry.alive(entity)) return;

        const char* name = "Entity";
        if (registry.has_component<TagComponent>(entity)) {
            name = registry.get_component<TagComponent>(entity).name.c_str();
        }

        auto children = Hierarchy::get_children(registry, entity);
        bool is_leaf = children.empty();
        bool is_selected = has_selection_ && selected_ == entity;

        // Unity-style visibility eye column: flips ActiveComponent.active, auto-
        // adding the component on first toggle. Inactive entities (self or any
        // ancestor) render dimmed to match Unity's Scene Visibility behavior.
        const bool self_active = registry.is_active(entity);
        bool ancestor_active = true;
        if (registry.has_component<HierarchyComponent>(entity)) {
            Entity p = registry.get_component<HierarchyComponent>(entity).parent;
            while (p != INVALID_ENTITY && registry.alive(p)) {
                if (!registry.is_active(p)) { ancestor_active = false; break; }
                if (registry.has_component<HierarchyComponent>(p)) {
                    p = registry.get_component<HierarchyComponent>(p).parent;
                } else {
                    break;
                }
            }
        }
        const bool effectively_visible = self_active && ancestor_active;

        ImGui::PushID(static_cast<int>(entity));
        const char* eye_glyph = self_active ? "O" : "-";
        ImVec4 eye_color = self_active ? ImVec4(0.85f, 0.85f, 0.85f, 1.0f)
                                       : ImVec4(0.45f, 0.45f, 0.45f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, eye_color);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.08f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1, 1, 1, 0.16f));
        if (ImGui::SmallButton(eye_glyph)) {
            // Wrap in an undo command so Ctrl+Z reverts visibility toggles
            // alongside transform/property edits — Unity behavior.
            const u32 ent_id = static_cast<u32>(entity);
            const bool prev = self_active;
            const bool next = !self_active;
            if (undo_mgr_) {
                Scene* sc = scene_;
                undo_mgr_->execute(std::make_unique<LambdaCommand>(
                    next ? "Show entity" : "Hide entity",
                    [sc, ent_id, next]() {
                        if (!sc) return;
                        auto& r = sc->registry();
                        if (r.alive(static_cast<Entity>(ent_id))) {
                            r.set_active(static_cast<Entity>(ent_id), next);
                        }
                    },
                    [sc, ent_id, prev]() {
                        if (!sc) return;
                        auto& r = sc->registry();
                        if (r.alive(static_cast<Entity>(ent_id))) {
                            r.set_active(static_cast<Entity>(ent_id), prev);
                        }
                    }));
            } else {
                registry.set_active(entity, next);
            }
        }
        ImGui::PopStyleColor(4);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(self_active ? "Toggle visibility (active)"
                                          : "Toggle visibility (disabled)");
        }
        ImGui::SameLine();

        // Lock column — Unity's SceneVis lock icon.  Clicking adds or removes
        // a LockedComponent marker.  Locked entities render greyed, ignore
        // gizmo edits, and the Inspector disables all edits on them.  Using
        // add/remove of a marker (no bool payload) keeps the serializer trivial.
        const bool is_locked = registry.has_component<LockedComponent>(entity);
        const char* lock_glyph = is_locked ? "L" : " ";
        ImVec4 lock_color = is_locked ? ImVec4(1.0f, 0.85f, 0.25f, 1.0f)
                                      : ImVec4(0.45f, 0.45f, 0.45f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, lock_color);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.08f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1, 1, 1, 0.16f));
        if (ImGui::SmallButton(lock_glyph)) {
            const u32 ent_id = static_cast<u32>(entity);
            if (undo_mgr_) {
                Scene* sc = scene_;
                undo_mgr_->execute(std::make_unique<LambdaCommand>(
                    is_locked ? "Unlock entity" : "Lock entity",
                    [sc, ent_id, was_locked = is_locked]() {
                        if (!sc) return;
                        auto& r = sc->registry();
                        if (!r.alive(static_cast<Entity>(ent_id))) return;
                        if (was_locked) {
                            r.remove_component<LockedComponent>(
                                static_cast<Entity>(ent_id));
                        } else {
                            r.add_component<LockedComponent>(
                                static_cast<Entity>(ent_id), LockedComponent{});
                        }
                    },
                    [sc, ent_id, was_locked = is_locked]() {
                        if (!sc) return;
                        auto& r = sc->registry();
                        if (!r.alive(static_cast<Entity>(ent_id))) return;
                        if (was_locked) {
                            r.add_component<LockedComponent>(
                                static_cast<Entity>(ent_id), LockedComponent{});
                        } else {
                            r.remove_component<LockedComponent>(
                                static_cast<Entity>(ent_id));
                        }
                    }));
            } else {
                if (is_locked) {
                    registry.remove_component<LockedComponent>(entity);
                } else {
                    registry.add_component<LockedComponent>(entity, LockedComponent{});
                }
            }
        }
        ImGui::PopStyleColor(4);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(is_locked ? "Unlock entity (allow edits)"
                                        : "Lock entity (block edits)");
        }
        ImGui::PopID();
        ImGui::SameLine();

        bool node_open = false;
        const bool this_is_renaming =
            is_renaming_ && renaming_ == static_cast<u32>(entity);

        if (this_is_renaming && registry.has_component<TagComponent>(entity)) {
            // Inline rename: replace the tree row's label with an InputText.
            // We still need the row to participate in tree layout, so we open
            // a leaf-style tree node first then overlay the input on the same
            // line.  Enter or focus-loss commits; Esc cancels.
            const ImGuiTreeNodeFlags tn_flags =
                ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                ImGuiTreeNodeFlags_SpanAvailWidth;
            ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(entity)),
                              tn_flags, "%s", "");
            ImGui::SameLine();
            if (rename_focus_pending_) {
                ImGui::SetKeyboardFocusHere();
                rename_focus_pending_ = false;
            }
            ImGui::SetNextItemWidth(-1.0f);
            const bool committed =
                ImGui::InputText("##rename", rename_buffer_, sizeof(rename_buffer_),
                                 ImGuiInputTextFlags_EnterReturnsTrue |
                                 ImGuiInputTextFlags_AutoSelectAll);
            const bool blurred = ImGui::IsItemDeactivated();
            const bool escaped =
                ImGui::IsKeyPressed(ImGuiKey_Escape, /*repeat*/ false);
            if (committed || (blurred && !escaped)) {
                if (rename_buffer_[0] != '\0') {
                    registry.get_component<TagComponent>(entity).name =
                        rename_buffer_;
                }
                is_renaming_ = false;
            } else if (escaped) {
                is_renaming_ = false;
            }
        } else {
            // Component-typed icon prefix matches Unity's Hierarchy column;
            // gives users a quick visual cue without expanding the row.
            const char* icon = hierarchy_icon(registry, entity);
            char label[256];
            std::snprintf(label, sizeof(label), "%s %s##%u",
                          icon, name, static_cast<u32>(entity));
            const bool dim_label = !effectively_visible;
            if (dim_label) {
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImVec4(0.55f, 0.55f, 0.55f, 1.0f));
            }
            node_open = ui::tree_node(label, is_selected, is_leaf);
            if (dim_label) ImGui::PopStyleColor();

            // Selection — Ctrl-click toggles, Shift-click extends, plain click
            // single-selects.  All paths route through the bound selection so
            // every consumer sees the same state.
            if (ui::is_item_clicked()) {
                const ImGuiIO& io = ImGui::GetIO();
                if (io.KeyCtrl || io.KeySuper) {
                    if (is_multi_selected(entity)) {
                        remove_from_selection(entity);
                    } else {
                        add_to_selection(entity);
                    }
                } else {
                    set_selected_entity(entity);
                }
            }
            if (ImGui::IsItemHovered() &&
                ImGui::IsMouseDoubleClicked(0)) {
                begin_rename(static_cast<u32>(entity));
            }
        }

        // Context menu (right-click)
        if (ui::begin_popup_context_item()) {
            ImGui::TextDisabled("%s", name);
            ImGui::Separator();
            if (ImGui::BeginMenu("Create Child")) {
                const char* kinds[] = {
                    "Empty", "Cube", "Sphere", "Plane",
                    "Camera", "Directional Light", "Point Light", "Spot Light",
                    "Sprite",
                };
                for (const char* k : kinds) {
                    if (ImGui::MenuItem(k)) {
                        Entity child = spawn_primitive(
                            *scene_, k,
                            primitive_cube_id_, primitive_plane_id_,
                            primitive_sphere_id_, entity);
                        if (child != INVALID_ENTITY) {
                            set_selected_entity(static_cast<u32>(child));
                        }
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Rename", "F2")) {
                begin_rename(static_cast<u32>(entity));
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete")) {
                registry.destroy(entity);
                if (selected_entity() == entity) clear_selection();
            }
            ui::end_popup();
        }

        // Drag-drop reparenting (entity → entity).
        if (ui::begin_drag_source()) {
            ui::set_drag_payload("ENTITY", &entity, sizeof(Entity));
            ui::text("%s", name);
            ui::end_drag_source();
        }
        if (ImGui::BeginDragDropTarget()) {
            // Existing entity-on-entity reparent payload.
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ENTITY")) {
                Entity dragged = *static_cast<const Entity*>(p->Data);
                if (dragged != entity &&
                    !Hierarchy::is_ancestor(registry, entity, dragged)) {
                    Hierarchy::set_parent(registry, dragged, entity);
                }
            }
            // Cross-panel asset drop — host decides what to do based on extension.
            if (const ImGuiPayload* p =
                    ImGui::AcceptDragDropPayload("NEXUS_ASSET_PATH")) {
                if (on_asset_drop_) {
                    const char* path = static_cast<const char*>(p->Data);
                    on_asset_drop_(std::string(path), static_cast<u32>(entity));
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (node_open && !is_leaf) {
            for (Entity child : children) {
                render_entity(child);
            }
            ui::tree_pop();
        }
    };

    for (Entity root : roots) {
        render_entity(root);
    }

    ui::end_window();
}

void HierarchyPanel::begin_rename(u32 entity) {
    if (!scene_) return;
    auto& reg = scene_->registry();
    if (!reg.alive(static_cast<Entity>(entity))) return;
    if (!reg.has_component<TagComponent>(static_cast<Entity>(entity))) return;
    const std::string& cur = reg.get_component<TagComponent>(
        static_cast<Entity>(entity)).name;
    std::snprintf(rename_buffer_, sizeof(rename_buffer_), "%s", cur.c_str());
    renaming_ = entity;
    is_renaming_ = true;
    rename_focus_pending_ = true;
}

// ── Selection: routes to bound EditorSelection if any, else local fallback ──

void HierarchyPanel::set_selected_entity(u32 entity) {
    if (ext_selection_) {
        ext_selection_->clear();
        ext_selection_->select(entity);
    } else {
        selected_ = entity;
        has_selection_ = true;
        multi_selection_.clear();
        multi_selection_.push_back(entity);
    }
}

void HierarchyPanel::clear_selection() {
    if (ext_selection_) {
        ext_selection_->clear();
    } else {
        has_selection_ = false;
        selected_ = 0;
        multi_selection_.clear();
    }
}

bool HierarchyPanel::has_selection() const {
    return ext_selection_ ? ext_selection_->has_selection() : has_selection_;
}

u32 HierarchyPanel::selected_entity() const {
    return ext_selection_ ? ext_selection_->primary() : selected_;
}

void HierarchyPanel::add_to_selection(u32 entity) {
    if (ext_selection_) {
        ext_selection_->select(entity);
    } else if (!is_multi_selected(entity)) {
        multi_selection_.push_back(entity);
    }
}

void HierarchyPanel::remove_from_selection(u32 entity) {
    if (ext_selection_) {
        ext_selection_->deselect(entity);
    } else {
        multi_selection_.erase(
            std::remove(multi_selection_.begin(), multi_selection_.end(), entity),
            multi_selection_.end());
    }
}

std::vector<u32> HierarchyPanel::multi_selection() const {
    return ext_selection_ ? ext_selection_->all() : multi_selection_;
}

void HierarchyPanel::clear_multi_selection() {
    if (ext_selection_) ext_selection_->clear();
    else                multi_selection_.clear();
}

bool HierarchyPanel::is_multi_selected(u32 entity) const {
    if (ext_selection_) return ext_selection_->is_selected(entity);
    return std::find(multi_selection_.begin(), multi_selection_.end(), entity)
           != multi_selection_.end();
}

// ── InspectorPanel ──────────────────────────────────────────────────────────

namespace {

/// Multi-select uniformity probe.  Reads the same scalar slot from every
/// entity in `targets` and returns true when all values match the primary.
/// `read` returns nullptr when the entity is missing the component, which
/// counts as "non-uniform" (the primary has a value but others lack the
/// component entirely — equivalent to differing).
template <typename T, typename ReadFn>
bool field_is_uniform(const std::vector<u32>& targets, ReadFn&& read) {
    if (targets.size() < 2) return true;
    const T* first = read(targets[0]);
    if (!first) return false;
    for (size_t i = 1; i < targets.size(); ++i) {
        const T* v = read(targets[i]);
        if (!v) return false;
        if (!(*v == *first)) return false;
    }
    return true;
}

/// Convenience wrapper: tests a component-member field (resolved by
/// has_component<C> + member-pointer) for uniformity across selection.
/// Returns true if all targets either lack the component or share the value.
/// Hides the boilerplate `nullptr → non-uniform` plumbing the call sites
/// were repeating.
template <typename C, typename FieldT, typename Scene>
bool component_field_uniform(Scene* scene,
                             const std::vector<u32>& targets,
                             FieldT C::*member) {
    return field_is_uniform<FieldT>(
        targets,
        [scene, member](u32 e) -> const FieldT* {
            if (!scene) return nullptr;
            auto& r = scene->registry();
            if (!r.alive(static_cast<Entity>(e))) return nullptr;
            if (!r.template has_component<C>(static_cast<Entity>(e))) return nullptr;
            return &(r.template get_component<C>(static_cast<Entity>(e)).*member);
        });
}

/// Drag-edit undo helper.  Wraps any IsItemActivated/IsItemDeactivatedAfterEdit
/// widget so:
///   • on drag-begin we capture the value into the panel's drag cache,
///   • on drag-end (release / Tab away) we push exactly one LambdaCommand
///     containing the before→after delta — Unity-style "one undo per drag".
///
/// `widget_call` is the actual ImGui widget invocation; it must return whether
/// the value changed THIS frame (DragFloatN/ColorEditN/InputFloat all do).
/// `apply` is given as a callback so the caller can decompose Vec3/Quat as
/// needed.  The undo command captures the entity id and looks up the component
/// fresh on each undo/redo, which keeps it robust to scene snapshot/restore.
template <typename WidgetFn, typename ApplyFn>
bool float_n_widget_undo(UndoRedoManager* mgr,
                          const std::vector<u32>& targets,
                          const char* property,
                          int n,
                          f32* values,
                          u64& widget_id_slot,
                          f32 (&begin_slot)[4],
                          std::vector<u32>& begin_targets_slot,
                          bool mixed,
                          WidgetFn&& widget_call,
                          ApplyFn&& apply) {
    if (mixed) {
        // Visual cue mirroring Unity's "—" indicator for fields whose value
        // differs across the multi-selection.  We render the input in a
        // muted style; the user typing/dragging commits the new value to
        // every selected entity.
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.35f, 0.30f, 0.10f, 0.6f));
    }
    const bool changed = widget_call();
    if (mixed) ImGui::PopStyleColor();
    if (mixed) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.25f, 1.0f), "(mixed)");
    }
    const u64 id = static_cast<u64>(ImGui::GetItemID());
    if (ImGui::IsItemActivated()) {
        widget_id_slot = id;
        begin_targets_slot = targets;
        for (int i = 0; i < n; ++i) begin_slot[i] = values[i];
    }
    if (mgr && ImGui::IsItemDeactivatedAfterEdit() &&
        widget_id_slot == id) {
        f32 before[4] = {0, 0, 0, 0};
        f32 after[4]  = {0, 0, 0, 0};
        for (int i = 0; i < n; ++i) {
            before[i] = begin_slot[i];
            after[i]  = values[i];
        }
        bool any_diff = false;
        for (int i = 0; i < n; ++i) {
            if (before[i] != after[i]) { any_diff = true; break; }
        }
        if (any_diff && !begin_targets_slot.empty()) {
            const std::vector<u32> ents = begin_targets_slot;
            std::string desc = std::string("Edit ") + property;
            if (ents.size() > 1) {
                desc += " (" + std::to_string(ents.size()) + " selected)";
            }
            ApplyFn cb = std::forward<ApplyFn>(apply);
            mgr->execute(std::make_unique<LambdaCommand>(
                std::move(desc),
                /*do*/   [ents, after,  cb]() mutable {
                    for (u32 e : ents) cb(e, after);
                },
                /*undo*/ [ents, before, cb]() mutable {
                    for (u32 e : ents) cb(e, before);
                }));
        }
        widget_id_slot = 0;
    }
    return changed;
}

/// DragFloatN with one-undo-per-drag semantics.  `targets` is the entity set
/// the edit broadcasts to — single-entity callers pass {target_}, multi-select
/// callers pass the bound EditorSelection's list.
template <typename ApplyFn>
bool drag_float_n_undo(InspectorPanel&,
                       UndoRedoManager* mgr,
                       const std::vector<u32>& targets,
                       const char* label,
                       const char* property,
                       int n,
                       f32* values,
                       f32 speed,
                       f32 vmin,
                       f32 vmax,
                       u64& widget_id_slot,
                       f32 (&begin_slot)[4],
                       std::vector<u32>& begin_targets_slot,
                       ApplyFn&& apply,
                       bool mixed = false) {
    return float_n_widget_undo(
        mgr, targets, property, n, values,
        widget_id_slot, begin_slot, begin_targets_slot, mixed,
        [&]() {
            switch (n) {
                case 1: return ImGui::DragFloat (label, values, speed, vmin, vmax);
                case 2: return ImGui::DragFloat2(label, values, speed, vmin, vmax);
                case 3: return ImGui::DragFloat3(label, values, speed, vmin, vmax);
                case 4: return ImGui::DragFloat4(label, values, speed, vmin, vmax);
                default: return false;
            }
        },
        std::forward<ApplyFn>(apply));
}

template <typename ApplyFn>
bool color_edit_n_undo(InspectorPanel&,
                       UndoRedoManager* mgr,
                       const std::vector<u32>& targets,
                       const char* label,
                       const char* property,
                       int n,
                       f32* values,
                       u64& widget_id_slot,
                       f32 (&begin_slot)[4],
                       std::vector<u32>& begin_targets_slot,
                       ApplyFn&& apply,
                       bool mixed = false) {
    return float_n_widget_undo(
        mgr, targets, property, n, values,
        widget_id_slot, begin_slot, begin_targets_slot, mixed,
        [&]() {
            return n == 3 ? ImGui::ColorEdit3(label, values)
                          : ImGui::ColorEdit4(label, values);
        },
        std::forward<ApplyFn>(apply));
}

/// Atomic checkbox with undo + multi-target broadcast.  Bool flips on click —
/// no drag begin/end — so we compare prev/new in one shot.
template <typename ApplyFn>
bool checkbox_undo(UndoRedoManager* mgr,
                   const std::vector<u32>& targets,
                   const char* label,
                   const char* property,
                   bool* value,
                   ApplyFn&& apply) {
    const bool prev = *value;
    const bool changed = ImGui::Checkbox(label, value);
    if (mgr && changed && prev != *value && !targets.empty()) {
        const bool before = prev;
        const bool after  = *value;
        const std::vector<u32> ents = targets;
        std::string desc = std::string("Toggle ") + property;
        if (ents.size() > 1) {
            desc += " (" + std::to_string(ents.size()) + " selected)";
        }
        ApplyFn cb = std::forward<ApplyFn>(apply);
        mgr->execute(std::make_unique<LambdaCommand>(
            std::move(desc),
            [ents, after,  cb]() mutable { for (u32 e : ents) cb(e, after);  },
            [ents, before, cb]() mutable { for (u32 e : ents) cb(e, before); }));
    }
    return changed;
}

/// Atomic combo with undo + multi-target broadcast.  Combo selection is
/// atomic (ImGui returns true on the change frame); no activation tracking
/// needed.  Apply lambda receives the selected index.
template <typename ApplyFn>
bool combo_undo(UndoRedoManager* mgr,
                const std::vector<u32>& targets,
                const char* label,
                const char* property,
                i32* current_index,
                const char* const items[],
                int item_count,
                ApplyFn&& apply) {
    const i32 prev = *current_index;
    const bool changed = ImGui::Combo(label, current_index, items, item_count);
    if (mgr && changed && prev != *current_index && !targets.empty()) {
        const i32 before = prev;
        const i32 after  = *current_index;
        const std::vector<u32> ents = targets;
        std::string desc = std::string("Set ") + property;
        if (ents.size() > 1) {
            desc += " (" + std::to_string(ents.size()) + " selected)";
        }
        ApplyFn cb = std::forward<ApplyFn>(apply);
        mgr->execute(std::make_unique<LambdaCommand>(
            std::move(desc),
            [ents, after,  cb]() mutable { for (u32 e : ents) cb(e, after);  },
            [ents, before, cb]() mutable { for (u32 e : ents) cb(e, before); }));
    }
    return changed;
}

/// InputInt with begin/end activation tracking + multi-target broadcast.
template <typename ApplyFn>
bool input_int_undo(UndoRedoManager* mgr,
                    const std::vector<u32>& targets,
                    const char* label,
                    const char* property,
                    i32* value,
                    u64& widget_id_slot,
                    i32& begin_slot,
                    std::vector<u32>& begin_targets_slot,
                    ApplyFn&& apply) {
    const bool changed = ImGui::InputInt(label, value);
    const u64 id = static_cast<u64>(ImGui::GetItemID());
    if (ImGui::IsItemActivated()) {
        widget_id_slot = id;
        begin_targets_slot = targets;
        begin_slot = *value;
    }
    if (mgr && ImGui::IsItemDeactivatedAfterEdit() &&
        widget_id_slot == id) {
        const i32 before = begin_slot;
        const i32 after  = *value;
        if (before != after && !begin_targets_slot.empty()) {
            const std::vector<u32> ents = begin_targets_slot;
            std::string desc = std::string("Edit ") + property;
            if (ents.size() > 1) {
                desc += " (" + std::to_string(ents.size()) + " selected)";
            }
            ApplyFn cb = std::forward<ApplyFn>(apply);
            mgr->execute(std::make_unique<LambdaCommand>(
                std::move(desc),
                [ents, after,  cb]() mutable { for (u32 e : ents) cb(e, after);  },
                [ents, before, cb]() mutable { for (u32 e : ents) cb(e, before); }));
        }
        widget_id_slot = 0;
    }
    return changed;
}

/// Draws a collapsing section whose header carries a right-aligned "⋮" context
/// menu matching Unity's per-component affordance.  The popup currently exposes
/// Reset (overwrite with a default-constructed T) and Remove Component.
///
/// Returning true means the section body should be rendered; returning false
/// means either collapsed OR the component was just removed (caller must
/// re-check has_component).
template <typename T>
bool component_header(Registry& reg, Entity entity, const char* label,
                      bool* removed_out = nullptr) {
    bool open = ImGui::CollapsingHeader(
        label,
        ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);

    // Right-align a compact "⋮" context button on the same row as the header.
    // The ASCII ":" glyph is used because the default ImGui font ships without
    // the full Unicode vertical-ellipsis — this keeps rendering stable across
    // backends while still communicating "more actions".
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 14.0f);
    ImGui::PushID(label);
    const char* popup_id = "ComponentContextMenu";
    if (ImGui::SmallButton(":")) {
        ImGui::OpenPopup(popup_id);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("More options");
    }

    bool component_removed = false;
    if (ImGui::BeginPopup(popup_id)) {
        ImGui::TextDisabled("%s", label);
        ImGui::Separator();
        if (ImGui::MenuItem("Reset")) {
            if (reg.has_component<T>(entity)) {
                reg.get_component<T>(entity) = T{};
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Remove Component")) {
            reg.remove_component<T>(entity);
            component_removed = true;
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();

    if (component_removed) {
        if (removed_out) *removed_out = true;
        return false;
    }
    return open;
}

} // namespace (anonymous)

void InspectorPanel::on_render() {
    namespace ui = nexus::editor::imgui;

    ui::begin_window("Inspector");

    if (!has_target_ || !scene_) {
        ui::text("No entity selected");
        ui::end_window();
        return;
    }

    auto& registry = scene_->registry();
    Entity target = static_cast<Entity>(target_);

    if (!registry.alive(target)) {
        ui::text("Entity no longer valid");
        has_target_ = false;
        ui::end_window();
        return;
    }

    // Multi-select banner — Unity-style indicator that edits broadcast.
    if (ext_selection_ && ext_selection_->count() > 1 &&
        ext_selection_->is_selected(target_)) {
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.25f, 1.0f),
                           "[Multi-select: %u entities — edits apply to all]",
                           ext_selection_->count());
        ImGui::Separator();
    }

    // If the entity is locked (directly or by an ancestor), push a global
    // disabled scope so every widget below is greyed out and unresponsive.
    const bool entity_locked = is_entity_effectively_locked(registry, target);
    if (entity_locked) ImGui::BeginDisabled(true);

    // ── Entity header: Active toggle · Lock · Entity id ──────────────────
    if (registry.has_component<ActiveComponent>(target)) {
        auto& ac = registry.get_component<ActiveComponent>(target);
        ImGui::Checkbox("##Active", &ac.active);
        ImGui::SameLine();
    }
    if (registry.has_component<TagComponent>(target)) {
        auto& tag = registry.get_component<TagComponent>(target);
        char name_buf[128];
        std::strncpy(name_buf, tag.name.c_str(), sizeof(name_buf) - 1);
        name_buf[sizeof(name_buf) - 1] = '\0';
        ImGui::SetNextItemWidth(-130.0f);
        if (ImGui::InputText("##EntityName", name_buf, sizeof(name_buf))) {
            PropertyEdit edit{"Tag", "name", tag.name, name_buf};
            tag.name = name_buf;
            push_edit(edit);
        }
        ImGui::SameLine();
        ImGui::Checkbox("Lock", &locked_);
        ImGui::SameLine();
        ImGui::TextDisabled("#%u", target_);

        // ── Tag / Layer row (Unity Inspector parity) ─────────────────
        // Two side-by-side combos: a category Tag (Unity's "Untagged",
        // "Player", "MainCamera", etc.) and a numeric Layer index.  Both
        // round-trip through TagComponent's new fields and persist via the
        // scene serializer.  Editable so users can introduce custom tags.
        const char* kBuiltinTags[] = {
            "Untagged", "Respawn", "Finish", "EditorOnly",
            "MainCamera", "Player", "GameController",
        };
        char tag_buf[64];
        std::snprintf(tag_buf, sizeof(tag_buf), "%s", tag.category.c_str());
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::BeginCombo("Tag", tag_buf)) {
            for (const char* t : kBuiltinTags) {
                bool sel = (tag.category == t);
                if (ImGui::Selectable(t, sel)) tag.category = t;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::Separator();
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##custom_tag", tag_buf, sizeof(tag_buf),
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
                tag.category = tag_buf;
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputInt("Layer", &tag.layer);
        if (tag.layer < 0)  tag.layer = 0;
        if (tag.layer > 31) tag.layer = 31;
    } else {
        ImGui::Text("(no TagComponent)");
        ImGui::SameLine();
        ImGui::Checkbox("Lock", &locked_);
        ImGui::SameLine();
        ImGui::TextDisabled("#%u", target_);
    }
    ui::separator();

    // ── Transform3DComponent ─────────────────────────────────────────────
    if (registry.has_component<Transform3DComponent>(target)) {
        if (component_header<Transform3DComponent>(registry, target, "Transform 3D")) {
            auto& t = registry.get_component<Transform3DComponent>(target);
            const auto edit_targets = current_edit_targets();
            Scene* sc = scene_;

            // Mixed-value probes — one-line via the component_field_uniform
            // helper.  Returns false when targets disagree (or some lack the
            // component); that drives the yellow "(mixed)" affordance.
            const bool pos_mixed = !component_field_uniform<Transform3DComponent, Vec3>(
                sc, edit_targets, &Transform3DComponent::position);
            const bool scl_mixed = !component_field_uniform<Transform3DComponent, Vec3>(
                sc, edit_targets, &Transform3DComponent::scale);
            const bool rot_mixed = !component_field_uniform<Transform3DComponent, Quat>(
                sc, edit_targets, &Transform3DComponent::rotation);

            f32 pos[4] = {t.position.x, t.position.y, t.position.z, 0};
            drag_float_n_undo(*this, undo_mgr_, edit_targets, "Position", "position",
                              3, pos, 0.05f, 0.0f, 0.0f,
                              drag_widget_id_, drag_begin_f_, drag_begin_targets_,
                              [scene = scene_](u32 ent, const f32* v) {
                                  if (!scene) return;
                                  auto& reg = scene->registry();
                                  if (!reg.alive(static_cast<Entity>(ent))) return;
                                  if (!reg.has_component<Transform3DComponent>(
                                          static_cast<Entity>(ent))) return;
                                  reg.get_component<Transform3DComponent>(
                                      static_cast<Entity>(ent)).position =
                                      Vec3(v[0], v[1], v[2]);
                              }, pos_mixed);
            t.position = Vec3(pos[0], pos[1], pos[2]);

            Vec3 euler_v = glm::degrees(glm::eulerAngles(t.rotation));
            f32 euler[4] = {euler_v.x, euler_v.y, euler_v.z, 0};
            drag_float_n_undo(*this, undo_mgr_, edit_targets, "Rotation", "rotation",
                              3, euler, 0.5f, 0.0f, 0.0f,
                              drag_widget_id_, drag_begin_f_, drag_begin_targets_,
                              [scene = scene_](u32 ent, const f32* v) {
                                  if (!scene) return;
                                  auto& reg = scene->registry();
                                  if (!reg.alive(static_cast<Entity>(ent))) return;
                                  if (!reg.has_component<Transform3DComponent>(
                                          static_cast<Entity>(ent))) return;
                                  reg.get_component<Transform3DComponent>(
                                      static_cast<Entity>(ent)).rotation =
                                      Quat(glm::radians(Vec3(v[0], v[1], v[2])));
                              }, rot_mixed);
            t.rotation = Quat(glm::radians(Vec3(euler[0], euler[1], euler[2])));

            f32 scl[4] = {t.scale.x, t.scale.y, t.scale.z, 0};
            drag_float_n_undo(*this, undo_mgr_, edit_targets, "Scale", "scale",
                              3, scl, 0.01f, 0.001f, 1000.0f,
                              drag_widget_id_, drag_begin_f_, drag_begin_targets_,
                              [scene = scene_](u32 ent, const f32* v) {
                                  if (!scene) return;
                                  auto& reg = scene->registry();
                                  if (!reg.alive(static_cast<Entity>(ent))) return;
                                  if (!reg.has_component<Transform3DComponent>(
                                          static_cast<Entity>(ent))) return;
                                  reg.get_component<Transform3DComponent>(
                                      static_cast<Entity>(ent)).scale =
                                      Vec3(v[0], v[1], v[2]);
                              }, scl_mixed);
            t.scale = Vec3(scl[0], scl[1], scl[2]);
        }
    }

    // ── Transform2DComponent ─────────────────────────────────────────────
    if (registry.has_component<Transform2DComponent>(target)) {
        if (component_header<Transform2DComponent>(registry, target, "Transform 2D")) {
            auto& t = registry.get_component<Transform2DComponent>(target);

            f32 pos[4] = {t.position.x, t.position.y, 0, 0};
            drag_float_n_undo(*this, undo_mgr_, current_edit_targets(), "Position", "position2d",
                              2, pos, 0.05f, 0.0f, 0.0f,
                              drag_widget_id_, drag_begin_f_, drag_begin_targets_,
                              [scene = scene_](u32 ent, const f32* v) {
                                  if (!scene) return;
                                  auto& reg = scene->registry();
                                  if (!reg.alive(static_cast<Entity>(ent))) return;
                                  if (!reg.has_component<Transform2DComponent>(
                                          static_cast<Entity>(ent))) return;
                                  reg.get_component<Transform2DComponent>(
                                      static_cast<Entity>(ent)).position =
                                      Vec2(v[0], v[1]);
                              });
            t.position = Vec2(pos[0], pos[1]);

            f32 rot[4] = {glm::degrees(t.rotation), 0, 0, 0};
            drag_float_n_undo(*this, undo_mgr_, current_edit_targets(), "Rotation", "rotation2d",
                              1, rot, 0.5f, 0.0f, 0.0f,
                              drag_widget_id_, drag_begin_f_, drag_begin_targets_,
                              [scene = scene_](u32 ent, const f32* v) {
                                  if (!scene) return;
                                  auto& reg = scene->registry();
                                  if (!reg.alive(static_cast<Entity>(ent))) return;
                                  if (!reg.has_component<Transform2DComponent>(
                                          static_cast<Entity>(ent))) return;
                                  reg.get_component<Transform2DComponent>(
                                      static_cast<Entity>(ent)).rotation =
                                      glm::radians(v[0]);
                              });
            t.rotation = glm::radians(rot[0]);

            f32 scl[4] = {t.scale.x, t.scale.y, 0, 0};
            drag_float_n_undo(*this, undo_mgr_, current_edit_targets(), "Scale", "scale2d",
                              2, scl, 0.01f, 0.001f, 1000.0f,
                              drag_widget_id_, drag_begin_f_, drag_begin_targets_,
                              [scene = scene_](u32 ent, const f32* v) {
                                  if (!scene) return;
                                  auto& reg = scene->registry();
                                  if (!reg.alive(static_cast<Entity>(ent))) return;
                                  if (!reg.has_component<Transform2DComponent>(
                                          static_cast<Entity>(ent))) return;
                                  reg.get_component<Transform2DComponent>(
                                      static_cast<Entity>(ent)).scale =
                                      Vec2(v[0], v[1]);
                              });
            t.scale = Vec2(scl[0], scl[1]);
        }
    }

    // ── Mesh Filter (separate header per Unity convention) ───────────────
    // Even though our scene model keeps mesh_id on MeshRendererComponent,
    // splitting this into its own collapsing header matches Unity's UX
    // (Mesh Filter holds the asset reference, Mesh Renderer holds the
    // material + render-pipeline knobs).  The single underlying field is
    // shared between the two sections.
    if (registry.has_component<MeshRendererComponent>(target)) {
        if (component_header<MeshRendererComponent>(registry, target, "Mesh Filter")) {
            auto& mr = registry.get_component<MeshRendererComponent>(target);
            i32 mesh_id_i = static_cast<i32>(mr.mesh_id);
            input_int_undo(undo_mgr_, current_edit_targets(), "Mesh", "mesh_id",
                           &mesh_id_i,
                           drag_widget_id_, drag_begin_i_, drag_begin_targets_,
                           [scene = scene_](u32 ent, i32 v) {
                               if (!scene) return;
                               auto& reg = scene->registry();
                               if (!reg.alive(static_cast<Entity>(ent))) return;
                               if (!reg.has_component<MeshRendererComponent>(
                                       static_cast<Entity>(ent))) return;
                               reg.get_component<MeshRendererComponent>(
                                   static_cast<Entity>(ent)).mesh_id =
                                   static_cast<u32>(std::max(0, v));
                           });
            mr.mesh_id = static_cast<u32>(std::max(0, mesh_id_i));
        }
    }

    // ── MeshRendererComponent ────────────────────────────────────────────
    if (registry.has_component<MeshRendererComponent>(target)) {
        if (component_header<MeshRendererComponent>(registry, target, "Mesh Renderer")) {
            auto& mr = registry.get_component<MeshRendererComponent>(target);
            const auto edit_targets = current_edit_targets();

            // Materials — material id + tint go here per Unity's grouping.
            ImGui::TextDisabled("Materials");
            ImGui::Indent(8.0f);
            i32 mat_id_i = static_cast<i32>(mr.material_id);
            input_int_undo(undo_mgr_, edit_targets, "Element 0", "material_id",
                           &mat_id_i,
                           drag_widget_id_, drag_begin_i_, drag_begin_targets_,
                           [scene = scene_](u32 ent, i32 v) {
                               if (!scene) return;
                               auto& reg = scene->registry();
                               if (!reg.alive(static_cast<Entity>(ent))) return;
                               if (!reg.has_component<MeshRendererComponent>(
                                       static_cast<Entity>(ent))) return;
                               reg.get_component<MeshRendererComponent>(
                                   static_cast<Entity>(ent)).material_id =
                                   static_cast<u32>(std::max(0, v));
                           });
            mr.material_id = static_cast<u32>(std::max(0, mat_id_i));
            // Resolved-path display: shows the material's filename next to
            // the raw id when the resolver is bound and the id is known.
            // "(none)" when id == 0; "<unknown>" when the cache doesn't
            // recognise it (stale scene reference).  Clear button zeroes
            // the id without an extra panel button — Unity convention.
            if (asset_paths_.material) {
                std::string label;
                if (mr.material_id == 0) {
                    label = "(none)";
                } else {
                    std::string p = asset_paths_.material(mr.material_id);
                    label = p.empty()
                        ? "<unknown>"
                        : std::filesystem::path(p).filename().string();
                }
                ImGui::TextDisabled("  → %s", label.c_str());
                if (mr.material_id != 0) {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Clear##mat")) {
                        mr.material_id = 0;
                    }
                }
            }

            f32 tint[4] = {mr.tint.x, mr.tint.y, mr.tint.z, mr.tint.w};
            const bool tint_mixed = !component_field_uniform<
                MeshRendererComponent, Vec4>(
                scene_, edit_targets, &MeshRendererComponent::tint);
            color_edit_n_undo(*this, undo_mgr_, edit_targets, "Tint", "tint",
                              4, tint,
                              drag_widget_id_, drag_begin_f_, drag_begin_targets_,
                              [scene = scene_](u32 ent, const f32* v) {
                                  if (!scene) return;
                                  auto& reg = scene->registry();
                                  if (!reg.alive(static_cast<Entity>(ent))) return;
                                  if (!reg.has_component<MeshRendererComponent>(
                                          static_cast<Entity>(ent))) return;
                                  reg.get_component<MeshRendererComponent>(
                                      static_cast<Entity>(ent)).tint =
                                      Vec4(v[0], v[1], v[2], v[3]);
                              }, tint_mixed);
            mr.tint = Vec4(tint[0], tint[1], tint[2], tint[3]);
            ImGui::Unindent(8.0f);
            ImGui::Spacing();

            // Lighting — cast/receive shadows, toggled atomically, undoable.
            ImGui::TextDisabled("Lighting");
            ImGui::Indent(8.0f);
            checkbox_undo(undo_mgr_, edit_targets, "Cast Shadows", "cast_shadows",
                          &mr.cast_shadows,
                          [scene = scene_](u32 ent, bool v) {
                              if (!scene) return;
                              auto& reg = scene->registry();
                              if (!reg.alive(static_cast<Entity>(ent))) return;
                              if (!reg.has_component<MeshRendererComponent>(
                                      static_cast<Entity>(ent))) return;
                              reg.get_component<MeshRendererComponent>(
                                  static_cast<Entity>(ent)).cast_shadows = v;
                          });
            checkbox_undo(undo_mgr_, edit_targets, "Receive Shadows", "recv_shadows",
                          &mr.receive_shadows,
                          [scene = scene_](u32 ent, bool v) {
                              if (!scene) return;
                              auto& reg = scene->registry();
                              if (!reg.alive(static_cast<Entity>(ent))) return;
                              if (!reg.has_component<MeshRendererComponent>(
                                      static_cast<Entity>(ent))) return;
                              reg.get_component<MeshRendererComponent>(
                                  static_cast<Entity>(ent)).receive_shadows = v;
                          });
            ImGui::Unindent(8.0f);
            ImGui::Spacing();

            // Probes — light + reflection probe combo modes.
            ImGui::TextDisabled("Probes");
            ImGui::Indent(8.0f);
            const char* light_probe_modes[] = {
                "Off", "Blend Probes", "Use Proxy Volume", "Custom",
            };
            i32 lp_i = static_cast<i32>(mr.light_probes);
            if (combo_undo(undo_mgr_, edit_targets, "Light Probes", "light_probes",
                           &lp_i, light_probe_modes, IM_ARRAYSIZE(light_probe_modes),
                           [scene = scene_](u32 ent, i32 v) {
                               if (!scene) return;
                               auto& reg = scene->registry();
                               if (!reg.alive(static_cast<Entity>(ent))) return;
                               if (!reg.has_component<MeshRendererComponent>(
                                       static_cast<Entity>(ent))) return;
                               reg.get_component<MeshRendererComponent>(
                                   static_cast<Entity>(ent)).light_probes =
                                   static_cast<LightProbesMode>(v);
                           })) {
                mr.light_probes = static_cast<LightProbesMode>(lp_i);
            }
            const char* refl_probe_modes[] = {
                "Off", "Blend Probes", "Blend Probes And Skybox", "Simple",
            };
            i32 rp_i = static_cast<i32>(mr.reflection_probes);
            if (combo_undo(undo_mgr_, edit_targets, "Reflection Probes", "refl_probes",
                           &rp_i, refl_probe_modes, IM_ARRAYSIZE(refl_probe_modes),
                           [scene = scene_](u32 ent, i32 v) {
                               if (!scene) return;
                               auto& reg = scene->registry();
                               if (!reg.alive(static_cast<Entity>(ent))) return;
                               if (!reg.has_component<MeshRendererComponent>(
                                       static_cast<Entity>(ent))) return;
                               reg.get_component<MeshRendererComponent>(
                                   static_cast<Entity>(ent)).reflection_probes =
                                   static_cast<ReflectionProbesMode>(v);
                           })) {
                mr.reflection_probes = static_cast<ReflectionProbesMode>(rp_i);
            }
            ImGui::Unindent(8.0f);
            ImGui::Spacing();

            // Additional Settings — dynamic occlusion (frustum / hi-Z cull).
            ImGui::TextDisabled("Additional Settings");
            ImGui::Indent(8.0f);
            checkbox_undo(undo_mgr_, edit_targets, "Dynamic Occlusion", "dyn_occlusion",
                          &mr.dynamic_occlusion,
                          [scene = scene_](u32 ent, bool v) {
                              if (!scene) return;
                              auto& reg = scene->registry();
                              if (!reg.alive(static_cast<Entity>(ent))) return;
                              if (!reg.has_component<MeshRendererComponent>(
                                      static_cast<Entity>(ent))) return;
                              reg.get_component<MeshRendererComponent>(
                                  static_cast<Entity>(ent)).dynamic_occlusion = v;
                          });
            ImGui::Unindent(8.0f);
        }
    }

    // ── SpriteRendererComponent ──────────────────────────────────────────
    if (registry.has_component<SpriteRendererComponent>(target)) {
        if (component_header<SpriteRendererComponent>(registry, target, "Sprite Renderer")) {
            auto& sr = registry.get_component<SpriteRendererComponent>(target);
            const auto edit_targets = current_edit_targets();
            const bool sprite_color_mixed = !component_field_uniform<
                SpriteRendererComponent, Vec4>(
                scene_, edit_targets, &SpriteRendererComponent::color);

            f32 col[4] = {sr.color.x, sr.color.y, sr.color.z, sr.color.w};
            color_edit_n_undo(*this, undo_mgr_, edit_targets, "Color", "sprite_color",
                              4, col,
                              drag_widget_id_, drag_begin_f_, drag_begin_targets_,
                              [scene = scene_](u32 ent, const f32* v) {
                                  if (!scene) return;
                                  auto& reg = scene->registry();
                                  if (!reg.alive(static_cast<Entity>(ent))) return;
                                  if (!reg.has_component<SpriteRendererComponent>(
                                          static_cast<Entity>(ent))) return;
                                  reg.get_component<SpriteRendererComponent>(
                                      static_cast<Entity>(ent)).color =
                                      Vec4(v[0], v[1], v[2], v[3]);
                              }, sprite_color_mixed);
            sr.color = Vec4(col[0], col[1], col[2], col[3]);

            ImGui::DragFloat2("Size", &sr.size.x, 0.05f, 0.001f, 10000.0f);
            ImGui::DragFloat2("UV Min", &sr.uv_min.x, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat2("UV Max", &sr.uv_max.x, 0.01f, 0.0f, 1.0f);
            ImGui::InputInt("Sort Order", &sr.sort_order);
        }
    }

    // ── CameraComponent ──────────────────────────────────────────────────
    if (registry.has_component<CameraComponent>(target)) {
        if (component_header<CameraComponent>(registry, target, "Camera")) {
            auto& cam = registry.get_component<CameraComponent>(target);
            const auto edit_targets = current_edit_targets();

            checkbox_undo(undo_mgr_, edit_targets, "Primary", "cam_primary",
                          &cam.is_primary,
                          [scene = scene_](u32 ent, bool v) {
                              if (!scene) return;
                              auto& reg = scene->registry();
                              if (!reg.alive(static_cast<Entity>(ent))) return;
                              if (!reg.has_component<CameraComponent>(
                                      static_cast<Entity>(ent))) return;
                              reg.get_component<CameraComponent>(
                                  static_cast<Entity>(ent)).is_primary = v;
                          });
            ImGui::SameLine();
            checkbox_undo(undo_mgr_, edit_targets, "Orthographic", "cam_ortho",
                          &cam.is_orthographic,
                          [scene = scene_](u32 ent, bool v) {
                              if (!scene) return;
                              auto& reg = scene->registry();
                              if (!reg.alive(static_cast<Entity>(ent))) return;
                              if (!reg.has_component<CameraComponent>(
                                      static_cast<Entity>(ent))) return;
                              reg.get_component<CameraComponent>(
                                  static_cast<Entity>(ent)).is_orthographic = v;
                          });

            if (cam.is_orthographic) {
                f32 sz[4] = {cam.ortho_size, 0, 0, 0};
                drag_float_n_undo(*this, undo_mgr_, edit_targets, "Ortho Size",
                                  "cam_ortho_size", 1, sz, 0.1f, 0.01f, 10000.0f,
                                  drag_widget_id_, drag_begin_f_, drag_begin_targets_,
                                  [scene = scene_](u32 ent, const f32* v) {
                                      if (!scene) return;
                                      auto& reg = scene->registry();
                                      if (!reg.alive(static_cast<Entity>(ent))) return;
                                      if (!reg.has_component<CameraComponent>(
                                              static_cast<Entity>(ent))) return;
                                      reg.get_component<CameraComponent>(
                                          static_cast<Entity>(ent)).ortho_size = v[0];
                                  });
                cam.ortho_size = sz[0];
            } else {
                f32 fov[4] = {cam.fov, 0, 0, 0};
                drag_float_n_undo(*this, undo_mgr_, edit_targets, "FOV", "cam_fov",
                                  1, fov, 0.2f, 1.0f, 179.0f,
                                  drag_widget_id_, drag_begin_f_, drag_begin_targets_,
                                  [scene = scene_](u32 ent, const f32* v) {
                                      if (!scene) return;
                                      auto& reg = scene->registry();
                                      if (!reg.alive(static_cast<Entity>(ent))) return;
                                      if (!reg.has_component<CameraComponent>(
                                              static_cast<Entity>(ent))) return;
                                      reg.get_component<CameraComponent>(
                                          static_cast<Entity>(ent)).fov = v[0];
                                  });
                cam.fov = fov[0];
            }

            f32 nr[4] = {cam.near_clip, 0, 0, 0};
            drag_float_n_undo(*this, undo_mgr_, edit_targets, "Near", "cam_near",
                              1, nr, 0.01f, 0.001f, cam.far_clip - 0.01f,
                              drag_widget_id_, drag_begin_f_, drag_begin_targets_,
                              [scene = scene_](u32 ent, const f32* v) {
                                  if (!scene) return;
                                  auto& reg = scene->registry();
                                  if (!reg.alive(static_cast<Entity>(ent))) return;
                                  if (!reg.has_component<CameraComponent>(
                                          static_cast<Entity>(ent))) return;
                                  reg.get_component<CameraComponent>(
                                      static_cast<Entity>(ent)).near_clip = v[0];
                              });
            cam.near_clip = nr[0];

            f32 fr[4] = {cam.far_clip, 0, 0, 0};
            drag_float_n_undo(*this, undo_mgr_, edit_targets, "Far", "cam_far",
                              1, fr, 1.0f, cam.near_clip + 0.01f, 100000.0f,
                              drag_widget_id_, drag_begin_f_, drag_begin_targets_,
                              [scene = scene_](u32 ent, const f32* v) {
                                  if (!scene) return;
                                  auto& reg = scene->registry();
                                  if (!reg.alive(static_cast<Entity>(ent))) return;
                                  if (!reg.has_component<CameraComponent>(
                                          static_cast<Entity>(ent))) return;
                                  reg.get_component<CameraComponent>(
                                      static_cast<Entity>(ent)).far_clip = v[0];
                              });
            cam.far_clip = fr[0];
        }
    }

    // ── DirectionalLightComponent ────────────────────────────────────────
    if (registry.has_component<DirectionalLightComponent>(target)) {
        if (component_header<DirectionalLightComponent>(registry, target, "Directional Light")) {
            auto& dl = registry.get_component<DirectionalLightComponent>(target);

            f32 dir[4] = {dl.direction.x, dl.direction.y, dl.direction.z, 0};
            drag_float_n_undo(*this, undo_mgr_, current_edit_targets(), "Direction", "dir_dir",
                              3, dir, 0.01f, -1.0f, 1.0f,
                              drag_widget_id_, drag_begin_f_, drag_begin_targets_,
                              [scene = scene_](u32 ent, const f32* v) {
                                  if (!scene) return;
                                  auto& reg = scene->registry();
                                  if (!reg.alive(static_cast<Entity>(ent))) return;
                                  if (!reg.has_component<DirectionalLightComponent>(
                                          static_cast<Entity>(ent))) return;
                                  reg.get_component<DirectionalLightComponent>(
                                      static_cast<Entity>(ent)).direction =
                                      Vec3(v[0], v[1], v[2]);
                              });
            dl.direction = Vec3(dir[0], dir[1], dir[2]);

            f32 col[4] = {dl.color.x, dl.color.y, dl.color.z, 1.0f};
            const bool dir_col_mixed = !component_field_uniform<
                DirectionalLightComponent, Vec3>(
                scene_, current_edit_targets(),
                &DirectionalLightComponent::color);
            color_edit_n_undo(*this, undo_mgr_, current_edit_targets(), "Color", "dir_color",
                              3, col,
                              drag_widget_id_, drag_begin_f_, drag_begin_targets_,
                              [scene = scene_](u32 ent, const f32* v) {
                                  if (!scene) return;
                                  auto& reg = scene->registry();
                                  if (!reg.alive(static_cast<Entity>(ent))) return;
                                  if (!reg.has_component<DirectionalLightComponent>(
                                          static_cast<Entity>(ent))) return;
                                  reg.get_component<DirectionalLightComponent>(
                                      static_cast<Entity>(ent)).color =
                                      Vec3(v[0], v[1], v[2]);
                              }, dir_col_mixed);
            dl.color = Vec3(col[0], col[1], col[2]);

            f32 inten[4] = {dl.intensity, 0, 0, 0};
            drag_float_n_undo(*this, undo_mgr_, current_edit_targets(), "Intensity", "dir_intensity",
                              1, inten, 0.05f, 0.0f, 1000.0f,
                              drag_widget_id_, drag_begin_f_, drag_begin_targets_,
                              [scene = scene_](u32 ent, const f32* v) {
                                  if (!scene) return;
                                  auto& reg = scene->registry();
                                  if (!reg.alive(static_cast<Entity>(ent))) return;
                                  if (!reg.has_component<DirectionalLightComponent>(
                                          static_cast<Entity>(ent))) return;
                                  reg.get_component<DirectionalLightComponent>(
                                      static_cast<Entity>(ent)).intensity = v[0];
                              });
            dl.intensity = inten[0];
        }
    }

    // ── PointLightComponent ──────────────────────────────────────────────
    if (registry.has_component<PointLightComponent>(target)) {
        if (component_header<PointLightComponent>(registry, target, "Point Light")) {
            auto& pl = registry.get_component<PointLightComponent>(target);

            f32 col[4] = {pl.color.x, pl.color.y, pl.color.z, 1.0f};
            const bool pt_col_mixed = !component_field_uniform<
                PointLightComponent, Vec3>(
                scene_, current_edit_targets(), &PointLightComponent::color);
            color_edit_n_undo(*this, undo_mgr_, current_edit_targets(), "Color", "pt_color",
                              3, col,
                              drag_widget_id_, drag_begin_f_, drag_begin_targets_,
                              [scene = scene_](u32 ent, const f32* v) {
                                  if (!scene) return;
                                  auto& reg = scene->registry();
                                  if (!reg.alive(static_cast<Entity>(ent))) return;
                                  if (!reg.has_component<PointLightComponent>(
                                          static_cast<Entity>(ent))) return;
                                  reg.get_component<PointLightComponent>(
                                      static_cast<Entity>(ent)).color =
                                      Vec3(v[0], v[1], v[2]);
                              }, pt_col_mixed);
            pl.color = Vec3(col[0], col[1], col[2]);

            f32 inten[4] = {pl.intensity, 0, 0, 0};
            drag_float_n_undo(*this, undo_mgr_, current_edit_targets(), "Intensity", "pt_intensity",
                              1, inten, 0.05f, 0.0f, 1000.0f,
                              drag_widget_id_, drag_begin_f_, drag_begin_targets_,
                              [scene = scene_](u32 ent, const f32* v) {
                                  if (!scene) return;
                                  auto& reg = scene->registry();
                                  if (!reg.alive(static_cast<Entity>(ent))) return;
                                  if (!reg.has_component<PointLightComponent>(
                                          static_cast<Entity>(ent))) return;
                                  reg.get_component<PointLightComponent>(
                                      static_cast<Entity>(ent)).intensity = v[0];
                              });
            pl.intensity = inten[0];

            f32 rad[4] = {pl.radius, 0, 0, 0};
            drag_float_n_undo(*this, undo_mgr_, current_edit_targets(), "Radius", "pt_radius",
                              1, rad, 0.1f, 0.01f, 1000.0f,
                              drag_widget_id_, drag_begin_f_, drag_begin_targets_,
                              [scene = scene_](u32 ent, const f32* v) {
                                  if (!scene) return;
                                  auto& reg = scene->registry();
                                  if (!reg.alive(static_cast<Entity>(ent))) return;
                                  if (!reg.has_component<PointLightComponent>(
                                          static_cast<Entity>(ent))) return;
                                  reg.get_component<PointLightComponent>(
                                      static_cast<Entity>(ent)).radius = v[0];
                              });
            pl.radius = rad[0];
        }
    }

    // ── SpotLightComponent ───────────────────────────────────────────────
    if (registry.has_component<SpotLightComponent>(target)) {
        if (component_header<SpotLightComponent>(registry, target, "Spot Light")) {
            auto& sl = registry.get_component<SpotLightComponent>(target);

            f32 col[4] = {sl.color.x, sl.color.y, sl.color.z, 1.0f};
            color_edit_n_undo(*this, undo_mgr_, current_edit_targets(), "Color", "spot_color",
                              3, col,
                              drag_widget_id_, drag_begin_f_, drag_begin_targets_,
                              [scene = scene_](u32 ent, const f32* v) {
                                  if (!scene) return;
                                  auto& reg = scene->registry();
                                  if (!reg.alive(static_cast<Entity>(ent))) return;
                                  if (!reg.has_component<SpotLightComponent>(
                                          static_cast<Entity>(ent))) return;
                                  reg.get_component<SpotLightComponent>(
                                      static_cast<Entity>(ent)).color =
                                      Vec3(v[0], v[1], v[2]);
                              });
            sl.color = Vec3(col[0], col[1], col[2]);

            ImGui::DragFloat("Intensity", &sl.intensity, 0.05f, 0.0f, 1000.0f);
            ImGui::DragFloat("Range", &sl.range, 0.1f, 0.01f, 1000.0f);
            ImGui::DragFloat("Inner Angle", &sl.inner_angle, 0.1f, 0.0f, 89.0f);
            ImGui::DragFloat("Outer Angle", &sl.outer_angle, 0.1f, sl.inner_angle, 89.9f);
        }
    }

    // ── AudioSourceComponent ─────────────────────────────────────────────
    if (registry.has_component<AudioSourceComponent>(target)) {
        if (component_header<AudioSourceComponent>(registry, target, "Audio Source")) {
            auto& as = registry.get_component<AudioSourceComponent>(target);
            const auto edit_targets = current_edit_targets();

            int clip_i = static_cast<int>(as.clip_id);
            if (ImGui::InputInt("Clip ID", &clip_i)) {
                as.clip_id = static_cast<u32>(std::max(0, clip_i));
            }
            if (asset_paths_.audio) {
                std::string label;
                if (as.clip_id == 0) {
                    label = "(none)";
                } else {
                    std::string p = asset_paths_.audio(as.clip_id);
                    label = p.empty()
                        ? "<unknown>"
                        : std::filesystem::path(p).filename().string();
                }
                ImGui::TextDisabled("  → %s", label.c_str());
                if (as.clip_id != 0) {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Clear##audio")) as.clip_id = 0;
                }
            }
            int bus_i = static_cast<int>(as.bus);
            if (ImGui::InputInt("Bus", &bus_i)) {
                as.bus = static_cast<u32>(std::max(0, bus_i));
            }

            f32 vol[4] = {as.volume, 0, 0, 0};
            drag_float_n_undo(*this, undo_mgr_, edit_targets, "Volume", "audio_volume",
                              1, vol, 0.01f, 0.0f, 2.0f,
                              drag_widget_id_, drag_begin_f_, drag_begin_targets_,
                              [scene = scene_](u32 ent, const f32* v) {
                                  if (!scene) return;
                                  auto& reg = scene->registry();
                                  if (!reg.alive(static_cast<Entity>(ent))) return;
                                  if (!reg.has_component<AudioSourceComponent>(
                                          static_cast<Entity>(ent))) return;
                                  reg.get_component<AudioSourceComponent>(
                                      static_cast<Entity>(ent)).volume = v[0];
                              });
            as.volume = vol[0];

            f32 ptch[4] = {as.pitch, 0, 0, 0};
            drag_float_n_undo(*this, undo_mgr_, edit_targets, "Pitch", "audio_pitch",
                              1, ptch, 0.01f, 0.01f, 4.0f,
                              drag_widget_id_, drag_begin_f_, drag_begin_targets_,
                              [scene = scene_](u32 ent, const f32* v) {
                                  if (!scene) return;
                                  auto& reg = scene->registry();
                                  if (!reg.alive(static_cast<Entity>(ent))) return;
                                  if (!reg.has_component<AudioSourceComponent>(
                                          static_cast<Entity>(ent))) return;
                                  reg.get_component<AudioSourceComponent>(
                                      static_cast<Entity>(ent)).pitch = v[0];
                              });
            as.pitch = ptch[0];

            checkbox_undo(undo_mgr_, edit_targets, "Looping", "audio_looping",
                          &as.looping,
                          [scene = scene_](u32 ent, bool v) {
                              if (!scene) return;
                              auto& reg = scene->registry();
                              if (!reg.alive(static_cast<Entity>(ent))) return;
                              if (!reg.has_component<AudioSourceComponent>(
                                      static_cast<Entity>(ent))) return;
                              reg.get_component<AudioSourceComponent>(
                                  static_cast<Entity>(ent)).looping = v;
                          });
            ImGui::SameLine();
            checkbox_undo(undo_mgr_, edit_targets, "Spatial", "audio_spatial",
                          &as.spatial,
                          [scene = scene_](u32 ent, bool v) {
                              if (!scene) return;
                              auto& reg = scene->registry();
                              if (!reg.alive(static_cast<Entity>(ent))) return;
                              if (!reg.has_component<AudioSourceComponent>(
                                      static_cast<Entity>(ent))) return;
                              reg.get_component<AudioSourceComponent>(
                                  static_cast<Entity>(ent)).spatial = v;
                          });
            ImGui::SameLine();
            checkbox_undo(undo_mgr_, edit_targets, "Play On Start", "audio_play_on_start",
                          &as.play_on_start,
                          [scene = scene_](u32 ent, bool v) {
                              if (!scene) return;
                              auto& reg = scene->registry();
                              if (!reg.alive(static_cast<Entity>(ent))) return;
                              if (!reg.has_component<AudioSourceComponent>(
                                      static_cast<Entity>(ent))) return;
                              reg.get_component<AudioSourceComponent>(
                                  static_cast<Entity>(ent)).play_on_start = v;
                          });
            if (as.spatial) {
                f32 mn[4] = {as.min_distance, 0, 0, 0};
                drag_float_n_undo(*this, undo_mgr_, edit_targets, "Min Distance",
                                  "audio_min_dist", 1, mn, 0.1f, 0.01f, 10000.0f,
                                  drag_widget_id_, drag_begin_f_, drag_begin_targets_,
                                  [scene = scene_](u32 ent, const f32* v) {
                                      if (!scene) return;
                                      auto& reg = scene->registry();
                                      if (!reg.alive(static_cast<Entity>(ent))) return;
                                      if (!reg.has_component<AudioSourceComponent>(
                                              static_cast<Entity>(ent))) return;
                                      reg.get_component<AudioSourceComponent>(
                                          static_cast<Entity>(ent)).min_distance = v[0];
                                  });
                as.min_distance = mn[0];

                f32 mx[4] = {as.max_distance, 0, 0, 0};
                drag_float_n_undo(*this, undo_mgr_, edit_targets, "Max Distance",
                                  "audio_max_dist", 1, mx, 0.1f, as.min_distance, 10000.0f,
                                  drag_widget_id_, drag_begin_f_, drag_begin_targets_,
                                  [scene = scene_](u32 ent, const f32* v) {
                                      if (!scene) return;
                                      auto& reg = scene->registry();
                                      if (!reg.alive(static_cast<Entity>(ent))) return;
                                      if (!reg.has_component<AudioSourceComponent>(
                                              static_cast<Entity>(ent))) return;
                                      reg.get_component<AudioSourceComponent>(
                                          static_cast<Entity>(ent)).max_distance = v[0];
                                  });
                as.max_distance = mx[0];
            }
        }
    }

    // ── AnimatorComponent ─────────────────────────────────────────────────
    //
    // Pairs the entity with an AnimationClip via clip_id (id allocated by
    // AnimationAssetCache).  Inspector shows the resolved filename next to
    // the raw int id, exposes speed / looping / play_on_start, and offers
    // a Play / Pause / Reset row that maps directly to the runtime
    // AnimatorSystem's `playing` / `time` fields.  All edits respect undo.
    if (registry.has_component<AnimatorComponent>(target)) {
        if (component_header<AnimatorComponent>(registry, target, "Animator")) {
            auto& an = registry.get_component<AnimatorComponent>(target);
            const auto edit_targets = current_edit_targets();

            int clip_i = static_cast<int>(an.clip_id);
            if (ImGui::InputInt("Clip ID##anim", &clip_i)) {
                an.clip_id = static_cast<u32>(std::max(0, clip_i));
            }
            if (asset_paths_.animation) {
                std::string label;
                if (an.clip_id == 0) {
                    label = "(none)";
                } else {
                    std::string p = asset_paths_.animation(an.clip_id);
                    label = p.empty()
                        ? "<unknown>"
                        : std::filesystem::path(p).filename().string();
                }
                ImGui::TextDisabled("  → %s", label.c_str());
                if (an.clip_id != 0) {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Clear##anim")) an.clip_id = 0;
                }
            }

            f32 speed[4] = {an.speed, 0, 0, 0};
            drag_float_n_undo(*this, undo_mgr_, edit_targets, "Speed",
                              "anim_speed", 1, speed, 0.01f, -10.0f, 10.0f,
                              drag_widget_id_, drag_begin_f_, drag_begin_targets_,
                              [scene = scene_](u32 ent, const f32* v) {
                                  if (!scene) return;
                                  auto& reg = scene->registry();
                                  if (!reg.alive(static_cast<Entity>(ent))) return;
                                  if (!reg.has_component<AnimatorComponent>(
                                          static_cast<Entity>(ent))) return;
                                  reg.get_component<AnimatorComponent>(
                                      static_cast<Entity>(ent)).speed = v[0];
                              });
            an.speed = speed[0];

            checkbox_undo(undo_mgr_, edit_targets, "Looping", "anim_looping",
                          &an.looping,
                          [scene = scene_](u32 ent, bool v) {
                              if (!scene) return;
                              auto& reg = scene->registry();
                              if (!reg.alive(static_cast<Entity>(ent))) return;
                              if (!reg.has_component<AnimatorComponent>(
                                      static_cast<Entity>(ent))) return;
                              reg.get_component<AnimatorComponent>(
                                  static_cast<Entity>(ent)).looping = v;
                          });
            ImGui::SameLine();
            checkbox_undo(undo_mgr_, edit_targets, "Play On Start",
                          "anim_play_on_start", &an.play_on_start,
                          [scene = scene_](u32 ent, bool v) {
                              if (!scene) return;
                              auto& reg = scene->registry();
                              if (!reg.alive(static_cast<Entity>(ent))) return;
                              if (!reg.has_component<AnimatorComponent>(
                                      static_cast<Entity>(ent))) return;
                              reg.get_component<AnimatorComponent>(
                                  static_cast<Entity>(ent)).play_on_start = v;
                          });

            // Transport row: Play / Pause / Reset.  In editor mode these
            // just flip flags + reset playhead so the user can scrub the
            // value live by editing `time`; the AnimatorSystem only ticks
            // during Play mode.
            ImGui::Text("Time: %.3f", an.time);
            if (ImGui::SmallButton(an.playing ? "Pause##anim" : "Play##anim")) {
                an.playing = !an.playing;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset##anim")) {
                an.time    = 0.0f;
                an.playing = false;
            }
        }
    }

    // ── ScriptComponent ───────────────────────────────────────────────────
    //
    // Holds the Lua script name + enabled flag.  on_create / on_update /
    // on_destroy are bound at runtime by ScriptSystem and aren't editable
    // from here.  Inspector exposes:
    //   - Script Name (text field — direct edit, no undo since it's a
    //     identifier-binding op, not a property tween).
    //   - Enabled toggle (Unity convention).
    //   - Initialised status (read-only) so users see whether on_create
    //     has fired yet.
    //   - Reload button — clears `initialized` so the script's on_create
    //     fires again next ScriptSystem tick (matches Unity's "Reset" in
    //     the script gear menu).
    if (registry.has_component<nexus::scripting::ScriptComponent>(target)) {
        if (component_header<nexus::scripting::ScriptComponent>(
                registry, target, "Script")) {
            auto& sc = registry.get_component<
                nexus::scripting::ScriptComponent>(target);

            char buf[256];
            std::snprintf(buf, sizeof(buf), "%s", sc.script_name.c_str());
            ImGui::SetNextItemWidth(-100.0f);
            if (ImGui::InputTextWithHint("Script##script_name",
                                         "(empty)", buf, sizeof(buf))) {
                sc.script_name = buf;
                // Renaming the bound script invalidates the previous
                // bindings; ScriptSystem will rebind on the next tick.
                sc.initialized = false;
            }

            ImGui::Checkbox("Enabled##script_enabled", &sc.enabled);

            ImGui::SameLine();
            if (ImGui::SmallButton("Reload##script_reload")) {
                // Force a re-bind: ScriptSystem checks `initialized` and
                // re-runs on_create when false.  Lifecycle callbacks are
                // bound by name so changing script_name + reloading is
                // the standard hot-swap path.
                sc.initialized = false;
            }

            ImGui::TextDisabled(sc.initialized
                ? "Status: initialised (on_create fired)"
                : "Status: pending — on_create runs next tick");
        }
    }

    // ── AudioListenerComponent ───────────────────────────────────────────
    if (registry.has_component<AudioListenerComponent>(target)) {
        if (component_header<AudioListenerComponent>(registry, target, "Audio Listener")) {
            auto& al = registry.get_component<AudioListenerComponent>(target);
            checkbox_undo(undo_mgr_, current_edit_targets(),
                          "Active", "listener_active", &al.active,
                          [scene = scene_](u32 ent, bool v) {
                              if (!scene) return;
                              auto& reg = scene->registry();
                              if (!reg.alive(static_cast<Entity>(ent))) return;
                              if (!reg.has_component<AudioListenerComponent>(
                                      static_cast<Entity>(ent))) return;
                              reg.get_component<AudioListenerComponent>(
                                  static_cast<Entity>(ent)).active = v;
                          });
        }
    }

    // ── RigidBody3DComponent ─────────────────────────────────────────────
    if (registry.has_component<RigidBody3DComponent>(target)) {
        if (component_header<RigidBody3DComponent>(registry, target, "Rigid Body 3D")) {
            auto& rb = registry.get_component<RigidBody3DComponent>(target);
            const char* types[] = { "Static", "Dynamic", "Kinematic" };
            i32 type_i = static_cast<i32>(rb.type);
            if (combo_undo(undo_mgr_, current_edit_targets(),
                           "Type", "rb3d_type", &type_i,
                           types, IM_ARRAYSIZE(types),
                           [scene = scene_](u32 ent, i32 v) {
                               if (!scene) return;
                               auto& reg = scene->registry();
                               if (!reg.alive(static_cast<Entity>(ent))) return;
                               if (!reg.has_component<RigidBody3DComponent>(
                                       static_cast<Entity>(ent))) return;
                               reg.get_component<RigidBody3DComponent>(
                                   static_cast<Entity>(ent)).type =
                                   static_cast<RigidBody3DComponent::Type>(v);
                           })) {
                rb.type = static_cast<RigidBody3DComponent::Type>(type_i);
            }
            ImGui::DragFloat("Mass", &rb.mass, 0.05f, 0.0f, 1000.0f);
            ImGui::DragFloat("Friction", &rb.friction, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Restitution", &rb.restitution, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Linear Damping", &rb.linear_damping, 0.01f, 0.0f, 10.0f);
            ImGui::DragFloat("Angular Damping", &rb.angular_damping, 0.01f, 0.0f, 10.0f);
            ImGui::DragFloat("Gravity Scale", &rb.gravity_scale, 0.01f, -10.0f, 10.0f);
            ImGui::DragFloat3("Velocity", &rb.velocity.x, 0.1f);
            ImGui::DragFloat3("Angular Velocity", &rb.angular_velocity.x, 0.1f);
        }
    }

    // ── RigidBody2DComponent ─────────────────────────────────────────────
    if (registry.has_component<RigidBody2DComponent>(target)) {
        if (component_header<RigidBody2DComponent>(registry, target, "Rigid Body 2D")) {
            auto& rb = registry.get_component<RigidBody2DComponent>(target);
            const char* types[] = { "Static", "Dynamic", "Kinematic" };
            i32 type_i = static_cast<i32>(rb.type);
            if (combo_undo(undo_mgr_, current_edit_targets(),
                           "Type", "rb2d_type", &type_i,
                           types, IM_ARRAYSIZE(types),
                           [scene = scene_](u32 ent, i32 v) {
                               if (!scene) return;
                               auto& reg = scene->registry();
                               if (!reg.alive(static_cast<Entity>(ent))) return;
                               if (!reg.has_component<RigidBody2DComponent>(
                                       static_cast<Entity>(ent))) return;
                               reg.get_component<RigidBody2DComponent>(
                                   static_cast<Entity>(ent)).type =
                                   static_cast<RigidBody2DComponent::Type>(v);
                           })) {
                rb.type = static_cast<RigidBody2DComponent::Type>(type_i);
            }
            ImGui::DragFloat("Density", &rb.density, 0.05f, 0.0f, 1000.0f);
            ImGui::DragFloat("Friction", &rb.friction, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Restitution", &rb.restitution, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Linear Damping", &rb.linear_damping, 0.01f, 0.0f, 10.0f);
            ImGui::DragFloat("Angular Damping", &rb.angular_damping, 0.01f, 0.0f, 10.0f);
            ImGui::DragFloat("Gravity Scale", &rb.gravity_scale, 0.01f, -10.0f, 10.0f);
            checkbox_undo(undo_mgr_, current_edit_targets(),
                          "Fixed Rotation", "rb2d_fixed_rot",
                          &rb.fixed_rotation,
                          [scene = scene_](u32 ent, bool v) {
                              if (!scene) return;
                              auto& reg = scene->registry();
                              if (!reg.alive(static_cast<Entity>(ent))) return;
                              if (!reg.has_component<RigidBody2DComponent>(
                                      static_cast<Entity>(ent))) return;
                              reg.get_component<RigidBody2DComponent>(
                                  static_cast<Entity>(ent)).fixed_rotation = v;
                          });
            ImGui::DragFloat2("Velocity", &rb.velocity.x, 0.1f);
            ImGui::DragFloat("Angular Velocity", &rb.angular_velocity, 0.1f);
        }
    }

    // ── Collider3DComponent ──────────────────────────────────────────────
    if (registry.has_component<Collider3DComponent>(target)) {
        if (component_header<Collider3DComponent>(registry, target, "Collider 3D")) {
            auto& c = registry.get_component<Collider3DComponent>(target);
            const char* shapes[] = { "Box", "Sphere", "Capsule" };
            int shape_i = static_cast<int>(c.shape);
            if (ImGui::Combo("Shape", &shape_i, shapes, IM_ARRAYSIZE(shapes))) {
                c.shape = static_cast<Collider3DComponent::Shape>(shape_i);
            }
            ImGui::DragFloat3("Offset", &c.offset.x, 0.05f);
            if (c.shape == Collider3DComponent::Box) {
                ImGui::DragFloat3("Half Extents", &c.half_extents.x, 0.05f, 0.001f, 1000.0f);
            } else {
                ImGui::DragFloat("Radius", &c.radius, 0.05f, 0.001f, 1000.0f);
                if (c.shape == Collider3DComponent::Capsule) {
                    ImGui::DragFloat("Height", &c.height, 0.05f, 0.001f, 1000.0f);
                }
            }
            ImGui::Checkbox("Trigger", &c.is_trigger);
        }
    }

    // ── Collider2DComponent ──────────────────────────────────────────────
    if (registry.has_component<Collider2DComponent>(target)) {
        if (component_header<Collider2DComponent>(registry, target, "Collider 2D")) {
            auto& c = registry.get_component<Collider2DComponent>(target);
            const char* shapes[] = { "Box", "Circle", "Polygon" };
            int shape_i = static_cast<int>(c.shape);
            if (ImGui::Combo("Shape", &shape_i, shapes, IM_ARRAYSIZE(shapes))) {
                c.shape = static_cast<Collider2DComponent::Shape>(shape_i);
            }
            ImGui::DragFloat2("Offset", &c.offset.x, 0.05f);
            if (c.shape == Collider2DComponent::Box) {
                ImGui::DragFloat2("Half Size", &c.half_size.x, 0.05f, 0.001f, 1000.0f);
            } else if (c.shape == Collider2DComponent::Circle) {
                ImGui::DragFloat("Radius", &c.radius, 0.05f, 0.001f, 1000.0f);
            }
            ImGui::Checkbox("Trigger", &c.is_trigger);
        }
    }

    // ── TilemapComponent (read-only summary) ─────────────────────────────
    if (registry.has_component<TilemapComponent>(target)) {
        if (component_header<TilemapComponent>(registry, target, "Tilemap")) {
            auto& tm = registry.get_component<TilemapComponent>(target);
            ImGui::Text("Size: %u x %u (%zu tiles)",
                        tm.width, tm.height, tm.tiles.size());
            ImGui::DragFloat("Tile Size", &tm.tile_size, 0.01f, 0.01f, 1000.0f);
            int tex_i = static_cast<int>(tm.texture_id);
            if (ImGui::InputInt("Texture ID", &tex_i)) {
                tm.texture_id = static_cast<u32>(std::max(0, tex_i));
            }
        }
    }

    ui::separator();

    // ── Add Component button ─────────────────────────────────────────────
    // Driven by ComponentRegistry so engine + user component types live in
    // one place.  Popup matches Unity's UX: a search box at the top, then
    // a flat list of available components grouped by category.  Already-
    // attached types are filtered out so the user can't double-add.
    //
    // Multi-select broadcast: when the EditorSelection has 2+ entries the
    // add fires for every selected entity inside a single LambdaCommand so
    // Ctrl+Z reverts the entire batch.  Locked entities in the selection
    // are skipped silently — matches the visibility / property edit policy.
    const float avail_w = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail_w - 220.0f) * 0.5f);
    const bool has_registry = component_registry_ != nullptr;
    if (!has_registry) ImGui::BeginDisabled();
    if (ImGui::Button("Add Component", ImVec2(220.0f, 0.0f))) {
        std::memset(add_component_search_, 0, sizeof(add_component_search_));
        ImGui::OpenPopup("InspectorAddComponentPopup");
    }
    if (!has_registry) ImGui::EndDisabled();

    if (has_registry &&
        ImGui::BeginPopup("InspectorAddComponentPopup")) {
        ImGui::TextDisabled("Component");
        ImGui::Separator();
        ImGui::SetNextItemWidth(260.0f);
        ImGui::InputTextWithHint("##AddComponentSearch", "Search...",
                                 add_component_search_,
                                 sizeof(add_component_search_));
        ImGui::Separator();

        // Resolve the broadcast targets ONCE so the same set is used for
        // both filtering ("hide if every target already has it") and
        // application.  Filtering against the primary target alone would
        // surface entries that fail-silent on N-1 of the selection.
        const std::vector<u32> targets = current_edit_targets();
        const std::string query(add_component_search_);

        // For each registered descriptor, hide if (a) every target already
        // has it, or (b) the query doesn't match name/category.  Matches
        // Unity's "show only what would change something" behaviour.
        ImGui::BeginChild("##AddComponentList", ImVec2(280.0f, 360.0f),
                          false, ImGuiWindowFlags_HorizontalScrollbar);
        std::string current_category;
        u32 visible_count = 0;
        for (const auto& d : component_registry_->descriptors()) {
            // Query filter (case-insensitive substring match on name OR
            // category, as available_to() does).
            auto icontains_local = [](const std::string& h,
                                      const std::string& n) {
                if (n.empty()) return true;
                if (n.size() > h.size()) return false;
                for (size_t i = 0; i + n.size() <= h.size(); ++i) {
                    bool ok = true;
                    for (size_t j = 0; j < n.size(); ++j) {
                        unsigned char a = static_cast<unsigned char>(h[i + j]);
                        unsigned char b = static_cast<unsigned char>(n[j]);
                        if (std::tolower(a) != std::tolower(b)) {
                            ok = false; break;
                        }
                    }
                    if (ok) return true;
                }
                return false;
            };
            if (!icontains_local(d.name, query) &&
                !icontains_local(d.category, query)) {
                continue;
            }
            // Hide if every target already has it (no-op work).
            bool all_have = !targets.empty();
            for (u32 t : targets) {
                if (!d.has(registry, static_cast<u64>(t))) {
                    all_have = false;
                    break;
                }
            }
            if (all_have) continue;

            if (current_category != d.category) {
                current_category = d.category;
                ImGui::Spacing();
                ImGui::TextDisabled("%s", d.category.c_str());
                ImGui::Separator();
            }
            ++visible_count;

            char label[160];
            std::snprintf(label, sizeof(label), "%s##cmp-%s",
                          d.name.c_str(), d.type_id.c_str());
            if (ImGui::Selectable(label)) {
                // Capture the *names* (not pointers) so undo state can
                // outlive the registry's vector (e.g. registry mutation
                // mid-undo).  The descriptor lookup is O(N) but N is small.
                const std::string type_id = d.type_id;
                const std::string desc_name = d.name;
                ComponentRegistry* reg_ptr = component_registry_;
                Scene* sc = scene_;
                std::vector<u32> targets_copy = targets;

                if (undo_mgr_ && sc) {
                    undo_mgr_->execute(std::make_unique<LambdaCommand>(
                        std::string("Add Component: ") + desc_name,
                        [reg_ptr, sc, targets_copy, type_id]() {
                            if (!sc || !reg_ptr) return;
                            const ComponentDescriptor* dd =
                                reg_ptr->find(type_id);
                            if (!dd) return;
                            auto& r = sc->registry();
                            for (u32 ent : targets_copy) {
                                if (!r.alive(static_cast<Entity>(ent))) continue;
                                if (!dd->has(r, ent)) dd->add(r, ent);
                            }
                        },
                        [reg_ptr, sc, targets_copy, type_id]() {
                            if (!sc || !reg_ptr) return;
                            const ComponentDescriptor* dd =
                                reg_ptr->find(type_id);
                            if (!dd) return;
                            auto& r = sc->registry();
                            for (u32 ent : targets_copy) {
                                if (!r.alive(static_cast<Entity>(ent))) continue;
                                dd->remove(r, ent);
                            }
                        }));
                } else {
                    // No undo manager bound — apply directly.
                    for (u32 ent : targets_copy) {
                        if (!d.has(registry, ent)) d.add(registry, ent);
                    }
                }
                ImGui::CloseCurrentPopup();
            }
        }
        if (visible_count == 0) {
            ImGui::TextDisabled("No matches.");
        }
        ImGui::EndChild();
        ImGui::EndPopup();
    }

    if (entity_locked) ImGui::EndDisabled();

    ui::end_window();
}

std::vector<PropertyEdit> InspectorPanel::drain_edits() {
    std::vector<PropertyEdit> result;
    std::swap(result, pending_edits_);
    return result;
}

std::vector<u32> InspectorPanel::current_edit_targets() const {
    // Multi-select broadcast only fires when (a) the bound selection has
    // 2+ entries AND (b) the inspector's primary target is among them.
    // Otherwise the user is editing a "frozen" entity (locked inspector,
    // or selection was dropped); fall back to the single target.
    if (ext_selection_ && ext_selection_->count() > 1 &&
        has_target_ && ext_selection_->is_selected(target_)) {
        return ext_selection_->all();
    }
    if (has_target_) return {target_};
    return {};
}

// ── ConsolePanel ────────────────────────────────────────────────────────────

namespace {

/// Stable 64-bit hash of (text, level) — used as the dedup key in
/// Collapse mode.  FNV-1a chosen for zero deps and adequate collision
/// resistance for the small console message domain.
u64 console_dedup_hash(const std::string& text, LogLevel level) {
    constexpr u64 kFnvOffset = 0xcbf29ce484222325ull;
    constexpr u64 kFnvPrime  = 0x00000100000001b3ull;
    u64 h = kFnvOffset;
    for (char raw : text) {
        h ^= static_cast<u64>(static_cast<unsigned char>(raw));
        h *= kFnvPrime;
    }
    h ^= static_cast<u64>(level);
    h *= kFnvPrime;
    return h;
}

bool icontains(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    if (needle.size() > haystack.size()) return false;
    auto lower = [](char c) {
        return static_cast<char>(
            std::tolower(static_cast<unsigned char>(c)));
    };
    for (size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        bool match = true;
        for (size_t j = 0; j < needle.size(); ++j) {
            if (lower(haystack[i + j]) != lower(needle[j])) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

const char* level_prefix(LogLevel l) {
    switch (l) {
        case LogLevel::Info:    return "[INFO]";
        case LogLevel::Warning: return "[WARN]";
        case LogLevel::Error:   return "[ERR ]";
        case LogLevel::Debug:   return "[DBG ]";
    }
    return "[?   ]";
}

ImVec4 level_color(LogLevel l) {
    switch (l) {
        case LogLevel::Info:    return ImVec4(0.92f, 0.92f, 0.92f, 1.0f);
        case LogLevel::Warning: return ImVec4(1.00f, 0.85f, 0.25f, 1.0f);
        case LogLevel::Error:   return ImVec4(1.00f, 0.35f, 0.35f, 1.0f);
        case LogLevel::Debug:   return ImVec4(0.60f, 0.60f, 0.60f, 1.0f);
    }
    return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
}

void format_timestamp(f64 sec, char* out, size_t out_size) {
    const u64 total = static_cast<u64>(sec);
    const u32 hh = static_cast<u32>((total / 3600) % 24);
    const u32 mm = static_cast<u32>((total / 60) % 60);
    const u32 ss = static_cast<u32>(total % 60);
    const u32 ms = static_cast<u32>((sec - static_cast<f64>(total)) * 1000.0);
    std::snprintf(out, out_size, "%02u:%02u:%02u.%03u", hh, mm, ss, ms);
}

} // namespace (anonymous)

void ConsolePanel::on_render() {
    namespace ui = nexus::editor::imgui;

    ui::begin_window("Console");

    // ── Toolbar row 1: actions + segmented level filters ──────────────
    if (ImGui::SmallButton("Clear")) clear();
    ImGui::SameLine();
    if (ImGui::SmallButton("Copy")) {
        std::string out;
        out.reserve(messages_.size() * 80);
        char ts[32];
        for (const auto& msg : messages_) {
            if (!is_level_shown(msg.level)) continue;
            if (!icontains(msg.text, search_)) continue;
            format_timestamp(msg.timestamp, ts, sizeof(ts));
            out += '[';
            out += ts;
            out += "] ";
            out += level_prefix(msg.level);
            out += ' ';
            out += msg.text;
            if (msg.count > 1) {
                char xbuf[16];
                std::snprintf(xbuf, sizeof(xbuf), " (x%u)", msg.count);
                out += xbuf;
            }
            out += '\n';
        }
        ImGui::SetClipboardText(out.c_str());
    }
    ImGui::SameLine();
    ImGui::Checkbox("Collapse", &collapse_);
    ImGui::SameLine();
    ImGui::Checkbox("Clear on Play", &clear_on_play_);
    ImGui::SameLine();
    ImGui::Checkbox("Error Pause", &error_pause_);
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &auto_scroll_);

    // Segmented level toggles with live counts (Unity-style).  Glyphs use
    // bullet/exclamation/cross to read like the colored circle icons in
    // Unity's Console without dragging in a custom icon font.
    const struct { LogLevel lvl; const char* glyph; bool* flag; u32 count; } rows[] = {
        { LogLevel::Info,    "(i)", &show_info_,    info_count_    },
        { LogLevel::Warning, "(!)", &show_warning_, warning_count_ },
        { LogLevel::Error,   "(x)", &show_error_,   error_count_   },
        { LogLevel::Debug,   "(d)", &show_debug_,   debug_count_   },
    };
    for (const auto& r : rows) {
        ImGui::SameLine();
        ImGui::PushID(r.glyph);
        ImVec4 col = *r.flag ? level_color(r.lvl)
                             : ImVec4(0.45f, 0.45f, 0.45f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        char label[24];
        std::snprintf(label, sizeof(label), "%s %u", r.glyph, r.count);
        if (ImGui::SmallButton(label)) *r.flag = !*r.flag;
        ImGui::PopStyleColor();
        ImGui::PopID();
    }

    // ── Toolbar row 2: search box ─────────────────────────────────────
    char search_buf[256];
    std::snprintf(search_buf, sizeof(search_buf), "%s", search_.c_str());
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputTextWithHint("##ConsoleSearch",
                                  "Search (case-insensitive)",
                                  search_buf, sizeof(search_buf))) {
        search_ = search_buf;
    }
    ImGui::Separator();

    // ── Scrollable message log ────────────────────────────────────────
    ImGui::BeginChild("ConsoleLog", ImVec2(0, 0), true);

    char ts[32];
    for (const auto& msg : messages_) {
        if (!is_level_shown(msg.level)) continue;
        if (!icontains(msg.text, search_)) continue;

        ImVec4 col = level_color(msg.level);
        format_timestamp(msg.timestamp, ts, sizeof(ts));
        if (msg.count > 1) {
            ImGui::TextColored(col, "[%s] %s %s  (x%u)",
                               ts, level_prefix(msg.level),
                               msg.text.c_str(), msg.count);
        } else {
            ImGui::TextColored(col, "[%s] %s %s",
                               ts, level_prefix(msg.level), msg.text.c_str());
        }
    }

    // Smart auto-scroll: only force scroll when the user was already at the
    // bottom on the *previous* frame.  If they scrolled up to inspect old
    // messages, new messages no longer yank them back down.
    const float max_y = ImGui::GetScrollMaxY();
    const float cur_y = ImGui::GetScrollY();
    const bool at_bottom = (cur_y >= max_y - 2.0f);
    if (auto_scroll_ && was_at_bottom_) {
        ImGui::SetScrollHereY(1.0f);
    }
    was_at_bottom_ = at_bottom;

    ImGui::EndChild();
    ui::end_window();
}

void ConsolePanel::add_message(const std::string& text, LogLevel level) {
    const f64 ts = ImGui::GetCurrentContext() ? ImGui::GetTime() : 0.0;

    // Collapse mode: fold into the most recent matching entry.  Only the
    // last message is checked — Unity behavior: distinct messages between
    // duplicates "break" the run, so old duplicates don't merge.
    const u64 key = console_dedup_hash(text, level);
    if (collapse_ && !messages_.empty()) {
        ConsoleMessage& back = messages_.back();
        if (back.dedup_key == key && back.text == text && back.level == level) {
            ++back.count;
            back.timestamp = ts;
            // Severity counters increment per *occurrence*, matching Unity's
            // status-bar badge behavior.
            switch (level) {
                case LogLevel::Info:    ++info_count_;    break;
                case LogLevel::Warning: ++warning_count_; break;
                case LogLevel::Error:   ++error_count_;   break;
                case LogLevel::Debug:   ++debug_count_;   break;
            }
            if (level == LogLevel::Error && error_pause_) error_pause_request_ = true;
            return;
        }
    }

    ConsoleMessage msg;
    msg.text = text;
    msg.level = level;
    msg.timestamp = ts;
    msg.dedup_key = key;
    messages_.push_back(std::move(msg));

    switch (level) {
        case LogLevel::Info:    ++info_count_;    break;
        case LogLevel::Warning: ++warning_count_; break;
        case LogLevel::Error:   ++error_count_;   break;
        case LogLevel::Debug:   ++debug_count_;   break;
    }
    if (level == LogLevel::Error && error_pause_) error_pause_request_ = true;

    // Prune oldest while preserving severity-counter parity with the buffer.
    while (messages_.size() > max_messages_) {
        const LogLevel dropped = messages_.front().level;
        const u32 dropped_count = messages_.front().count;
        auto dec = [&](u32& c) { c = c > dropped_count ? c - dropped_count : 0; };
        switch (dropped) {
            case LogLevel::Info:    dec(info_count_);    break;
            case LogLevel::Warning: dec(warning_count_); break;
            case LogLevel::Error:   dec(error_count_);   break;
            case LogLevel::Debug:   dec(debug_count_);   break;
        }
        messages_.erase(messages_.begin());
    }
}

void ConsolePanel::clear() {
    messages_.clear();
    info_count_ = 0;
    warning_count_ = 0;
    error_count_ = 0;
    debug_count_ = 0;
    error_pause_request_ = false;
}

void ConsolePanel::set_collapse_mode(bool c) {
    collapse_ = c;
    // Toggling collapse on does NOT retroactively merge — that would mutate
    // history the user can already see.  Future messages collapse; existing
    // ones stay as-is.  Matches Unity behavior.
}

void ConsolePanel::on_enter_play() {
    if (clear_on_play_) clear();
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

namespace {

/// Classify an asset entry and return a short, monospaced bracket tag that
/// doubles as a primitive icon until real thumbnail rendering lands.
const char* asset_icon(const AssetBrowserEntry& entry) {
    if (entry.is_directory) return "[DIR]";
    const auto& ext = entry.extension;
    if (ext == ".bmp" || ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
        ext == ".tga" || ext == ".hdr") return "[IMG]";
    if (ext == ".obj" || ext == ".gltf" || ext == ".glb" ||
        ext == ".fbx" || ext == ".dae")  return "[MESH]";
    if (ext == ".wav" || ext == ".ogg" || ext == ".mp3") return "[SFX]";
    if (ext == ".mat" || ext == ".material") return "[MAT]";
    if (ext == ".prefab" || ext == ".nexusprefab") return "[PFB]";
    if (ext == ".anim") return "[ANIM]";
    if (ext == ".lua")  return "[LUA]";
    if (ext == ".glsl" || ext == ".vert" || ext == ".frag") return "[SHDR]";
    if (ext == ".nxs"  || ext == ".json") return "[SCN]";
    if (ext == ".pak")  return "[PAK]";
    return "[F]";
}

/// Per-extension tile color used when no real GPU thumbnail is bound — gives
/// the project view a Unity-like visual cue at a glance even without a fully
/// wired image decoder.  When `set_thumbnail()` registers a real texture the
/// panel renders that instead and ignores this fallback.
ImU32 asset_tile_color(const AssetBrowserEntry& entry) {
    if (entry.is_directory) return IM_COL32(110, 90, 50, 255);   // amber
    const auto& ext = entry.extension;
    if (ext == ".bmp" || ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
        ext == ".tga" || ext == ".hdr") return IM_COL32(80, 130, 70, 255);   // image green
    if (ext == ".obj" || ext == ".gltf" || ext == ".glb" ||
        ext == ".fbx" || ext == ".dae")  return IM_COL32(60, 95, 140, 255);  // mesh blue
    if (ext == ".wav" || ext == ".ogg" || ext == ".mp3")
                                       return IM_COL32(150, 70, 130, 255); // audio magenta
    if (ext == ".mat" || ext == ".material")
                                       return IM_COL32(70, 110, 95, 255);  // material teal
    if (ext == ".prefab" || ext == ".nexusprefab")
                                       return IM_COL32(120, 80, 140, 255); // prefab violet
    if (ext == ".anim") return IM_COL32(180, 130, 60, 255);                 // animation amber
    if (ext == ".lua") return IM_COL32(40, 80, 130, 255);
    if (ext == ".glsl" || ext == ".vert" || ext == ".frag")
                                       return IM_COL32(140, 100, 50, 255); // shader copper
    if (ext == ".nxs"  || ext == ".json")
                                       return IM_COL32(100, 100, 100, 255);// scene grey
    if (ext == ".pak")  return IM_COL32(70, 70, 90, 255);
    return IM_COL32(60, 60, 60, 255);
}

} // namespace (anonymous)

// ── AssetBrowserPanel: pure default-visual helpers (test-friendly) ──────────
//
// These statics live OUTSIDE the anonymous namespace so they're linkable
// from test TUs.  The bodies inline the same dispatch as `asset_icon` /
// `asset_tile_color` above — keeping them in sync with that table is a
// small duplication cost paid for testability of the visual contract.
const char* AssetBrowserPanel::default_icon_for(const std::string& ext) {
    if (ext == ".bmp" || ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
        ext == ".tga" || ext == ".hdr") return "[IMG]";
    if (ext == ".obj" || ext == ".gltf" || ext == ".glb" ||
        ext == ".fbx" || ext == ".dae")  return "[MESH]";
    if (ext == ".wav" || ext == ".ogg" || ext == ".mp3") return "[SFX]";
    if (ext == ".mat" || ext == ".material") return "[MAT]";
    if (ext == ".prefab" || ext == ".nexusprefab") return "[PFB]";
    if (ext == ".anim") return "[ANIM]";
    if (ext == ".lua")  return "[LUA]";
    if (ext == ".glsl" || ext == ".vert" || ext == ".frag") return "[SHDR]";
    if (ext == ".nxs"  || ext == ".json") return "[SCN]";
    if (ext == ".pak")  return "[PAK]";
    return "[F]";
}
u32 AssetBrowserPanel::default_tile_color_for(const std::string& ext) {
    if (ext == ".bmp" || ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
        ext == ".tga" || ext == ".hdr") return IM_COL32(80, 130, 70, 255);
    if (ext == ".obj" || ext == ".gltf" || ext == ".glb" ||
        ext == ".fbx" || ext == ".dae")  return IM_COL32(60, 95, 140, 255);
    if (ext == ".wav" || ext == ".ogg" || ext == ".mp3")
                                       return IM_COL32(150, 70, 130, 255);
    if (ext == ".mat" || ext == ".material") return IM_COL32(70, 110, 95, 255);
    if (ext == ".prefab" || ext == ".nexusprefab")
                                       return IM_COL32(120, 80, 140, 255);
    if (ext == ".anim") return IM_COL32(180, 130, 60, 255);
    if (ext == ".lua") return IM_COL32(40, 80, 130, 255);
    if (ext == ".glsl" || ext == ".vert" || ext == ".frag")
                                       return IM_COL32(140, 100, 50, 255);
    if (ext == ".nxs"  || ext == ".json")
                                       return IM_COL32(100, 100, 100, 255);
    if (ext == ".pak")  return IM_COL32(70, 70, 90, 255);
    return IM_COL32(60, 60, 60, 255);
}

namespace {

/// Format a byte count into a 6-char-ish human-readable string (e.g.
/// "  3 B", " 12 KB", "  1.4 MB").  Avoids std::to_string overhead in the
/// hot render loop.
void format_file_size(u64 bytes, char* out, size_t out_size) {
    if (bytes < 1024) {
        std::snprintf(out, out_size, "%llu B", static_cast<unsigned long long>(bytes));
    } else if (bytes < 1024ull * 1024ull) {
        std::snprintf(out, out_size, "%.1f KB",
                      static_cast<double>(bytes) / 1024.0);
    } else if (bytes < 1024ull * 1024ull * 1024ull) {
        std::snprintf(out, out_size, "%.1f MB",
                      static_cast<double>(bytes) / (1024.0 * 1024.0));
    } else {
        std::snprintf(out, out_size, "%.2f GB",
                      static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    }
}

} // namespace (anonymous)

void AssetBrowserPanel::on_render() {
    namespace ui = nexus::editor::imgui;
    namespace fs = std::filesystem;

    ui::begin_window("Asset Browser");

    // ── Navigation bar ──────────────────────────────────────────────────
    ImGui::BeginDisabled(!can_go_back());
    if (ImGui::SmallButton("<##back")) go_back();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!can_go_forward());
    if (ImGui::SmallButton(">##fwd")) go_forward();
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::SmallButton("^##up")) navigate_up();
    ImGui::SameLine();
    if (ImGui::SmallButton("Refresh")) {
        if (on_refresh_) on_refresh_();
        navigation_dirty_ = true;
    }
    ImGui::SameLine();

    bool is_grid = view_mode_ == ViewMode::Grid;
    if (ImGui::SmallButton(is_grid ? "Grid" : "List")) {
        view_mode_ = is_grid ? ViewMode::List : ViewMode::Grid;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Recursive", &recursive_search_);

    // ── Clickable breadcrumb trail ────────────────────────────────────
    // Each path component is a button that jumps to that ancestor.  Trims
    // to root_path_ so users can't accidentally navigate above the project.
    {
        const fs::path root  = root_path_.empty() ? fs::path{} : fs::path(root_path_);
        const fs::path here  = current_path_.empty() ? root : fs::path(current_path_);
        std::error_code ec;
        const fs::path rel   = root.empty() ? here
                                            : fs::relative(here, root, ec);
        if (!root.empty() && !ec) {
            // Root crumb.
            if (ImGui::SmallButton(root.filename().empty()
                                       ? root.string().c_str()
                                       : root.filename().string().c_str())) {
                navigate_to(root.string());
            }
            fs::path acc = root;
            for (const auto& seg : rel) {
                if (seg == "." || seg.empty()) continue;
                acc /= seg;
                ImGui::SameLine();
                ImGui::TextDisabled("/");
                ImGui::SameLine();
                ImGui::PushID(acc.string().c_str());
                if (ImGui::SmallButton(seg.string().c_str())) {
                    navigate_to(acc.string());
                }
                ImGui::PopID();
            }
        } else {
            ImGui::TextDisabled("%s",
                                current_path_.empty() ? "/" : current_path_.c_str());
        }
    }

    // ── Search box ────────────────────────────────────────────────────
    char search_buf[256];
    std::snprintf(search_buf, sizeof(search_buf), "%s", search_.c_str());
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputTextWithHint("##AssetSearch",
                                  recursive_search_
                                    ? "Search project (recursive)"
                                    : "Filter current folder",
                                  search_buf, sizeof(search_buf))) {
        search_ = search_buf;
    }
    ImGui::Separator();

    // ── Left sidebar: Favorites + virtual collections ──────────────────
    //
    // Mirrors Unity's project view rail (Favorites / All Materials / All
    // Models / All Prefabs).  Clicking a row sets current_filter_ and the
    // host responds by populating entries_ with the matching project-wide
    // list.  Width tracks ImGui's column splitter so users can resize.
    constexpr float kSidebarMin = 110.0f;
    constexpr float kSidebarMax = 320.0f;
    static float sidebar_width = 160.0f;
    if (sidebar_width < kSidebarMin) sidebar_width = kSidebarMin;
    if (sidebar_width > kSidebarMax) sidebar_width = kSidebarMax;

    auto sidebar_row = [&](const char* label, SidebarFilter target) {
        const bool active = (current_filter_ == target);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Header,        IM_COL32(90, 128, 181, 255));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(102, 145, 200, 255));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive,  IM_COL32(80, 116, 168, 255));
        }
        if (ImGui::Selectable(label, active)) {
            set_filter(target);
        }
        if (active) ImGui::PopStyleColor(3);
    };

    ImGui::BeginChild("AssetSidebar", ImVec2(sidebar_width, 0), true);
    {
        const bool assets_active = (current_filter_ == SidebarFilter::None);
        if (assets_active) {
            ImGui::PushStyleColor(ImGuiCol_Header,        IM_COL32(90, 128, 181, 255));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(102, 145, 200, 255));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive,  IM_COL32(80, 116, 168, 255));
        }
        if (ImGui::Selectable("Assets", assets_active)) {
            // Clear filter without changing the current_path_ — host rescans
            // the folder.  If the user is already in folder mode, treat this
            // as a no-op refresh.
            if (current_filter_ != SidebarFilter::None) {
                current_filter_ = SidebarFilter::None;
                navigation_dirty_ = true;
            }
        }
        if (assets_active) ImGui::PopStyleColor(3);

        ImGui::Spacing();
        ImGui::TextDisabled("Favorites");
        ImGui::Separator();
        char fav_label[64];
        std::snprintf(fav_label, sizeof(fav_label),
                      "[*] Favorites (%u)", favorite_count());
        sidebar_row(fav_label, SidebarFilter::Favorites);

        ImGui::Spacing();
        ImGui::TextDisabled("All");
        ImGui::Separator();
        sidebar_row("All Materials", SidebarFilter::AllMaterials);
        sidebar_row("All Models",    SidebarFilter::AllModels);
        sidebar_row("All Prefabs",   SidebarFilter::AllPrefabs);
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // ── Content area ────────────────────────────────────────────────────
    ImGui::BeginChild("AssetContent", ImVec2(0, 0), true);

    auto matches = [&](const AssetBrowserEntry& e) {
        return search_.empty() || icontains(e.name, search_);
    };

    // Cross-panel drag source — payload is the absolute path string.  Other
    // panels (Hierarchy, Viewport) accept the same payload type for asset
    // drag-into-scene workflows that match Unity's Project → Hierarchy/Game
    // drag.  Path is null-terminated and capped at 1KB to fit ImGui's payload
    // budget; longer paths are silently truncated, which would only happen on
    // pathological FS layouts.
    auto drag_source = [&](const AssetBrowserEntry& e) {
        if (e.is_directory) return;
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            char buf[1024];
            std::snprintf(buf, sizeof(buf), "%s", e.path.c_str());
            ImGui::SetDragDropPayload("NEXUS_ASSET_PATH", buf, sizeof(buf));
            ImGui::TextUnformatted(e.name.c_str());
            ImGui::EndDragDropSource();
        }
    };

    auto context_menu = [&](const AssetBrowserEntry& e) {
        if (ImGui::BeginPopupContextItem()) {
            ImGui::TextDisabled("%s", e.name.c_str());
            ImGui::Separator();
            if (e.is_directory) {
                if (ImGui::MenuItem("Open")) navigate_to(e.path);
            } else {
                if (ImGui::MenuItem("Open") && on_open_) on_open_(e);
            }
            if (ImGui::MenuItem("Reveal in File Manager") && on_reveal_) {
                on_reveal_(e);
            }
            if (ImGui::MenuItem("Copy Path")) {
                ImGui::SetClipboardText(e.path.c_str());
            }
            // Favorites toggle — only meaningful for files, but we let the
            // user mark folders too so the membership matches Unity's
            // free-form Favorites list.
            ImGui::Separator();
            const bool fav = is_favorite(e.path);
            if (ImGui::MenuItem(fav ? "Remove from Favorites"
                                    : "Add to Favorites")) {
                if (fav) remove_favorite(e.path);
                else     add_favorite(e.path);
                if (current_filter_ == SidebarFilter::Favorites) {
                    navigation_dirty_ = true;
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete") && on_delete_) on_delete_(e);
            ImGui::EndPopup();
        }
    };

    if (view_mode_ == ViewMode::Grid) {
        const float panel_width = ImGui::GetContentRegionAvail().x;
        const float cell_size   = static_cast<float>(thumbnail_size_) + 16.0f;
        u32 columns = static_cast<u32>(panel_width / cell_size);
        if (columns < 1) columns = 1;

        u32 col = 0;
        for (const auto& entry : entries_) {
            if (!matches(entry)) continue;

            const char* icon = asset_icon(entry);
            char label[512];
            std::snprintf(label, sizeof(label), "%s\n%s##%s",
                          icon, entry.name.c_str(), entry.path.c_str());

            const bool is_selected = (selected_ == entry.path);
            const ImVec2 cell(static_cast<float>(thumbnail_size_),
                              static_cast<float>(thumbnail_size_));
            ImGui::PushID(entry.path.c_str());

            // Draw a colored tile (or real thumbnail when bound) behind the
            // selectable so the project view feels like Unity's grid even
            // before a real GPU image decoder lands.  Real thumbnails plug in
            // via set_thumbnail() with no further panel changes.
            const ImVec2 cur = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const std::uintptr_t tex = thumbnail(entry.path);
            if (tex != 0u) {
                dl->AddImage(static_cast<ImTextureID>(tex), cur,
                             ImVec2(cur.x + cell.x, cur.y + cell.y));
            } else {
                ImU32 base = asset_tile_color(entry);
                // Material content-aware tint — when a sampler is bound and
                // the file is a material, paint the tile in the actual
                // albedo so the user sees a Unity-style at-a-glance read.
                // Sampler returning false (parse failed / not a material)
                // falls back to the extension tint.
                const auto& ext = entry.extension;
                if (material_color_sampler_ &&
                    (ext == ".mat" || ext == ".material")) {
                    f32 c[4]{};
                    if (material_color_sampler_(entry.path, c)) {
                        const u8 r = static_cast<u8>(std::clamp(c[0], 0.0f, 1.0f) * 255.0f);
                        const u8 g = static_cast<u8>(std::clamp(c[1], 0.0f, 1.0f) * 255.0f);
                        const u8 b = static_cast<u8>(std::clamp(c[2], 0.0f, 1.0f) * 255.0f);
                        const u8 a = static_cast<u8>(std::clamp(c[3], 0.0f, 1.0f) * 255.0f);
                        base = IM_COL32(r, g, b, a == 0 ? 255 : a);
                    }
                }
                dl->AddRectFilled(cur,
                                  ImVec2(cur.x + cell.x, cur.y + cell.y),
                                  base, 4.0f);
                dl->AddRect(cur,
                            ImVec2(cur.x + cell.x, cur.y + cell.y),
                            IM_COL32(0, 0, 0, 110), 4.0f);

                // Type-specific overlays — small cues that fit inside the
                // cell so users distinguish prefabs / animations from
                // generic colored tiles even when no real thumbnail is
                // bound yet.
                if (ext == ".prefab" || ext == ".nexusprefab") {
                    // Cube-outline glyph centred near the bottom.
                    const float pad = std::min(cell.x, cell.y) * 0.18f;
                    const ImVec2 a(cur.x + pad,           cur.y + pad);
                    const ImVec2 b(cur.x + cell.x - pad,  cur.y + pad);
                    const ImVec2 c(cur.x + cell.x - pad,  cur.y + cell.y - pad);
                    const ImVec2 d(cur.x + pad,           cur.y + cell.y - pad);
                    const ImU32 line = IM_COL32(255, 255, 255, 200);
                    dl->AddLine(a, b, line, 1.5f);
                    dl->AddLine(b, c, line, 1.5f);
                    dl->AddLine(c, d, line, 1.5f);
                    dl->AddLine(d, a, line, 1.5f);
                    dl->AddLine(a, c, line, 1.0f);
                    dl->AddLine(b, d, line, 1.0f);
                } else if (ext == ".anim") {
                    // Sine-wave glyph along the bottom strip — quick visual
                    // metaphor for "this is animated data" without sampling
                    // the clip's actual tracks.
                    constexpr int kSegs = 16;
                    const float strip_h = cell.y * 0.30f;
                    const float strip_y = cur.y + cell.y - strip_h - 4.0f;
                    const ImU32 line = IM_COL32(255, 240, 200, 220);
                    ImVec2 prev(cur.x + 4.0f, strip_y + strip_h * 0.5f);
                    for (int i = 1; i <= kSegs; ++i) {
                        const float t = static_cast<float>(i) /
                                        static_cast<float>(kSegs);
                        const float x = cur.x + 4.0f + (cell.x - 8.0f) * t;
                        const float y = strip_y + strip_h * 0.5f -
                            std::sin(t * 6.2832f) * strip_h * 0.4f;
                        const ImVec2 next(x, y);
                        dl->AddLine(prev, next, line, 1.5f);
                        prev = next;
                    }
                }
            }

            if (ImGui::Selectable(label, is_selected,
                                  ImGuiSelectableFlags_AllowDoubleClick |
                                  ImGuiSelectableFlags_AllowOverlap, cell)) {
                selected_ = entry.path;
                if (ImGui::IsMouseDoubleClicked(0)) {
                    if (entry.is_directory)      navigate_to(entry.path);
                    else if (on_open_)           on_open_(entry);
                }
            }
            drag_source(entry);
            context_menu(entry);
            ImGui::PopID();

            ++col;
            if (col < columns) ImGui::SameLine();
            else                col = 0;
        }
    } else {
        if (ImGui::BeginTable("AssetBrowserList", 3,
                              ImGuiTableFlags_Borders |
                              ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Size",
                                    ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("Type",
                                    ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableHeadersRow();

            for (const auto& entry : entries_) {
                if (!matches(entry)) continue;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                const bool is_selected = (selected_ == entry.path);
                char row_label[512];
                std::snprintf(row_label, sizeof(row_label), "%s %s##row-%s",
                              asset_icon(entry), entry.name.c_str(),
                              entry.path.c_str());
                if (ImGui::Selectable(row_label, is_selected,
                                      ImGuiSelectableFlags_SpanAllColumns |
                                      ImGuiSelectableFlags_AllowDoubleClick)) {
                    selected_ = entry.path;
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        if (entry.is_directory)  navigate_to(entry.path);
                        else if (on_open_)       on_open_(entry);
                    }
                }
                drag_source(entry);
                context_menu(entry);

                ImGui::TableNextColumn();
                if (entry.is_directory) {
                    ImGui::TextDisabled("--");
                } else {
                    char sz[32];
                    format_file_size(entry.file_size, sz, sizeof(sz));
                    ImGui::TextUnformatted(sz);
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(entry.is_directory
                                          ? "dir"
                                          : (entry.extension.empty()
                                                 ? "file"
                                                 : entry.extension.c_str() + 1));
            }
            ImGui::EndTable();
        }
    }

    ImGui::EndChild();

    // ── Footer ─────────────────────────────────────────────────────────
    ImGui::Separator();
    if (!selected_.empty()) {
        ImGui::TextDisabled("Selected: %s", selected_.c_str());
    } else {
        ImGui::TextDisabled("%zu items", entries_.size());
    }
    if (view_mode_ == ViewMode::Grid) {
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 140.0f);
        int tn = static_cast<int>(thumbnail_size_);
        ImGui::SetNextItemWidth(140.0f);
        if (ImGui::SliderInt("##Thumb", &tn, 32, 256, "Thumb %d")) {
            thumbnail_size_ = static_cast<u32>(tn);
        }
    }

    ui::end_window();
}

void AssetBrowserPanel::navigate_to(const std::string& path) {
    namespace fs = std::filesystem;

    // Cap navigation at root_path_ (if set) so the user can never see content
    // above the project — Unity's Project window enforces the same boundary.
    std::string clamped = path;
    if (!root_path_.empty()) {
        std::error_code ec;
        const fs::path target = fs::weakly_canonical(fs::path(path), ec);
        const fs::path root   = fs::weakly_canonical(fs::path(root_path_), ec);
        if (!ec && !target.empty() && !root.empty()) {
            // mismatch → climb up; if we ended up above root, snap to root.
            const auto rel = fs::relative(target, root, ec);
            if (ec || rel.empty() || rel.string().rfind("..", 0) == 0) {
                clamped = root.string();
            } else {
                clamped = target.string();
            }
        }
    }

    if (history_index_ >= 0 &&
        history_index_ + 1 < static_cast<i32>(history_.size())) {
        history_.erase(history_.begin() + history_index_ + 1, history_.end());
    }
    history_.push_back(clamped);
    history_index_ = static_cast<i32>(history_.size()) - 1;
    current_path_ = clamped;
    // Returning to a folder clears any virtual sidebar filter so the host's
    // next rescan reads from the folder again rather than the filter.
    current_filter_ = SidebarFilter::None;
    navigation_dirty_ = true;
}

void AssetBrowserPanel::set_filter(SidebarFilter f) {
    if (current_filter_ == f && f != SidebarFilter::None) {
        // Re-clicking the same active filter is treated as a refresh request,
        // matching Unity's behavior — fire the callback so the host can re-
        // populate (e.g. after the user added a new material on disk).
        if (on_filter_request_) on_filter_request_(f);
        navigation_dirty_ = true;
        return;
    }
    current_filter_ = f;
    navigation_dirty_ = true;
    if (f != SidebarFilter::None && on_filter_request_) {
        on_filter_request_(f);
    }
}

void AssetBrowserPanel::navigate_up() {
    namespace fs = std::filesystem;
    if (current_path_.empty()) return;
    const fs::path parent = fs::path(current_path_).parent_path();
    if (parent.empty()) return;
    // navigate_to enforces the root cap, so we don't need to here.
    navigate_to(parent.string());
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

// ── UndoHistoryPanel ───────────────────────────────────────────────────────

void UndoHistoryPanel::on_render() {
    if (!visible_) return;
    if (!ImGui::Begin(title_.c_str(), &visible_)) {
        ImGui::End();
        return;
    }
    if (!mgr_) {
        ImGui::TextUnformatted("No undo manager bound.");
        ImGui::End();
        return;
    }

    const u32 undo_n = mgr_->undo_count();
    const u32 redo_n = mgr_->redo_count();
    const u32 total  = undo_n + redo_n;
    const bool dirty = mgr_->is_dirty();

    ImGui::Text("Applied: %u   Redo: %u   Total: %u   %s",
                undo_n, redo_n, total, dirty ? "[modified]" : "[saved]");
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) {
        mgr_->clear();
        ImGui::End();
        return;
    }
    ImGui::Separator();

    // Row 0 = "Initial state" (empty stack).  We render in chronological order
    // so the list reads top→bottom like Unity: oldest first, most-recent last,
    // with the redo stack appearing beneath the current position greyed out.
    ImGui::BeginChild("##undo-history-list", ImVec2(0, 0), true);

    // Anchor row — represents the pristine state before anything was done.
    {
        ImGui::PushID("row-initial");
        const bool current = (undo_n == 0);
        if (ImGui::Selectable("[ Initial State ]", current,
                              ImGuiSelectableFlags_AllowDoubleClick)) {
            mgr_->jump_to_undo(0);
        }
        ImGui::PopID();
    }

    // Applied commands (undo_stack_ oldest→newest ≡ undo_history() reversed).
    // undo_history() returns newest→oldest; walk backwards to get oldest→newest.
    const auto applied = mgr_->undo_history();
    for (u32 i = 0; i < applied.size(); ++i) {
        // Row index within applied: 0..undo_n-1. Target-after-click is i+1
        // (i.e. the state *after* this command has been applied).
        const u32 display_idx = static_cast<u32>(applied.size() - 1 - i);
        const std::string& desc = applied[display_idx];
        const u32 target = display_idx + 1;
        const bool current = (target == undo_n);
        char label[256];
        std::snprintf(label, sizeof(label), "%s%u. %s",
                      current ? "> " : "  ", target, desc.c_str());
        // Push an integer-scoped ID so duplicate descriptions (e.g. repeated
        // "Move entity" commands) never collide inside ImGui's ID stack.
        ImGui::PushID(static_cast<int>(target));
        if (ImGui::Selectable(label, current,
                              ImGuiSelectableFlags_AllowDoubleClick)) {
            mgr_->jump_to_undo(target);
        }
        ImGui::PopID();
    }

    // Redo commands — shown greyed out below the cursor.
    if (redo_n > 0) {
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.55f, 1.0f));
        const auto pending = mgr_->redo_history();
        // redo_history() returns top-of-stack first = next redo first; we want
        // natural chronological order, so it already reads correctly here.
        for (u32 i = 0; i < pending.size(); ++i) {
            const u32 target = undo_n + i + 1;
            char label[256];
            std::snprintf(label, sizeof(label), "  %u. %s",
                          target, pending[i].c_str());
            ImGui::PushID(static_cast<int>(target));
            if (ImGui::Selectable(label, false,
                                  ImGuiSelectableFlags_AllowDoubleClick)) {
                mgr_->jump_to_undo(target);
            }
            ImGui::PopID();
        }
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::End();
}

// ── ProfilerPanel ──────────────────────────────────────────────────────────

void ProfilerPanel::on_render() {
    if (!visible_) return;
    if (!ImGui::Begin(title_.c_str(), &visible_)) {
        ImGui::End();
        return;
    }
    if (!profiler_) {
        ImGui::TextUnformatted("No profiler bound.");
        ImGui::End();
        return;
    }

    bool paused = paused_;
    if (ImGui::Checkbox("Pause", &paused)) {
        paused_ = paused;
        if (paused_) {
            // Freeze both the histogram source AND the latest frame sample
            // table at the same instant so the chart and the breakdown show
            // the same data — matches Unity's Profiler pause behavior.
            frozen_frame_times_ms_.clear();
            for (const auto& f : profiler_->history()) {
                frozen_frame_times_ms_.push_back(
                    static_cast<f32>(f.total_cpu_us / 1000.0));
            }
            if (const FrameProfile* lf = profiler_->last_frame()) {
                frozen_last_frame_ = *lf;
                frozen_has_frame_  = true;
            } else {
                frozen_has_frame_ = false;
            }
        } else {
            frozen_frame_times_ms_.clear();
            frozen_has_frame_ = false;
        }
    }
    ImGui::SameLine();
    bool active = profiler_->is_active();
    if (ImGui::Checkbox("Active", &active)) {
        profiler_->set_active(active);
    }
    ImGui::SameLine();
    ImGui::Text("Avg CPU: %.2f ms   Avg GPU: %.2f ms",
                profiler_->average_cpu_us() / 1000.0,
                profiler_->average_gpu_us() / 1000.0);

    // ── Frame time histogram ─────────────────────────────────────────
    const auto& hist = profiler_->history();
    std::vector<f32> buffer;
    const std::vector<f32>* src = nullptr;
    if (paused_ && !frozen_frame_times_ms_.empty()) {
        src = &frozen_frame_times_ms_;
    } else {
        buffer.reserve(hist.size());
        for (const auto& f : hist) {
            buffer.push_back(static_cast<f32>(f.total_cpu_us / 1000.0));
        }
        src = &buffer;
    }

    if (!src->empty()) {
        f32 max_ms = 0.0f;
        for (f32 v : *src) max_ms = std::max(max_ms, v);
        // Leave at least a 16.67 ms (60 FPS) headroom so the bar height is
        // comparable across frames and a smooth scene doesn't look noisy.
        const f32 scale_max = std::max(max_ms, 16.67f);
        char overlay[64];
        std::snprintf(overlay, sizeof(overlay), "max %.2f ms", max_ms);
        ImGui::PlotHistogram("##cpu-frames", src->data(),
                             static_cast<int>(src->size()),
                             0, overlay, 0.0f, scale_max,
                             ImVec2(-1.0f, 100.0f));
    } else {
        ImGui::TextDisabled("No frame history yet.");
    }

    // ── Latest-frame CPU sample list ────────────────────────────────
    ImGui::Separator();
    ImGui::TextUnformatted(paused_ ? "Frozen frame CPU samples:"
                                   : "Latest frame CPU samples:");
    const FrameProfile* last =
        (paused_ && frozen_has_frame_) ? &frozen_last_frame_
                                       : profiler_->last_frame();
    if (!last || last->cpu_samples.empty()) {
        ImGui::TextDisabled("(no samples)");
    } else {
        if (ImGui::BeginTable("##cpu-samples", 3,
                              ImGuiTableFlags_Borders |
                              ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_ScrollY,
                              ImVec2(0, 180))) {
            ImGui::TableSetupColumn("Scope");
            ImGui::TableSetupColumn("Time (ms)",
                                    ImGuiTableColumnFlags_WidthFixed, 90);
            ImGui::TableSetupColumn("%",
                                    ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableHeadersRow();
            const f64 total = last->total_cpu_us > 0.0 ? last->total_cpu_us : 1.0;
            for (const auto& s : last->cpu_samples) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                // Indent nested scopes by depth for the tree illusion.
                for (u32 i = 0; i < s.depth; ++i) ImGui::Indent(8.0f);
                ImGui::TextUnformatted(s.name.c_str());
                for (u32 i = 0; i < s.depth; ++i) ImGui::Unindent(8.0f);
                ImGui::TableNextColumn();
                ImGui::Text("%.3f", s.duration_us / 1000.0);
                ImGui::TableNextColumn();
                ImGui::Text("%5.1f", 100.0 * s.duration_us / total);
            }
            ImGui::EndTable();
        }
    }

    ImGui::End();
}

// ── AnimationPanel ─────────────────────────────────────────────────────────

f32 AnimationPanel::time_to_x(f32 t, f32 view_x_min, f32 view_x_max,
                              f32 duration) {
    if (duration <= 0.0f || view_x_max <= view_x_min) return view_x_min;
    if (t < 0.0f)        t = 0.0f;
    if (t > duration)    t = duration;
    const f32 frac = t / duration;
    return view_x_min + frac * (view_x_max - view_x_min);
}

f32 AnimationPanel::x_to_time(f32 x, f32 view_x_min, f32 view_x_max,
                              f32 duration) {
    if (duration <= 0.0f || view_x_max <= view_x_min) return 0.0f;
    if (x < view_x_min) x = view_x_min;
    if (x > view_x_max) x = view_x_max;
    const f32 frac = (x - view_x_min) / (view_x_max - view_x_min);
    return frac * duration;
}

u32 AnimationPanel::clip_total_keys(const nexus::anim::AnimationClip& clip) {
    u32 n = 0;
    for (const auto& ch : clip.channels()) {
        n += static_cast<u32>(ch.positions.size());
        n += static_cast<u32>(ch.rotations.size());
        n += static_cast<u32>(ch.scales.size());
    }
    return n;
}

void AnimationPanel::on_render() {
    if (!visible_) return;
    if (!ImGui::Begin(title_.c_str(), &visible_)) {
        ImGui::End();
        return;
    }

    const nexus::anim::AnimationClip* clip =
        (resolver_ && clip_id_ != 0u) ? resolver_(clip_id_) : nullptr;

    // Header — clip name + duration + counts.  Always visible so the user
    // sees why the timeline below is empty when no clip is bound.
    if (!clip) {
        ImGui::TextDisabled("No clip bound.  Drop a .anim onto an "
                             "AnimatorComponent or select an animator.");
        ImGui::End();
        return;
    }
    ImGui::Text("%s   duration=%.2fs   keys=%u   channels=%zu",
                clip->name().empty() ? "(unnamed)" : clip->name().c_str(),
                static_cast<double>(clip->duration()),
                clip_total_keys(*clip),
                clip->channels().size());

    // Zoom + scrub controls.
    ImGui::SetNextItemWidth(120.0f);
    f32 z = zoom_;
    if (ImGui::SliderFloat("Zoom (px/s)##anim", &z, 16.0f, 4096.0f,
                            "%.0f", ImGuiSliderFlags_Logarithmic)) {
        set_zoom(z);
    }
    ImGui::SameLine();
    f32 ph = playhead_;
    if (clip->duration() > 0.0f) {
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::SliderFloat("Time##anim", &ph, 0.0f, clip->duration(),
                                "%.3fs")) {
            set_playhead(ph);
        }
    }
    ImGui::Separator();

    // ── Dopesheet area ─────────────────────────────────────────────────
    //
    // One row per channel × component (positions / rotations / scales).
    // Keyframes drawn as small diamonds at their `time` position; playhead
    // drawn as a vertical line.  Clicking inside the timeline scrubs the
    // playhead — write-back goes through set_playhead so subclasses can
    // hook the value if needed.
    constexpr float kRowHeight   = 18.0f;
    constexpr float kLabelWidth  = 130.0f;
    constexpr float kTopPadding  = 4.0f;
    const f32 duration = clip->duration();
    if (duration <= 0.0f) {
        ImGui::TextDisabled("Clip has zero duration — no timeline to draw.");
        ImGui::End();
        return;
    }

    const u32 row_count = static_cast<u32>(clip->channels().size()) * 3u;
    const float content_h = static_cast<float>(row_count) * kRowHeight + 24.0f;
    ImGui::BeginChild("##AnimDopesheet",
                       ImVec2(0, std::max(content_h, 64.0f)), true,
                       ImGuiWindowFlags_HorizontalScrollbar);

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float track_x_min = origin.x + kLabelWidth;
    const float track_x_max = track_x_min + duration * zoom_;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 1-second gridlines so users can read the timeline at a glance.
    const ImU32 grid_col = IM_COL32(80, 80, 80, 200);
    for (i32 s = 0; s <= static_cast<i32>(duration) + 1; ++s) {
        const f32 x = time_to_x(static_cast<f32>(s),
                                track_x_min, track_x_max, duration);
        dl->AddLine(ImVec2(x, origin.y),
                    ImVec2(x, origin.y + content_h),
                    grid_col, 1.0f);
    }

    // Per-channel rows.
    auto draw_keys = [&](float row_y, ImU32 color, const auto& keys) {
        for (const auto& k : keys) {
            const f32 cx = time_to_x(k.time, track_x_min, track_x_max, duration);
            const f32 cy = row_y + kRowHeight * 0.5f;
            const f32 r  = 5.0f;
            // Diamond: 4 line segments around centre (cx, cy).
            const ImVec2 p0(cx,     cy - r);
            const ImVec2 p1(cx + r, cy);
            const ImVec2 p2(cx,     cy + r);
            const ImVec2 p3(cx - r, cy);
            dl->AddQuadFilled(p0, p1, p2, p3, color);
            dl->AddQuad(p0, p1, p2, p3, IM_COL32(0, 0, 0, 200), 1.0f);
        }
    };

    u32 row = 0;
    for (const auto& ch : clip->channels()) {
        char label[64];
        std::snprintf(label, sizeof(label), "Bone %d  Pos", ch.bone_index);
        const float row_y = origin.y + kTopPadding + static_cast<f32>(row) * kRowHeight;
        ImGui::SetCursorScreenPos(ImVec2(origin.x + 4.0f, row_y));
        ImGui::TextDisabled("%s", label);
        draw_keys(row_y, IM_COL32(120, 200, 120, 230), ch.positions);
        ++row;

        std::snprintf(label, sizeof(label), "Bone %d  Rot", ch.bone_index);
        const float row_yr = origin.y + kTopPadding + static_cast<f32>(row) * kRowHeight;
        ImGui::SetCursorScreenPos(ImVec2(origin.x + 4.0f, row_yr));
        ImGui::TextDisabled("%s", label);
        draw_keys(row_yr, IM_COL32(220, 180, 100, 230), ch.rotations);
        ++row;

        std::snprintf(label, sizeof(label), "Bone %d  Scl", ch.bone_index);
        const float row_ys = origin.y + kTopPadding + static_cast<f32>(row) * kRowHeight;
        ImGui::SetCursorScreenPos(ImVec2(origin.x + 4.0f, row_ys));
        ImGui::TextDisabled("%s", label);
        draw_keys(row_ys, IM_COL32(180, 150, 220, 230), ch.scales);
        ++row;
    }

    // Playhead vertical line drawn last so it overlays diamonds.
    const f32 ph_x = time_to_x(playhead_, track_x_min, track_x_max, duration);
    dl->AddLine(ImVec2(ph_x, origin.y),
                ImVec2(ph_x, origin.y + content_h),
                IM_COL32(255, 60, 60, 230), 2.0f);

    // Click-to-scrub on the track area — only fires when the mouse is
    // inside the timeline x-range, so clicks on row labels don't move
    // the playhead.
    const ImVec2 mouse = ImGui::GetMousePos();
    if (ImGui::IsWindowHovered() && ImGui::IsMouseDown(0) &&
        mouse.x >= track_x_min && mouse.x <= track_x_max &&
        mouse.y >= origin.y    && mouse.y <= origin.y + content_h) {
        set_playhead(x_to_time(mouse.x, track_x_min, track_x_max, duration));
    }

    // Reserve dummy space so ImGui's child sizing accounts for the
    // dopesheet (which we drew via the draw list directly).
    ImGui::Dummy(ImVec2(track_x_max - origin.x + 16.0f, content_h));
    ImGui::EndChild();

    ImGui::End();
}

} // namespace nexus::editor
