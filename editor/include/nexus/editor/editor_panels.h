#pragma once

#include "nexus/editor/panel.h"
#include "nexus/core/math.h"
#include "nexus/rhi/rhi_types.h"
#include "nexus/perf/profiler.h"
#include <algorithm>
#include <vector>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <string>

namespace nexus {
class Registry;
class Scene;
class ForwardRenderer3D;
class BatchRenderer2D;
class DebugRenderer;
struct Mesh;
namespace rhi  { class RHI; }
namespace anim { class AnimationClip; class ParticleEmitterSystem; }
}

namespace nexus::editor {
class UndoRedoManager;
}

namespace nexus::editor {

// ─────────────────────────────────────────────────────────────────────────────
// ViewportPanel — 3D/2D scene viewport with gizmo controls
// ─────────────────────────────────────────────────────────────────────────────

enum class GizmoMode : u8 {
    None,
    Translate,
    Rotate,
    Scale
};

enum class GizmoSpace : u8 {
    Local,
    World
};

/// What camera the viewport uses to draw the scene.
///   SceneView — editor's own free-orbit camera (always renders, like Unity's
///               Scene tab).  Shows the world even outside Play mode.
///   GameView  — uses the scene's first CameraComponent (the "main camera"),
///               like Unity's Game tab.  Renders only when a runtime camera
///               exists; in stop mode the scene is paused but still drawn so
///               the user can see what the game would look like.
enum class ViewportCameraMode : u8 {
    SceneView,
    GameView,
};

/// Aspect ratio presets for the Game viewport.  "Free" matches the panel's
/// dock size; fixed ratios letterbox or pillarbox the rendered image so the
/// developer can preview how gameplay will look at a target resolution.
enum class GameAspect : u8 {
    Free,
    R16x9,
    R16x10,
    R4x3,
    R1x1,
};

/// Scene-view shading mode.  Mirrors Unity's Scene tab "Shaded" dropdown:
///   Shaded         — full lit material rendering (default)
///   Wireframe      — draw geometry as lines only
///   ShadedWireframe— shaded fill plus wireframe overlay
///   Albedo         — debug view ignoring lighting (tint only)
///   Normals        — debug view tinting by world normal (renderer-side TODO)
///   Overdraw       — debug view colouring by overdraw count (renderer TODO)
/// Renderers that haven't implemented a debug view treat it as Shaded.
enum class SceneShadingMode : u8 {
    Shaded,
    Wireframe,
    ShadedWireframe,
    Albedo,
    Normals,
    Overdraw,
};

/// Pre-canned camera viewpoints.  Selecting one snaps the editor camera
/// onto that orthographic-style framing while keeping the existing target.
/// Free is the user-driven orbit camera.
enum class SceneCameraPreset : u8 {
    Free,
    Top,
    Bottom,
    Front,
    Back,
    Left,
    Right,
    Iso,
};

/// Per-frame render statistics surfaced to the Game viewport's Stats overlay.
/// Populated by ViewportPanel::render_scene_to_fbo() at the end of each draw
/// pass — kept as a struct so future renderers can fill the same fields and
/// the panel UI is decoupled from any specific renderer.
struct ViewportRenderStats {
    u32 draw_calls{0};       // total submit_geometry / draw_mesh / draw_quad calls
    u32 mesh_draw_calls{0};
    u32 sprite_draw_calls{0};
    u32 triangles{0};        // sum of (mesh.indices.size() / 3) for drawn meshes
    u32 vertices{0};         // sum of mesh.vertices.size() for drawn meshes
    u32 set_pass_calls{0};   // RHI shader/pipeline binds (proxy for Unity's metric)
};

class ViewportPanel : public Panel {
public:
    explicit ViewportPanel(const std::string& title = "Viewport",
                           ViewportCameraMode mode = ViewportCameraMode::SceneView)
        : Panel(title), camera_mode_(mode) {}
    ~ViewportPanel() override;

    void on_render() override;
    const char* type_id() const override { return "ViewportPanel"; }

    /// Bind scene and renderers for viewport rendering.
    void bind_scene(Scene* scene) { scene_ = scene; }
    void bind_renderer_3d(ForwardRenderer3D* r) { renderer_3d_ = r; }
    void bind_renderer_2d(BatchRenderer2D* r) { renderer_2d_ = r; }
    void bind_debug_renderer(DebugRenderer* d) { debug_renderer_ = d; }
    void bind_rhi(nexus::rhi::RHI* rhi) { rhi_ = rhi; }
    /// Bind the live ParticleEmitterSystem so the viewport can draw the
    /// per-entity Particle records the system owns.  When unbound the
    /// viewport simply skips the particle pass — the system continues
    /// to tick from SceneBridge.simulate either way.
    void bind_particle_system(::nexus::anim::ParticleEmitterSystem* p) {
        particle_system_ = p;
    }

    /// Register a CPU-side mesh under a numeric id so MeshRendererComponent
    /// records can resolve it during the viewport's render pass.  Ownership of
    /// the underlying mesh stays with the caller — the panel only stores the
    /// pointer.  Pass nullptr to remove a registration.
    void register_mesh(u32 mesh_id, Mesh* mesh) {
        if (mesh) mesh_registry_[mesh_id] = mesh;
        else      mesh_registry_.erase(mesh_id);
    }
    /// Test introspection — returns nullptr when unregistered.  Production
    /// code paths still go through the panel's internal registry walks; this
    /// is the single accessor that exposes them for verification.
    Mesh* find_registered_mesh(u32 mesh_id) const {
        auto it = mesh_registry_.find(mesh_id);
        return it == mesh_registry_.end() ? nullptr : it->second;
    }
    u32 registered_mesh_count() const {
        return static_cast<u32>(mesh_registry_.size());
    }

    GizmoMode gizmo_mode() const { return gizmo_mode_; }
    void set_gizmo_mode(GizmoMode mode) { gizmo_mode_ = mode; }

    GizmoSpace gizmo_space() const { return gizmo_space_; }
    void set_gizmo_space(GizmoSpace space) { gizmo_space_ = space; }
    void toggle_gizmo_space() {
        gizmo_space_ = (gizmo_space_ == GizmoSpace::Local)
            ? GizmoSpace::World : GizmoSpace::Local;
    }

    bool is_grid_visible() const { return show_grid_; }
    void set_grid_visible(bool v) { show_grid_ = v; }

    bool is_axes_visible() const { return show_axes_; }
    void set_axes_visible(bool v) { show_axes_ = v; }

    bool is_gizmo_active() const { return gizmo_active_; }
    void set_gizmo_active(bool a) { gizmo_active_ = a; }

    void set_viewport_size(u32 w, u32 h) { width_ = w; height_ = h; }
    u32 viewport_width() const { return width_; }
    u32 viewport_height() const { return height_; }

    ViewportCameraMode camera_mode() const { return camera_mode_; }
    void set_camera_mode(ViewportCameraMode mode) { camera_mode_ = mode; }

    // ── SceneView toolbar state ──────────────────────────────────────────
    SceneShadingMode shading_mode() const { return shading_mode_; }
    void set_shading_mode(SceneShadingMode m) { shading_mode_ = m; }

    bool view_2d() const { return view_2d_; }
    void set_view_2d(bool b) { view_2d_ = b; }

    bool show_scene_gizmos() const { return show_scene_gizmos_; }
    void set_show_scene_gizmos(bool b) { show_scene_gizmos_ = b; }

    /// Snap the editor camera to a Unity-style preset.  `Free` is a no-op.
    /// All presets keep the camera looking at the current orbit target so
    /// the user doesn't lose focus on the selected entity.
    void apply_camera_preset(SceneCameraPreset p);

    /// Pure helper — yaw / pitch (in degrees) that a preset corresponds to.
    /// Exposed for tests.  Returns false for `Free` (no change).
    static bool preset_to_yaw_pitch(SceneCameraPreset p,
                                    f32& out_yaw, f32& out_pitch);

    // ── GameView toolbar state ───────────────────────────────────────────
    GameAspect game_aspect() const { return game_aspect_; }
    void set_game_aspect(GameAspect a) { game_aspect_ = a; }

    bool show_stats() const { return show_stats_; }
    void set_show_stats(bool b) { show_stats_ = b; }

    bool show_game_gizmos() const { return show_game_gizmos_; }
    void set_show_game_gizmos(bool b) { show_game_gizmos_ = b; }

    u8 display_index() const { return display_index_; }
    void set_display_index(u8 i) { display_index_ = i > 3u ? 3u : i; }

    f32 game_scale() const { return game_scale_; }
    void set_game_scale(f32 s) {
        if (s < 1.0f) s = 1.0f;
        if (s > 5.0f) s = 5.0f;
        game_scale_ = s;
    }

    /// Last frame's render stats — populated by render_scene_to_fbo().
    /// Tests that don't run the renderer can poke this directly.
    const ViewportRenderStats& last_render_stats() const { return last_stats_; }
    void set_last_render_stats(const ViewportRenderStats& s) { last_stats_ = s; }

    /// Aspect ratio for an aspect preset, in (width / height).  Returns 0
    /// for Free (no constraint).  Pure helper so callers and tests share the
    /// same source of truth.
    static f32 aspect_ratio_value(GameAspect a) {
        switch (a) {
            case GameAspect::R16x9:  return 16.0f / 9.0f;
            case GameAspect::R16x10: return 16.0f / 10.0f;
            case GameAspect::R4x3:   return 4.0f  / 3.0f;
            case GameAspect::R1x1:   return 1.0f;
            default:                 return 0.0f;
        }
    }

    /// Compute the letterboxed render size that fits inside an avail region.
    /// Returns (avail_w, avail_h) when aspect is Free.  Otherwise picks the
    /// larger dimension that matches the target ratio.  Pure function so it
    /// can be unit-tested without ImGui.
    static void compute_letterbox(f32 avail_w, f32 avail_h, GameAspect aspect,
                                  f32 scale, f32& out_w, f32& out_h) {
        if (avail_w <= 0.0f || avail_h <= 0.0f) {
            out_w = out_h = 0.0f; return;
        }
        if (scale < 1.0f) scale = 1.0f;
        const f32 ratio = aspect_ratio_value(aspect);
        f32 fit_w = avail_w;
        f32 fit_h = avail_h;
        if (ratio > 0.0f) {
            if (avail_w / avail_h > ratio) {
                fit_h = avail_h;
                fit_w = avail_h * ratio;
            } else {
                fit_w = avail_w;
                fit_h = avail_w / ratio;
            }
        }
        // Apply scale, but never exceed the dock area — scale > 1 lets the
        // user zoom INTO the image, but the visible region is still capped.
        out_w = std::min(fit_w * scale, avail_w);
        out_h = std::min(fit_h * scale, avail_h);
    }

    /// Editor-camera state (used when camera_mode_ == SceneView).  Exposed so
    /// the host editor can drive it from input or save/restore it across runs.
    Vec3& editor_cam_target()       { return editor_cam_target_; }
    float& editor_cam_distance()    { return editor_cam_distance_; }
    float& editor_cam_yaw_deg()     { return editor_cam_yaw_deg_; }
    float& editor_cam_pitch_deg()   { return editor_cam_pitch_deg_; }

    /// Bind the hierarchy panel so the viewport can draw an ImGuizmo handle
    /// over the currently-selected entity and write edits back into its
    /// Transform3DComponent.  Optional — no gizmo is drawn when nullptr.
    void bind_hierarchy(class HierarchyPanel* h) { hierarchy_ = h; }

    /// Frame the viewport's editor camera on a world-space point (F-key style).
    void focus_on(Vec3 target, float distance = 6.0f) {
        editor_cam_target_   = target;
        editor_cam_distance_ = distance;
    }

    /// Host hook invoked when an asset (drag payload "NEXUS_ASSET_PATH") is
    /// dropped on the viewport.  The path is the absolute filesystem path;
    /// hosts decode by extension and place at editor camera target / a
    /// caller-provided strategy.
    using AssetDropCallback = std::function<void(const std::string&)>;
    void set_on_asset_drop(AssetDropCallback cb) { on_asset_drop_ = std::move(cb); }

private:
    void ensure_framebuffer(u32 w, u32 h);
    void release_framebuffer();
    void render_scene_to_fbo();

    GizmoMode gizmo_mode_{GizmoMode::Translate};
    GizmoSpace gizmo_space_{GizmoSpace::World};
    ViewportCameraMode camera_mode_{ViewportCameraMode::SceneView};
    bool show_grid_{true};
    bool show_axes_{true};
    bool gizmo_active_{false};
    bool hovered_{false};
    // Unity-style snap controls that feed ImGuizmo::Manipulate's snap parameter
    // when toggled. One value per operation keeps the UX intuitive.
    bool snap_enabled_{false};
    float snap_translate_{0.25f};
    float snap_rotate_deg_{15.0f};
    float snap_scale_{0.10f};
    // SceneView-only: shading mode + projection lock + scene-internal gizmo
    // visibility.  Defaults match Unity's "Shaded" + 3D + Gizmos-on.
    SceneShadingMode shading_mode_{SceneShadingMode::Shaded};
    bool view_2d_{false};
    bool show_scene_gizmos_{true};

    // GameView-only: aspect override + stats HUD, mirroring Unity's Game tab.
    GameAspect game_aspect_{GameAspect::Free};
    bool show_stats_{false};
    // Show / hide the Scene's gizmo + grid overlays inside the Game tab.  In
    // Unity this is the "Gizmos" toggle on the Game tab toolbar; the
    // SceneView already always draws them, so this only affects GameView.
    bool show_game_gizmos_{false};
    // 0..3 — picks one of up to 4 logical displays.  Cameras with a
    // matching `display_index` (when the engine grows multi-display support)
    // are routed to this viewport.  Currently a UI-only setting kept so the
    // toolbar matches Unity's Game tab.
    u8 display_index_{0};
    // 1.0..5.0 — zoom factor on the rendered framebuffer for pixel-perfect
    // inspection.  Values > 1.0 enlarge the image inside the panel area;
    // panel still letterboxes against the chosen aspect ratio.
    f32 game_scale_{1.0f};
    ViewportRenderStats last_stats_{};
    u32 width_{0};
    u32 height_{0};
    Scene* scene_{nullptr};
    ForwardRenderer3D* renderer_3d_{nullptr};
    BatchRenderer2D* renderer_2d_{nullptr};
    DebugRenderer* debug_renderer_{nullptr};
    ::nexus::anim::ParticleEmitterSystem* particle_system_{nullptr};
    nexus::rhi::RHI* rhi_{nullptr};
    std::unordered_map<u32, Mesh*> mesh_registry_;

    // Off-screen render target.
    nexus::rhi::FramebufferHandle fbo_{nexus::rhi::INVALID_HANDLE};
    u32 fbo_w_{0};
    u32 fbo_h_{0};

    // Editor (free-orbit) camera state.
    Vec3 editor_cam_target_{0.0f, 0.5f, 0.0f};
    float editor_cam_distance_{6.0f};
    float editor_cam_yaw_deg_{45.0f};
    float editor_cam_pitch_deg_{-25.0f};

    // Cached view/projection for the frame currently being drawn — needed so
    // the gizmo pass (outside render_scene_to_fbo) can reuse exactly the same
    // matrices that were used to render the mesh.
    Mat4 last_view_{1.0f};
    Mat4 last_proj_{1.0f};
    bool last_cam_ready_{false};

    // Optional hierarchy binding for the gizmo + frame-selected shortcut.
    class HierarchyPanel* hierarchy_{nullptr};
    AssetDropCallback on_asset_drop_;
};

// ─────────────────────────────────────────────────────────────────────────────
// HierarchyPanel — entity hierarchy tree view
// ─────────────────────────────────────────────────────────────────────────────

class HierarchyPanel : public Panel {
public:
    HierarchyPanel() : Panel("Hierarchy") {}

    void on_render() override;
    const char* type_id() const override { return "HierarchyPanel"; }

    void bind_scene(Scene* scene) { scene_ = scene; }

    /// Bind to the editor's central EditorSelection so the hierarchy and
    /// other consumers (Inspector, viewport gizmo, snapshot/restore on Play)
    /// share one source of truth.  When unbound the panel falls back to its
    /// own local storage — keeps headless unit tests trivial.
    void bind_selection(class EditorSelection* sel) { ext_selection_ = sel; }

    /// Bind the editor's UndoRedoManager so the eye/lock toggles become
    /// Ctrl+Z-able instead of mutating the registry directly.
    void bind_undo_manager(UndoRedoManager* mgr) { undo_mgr_ = mgr; }

    /// Single-select (clears existing multi-selection first).
    void set_selected_entity(u32 entity);
    void clear_selection();
    bool has_selection() const;
    u32 selected_entity() const;

    /// Filter entities by name.
    void set_filter(const std::string& filter) { filter_ = filter; }
    const std::string& filter() const { return filter_; }

    /// Multi-selection support — also routed through EditorSelection when bound.
    void add_to_selection(u32 entity);
    void remove_from_selection(u32 entity);
    std::vector<u32> multi_selection() const;
    void clear_multi_selection();
    bool is_multi_selected(u32 entity) const;

    /// Configure which mesh ids the Create menu assigns to the built-in
    /// primitives (Cube / Plane / Sphere).  The editor registers these mesh
    /// handles with the viewport at startup; pass the same ids here so that
    /// "Create > Cube" drops an entity that is immediately visible.  Use 0 to
    /// disable a primitive entry.
    void set_primitive_mesh_ids(u32 cube, u32 plane, u32 sphere) {
        primitive_cube_id_   = cube;
        primitive_plane_id_  = plane;
        primitive_sphere_id_ = sphere;
    }

    /// Begin inline rename of an entity's TagComponent.  No-op if the entity
    /// has no TagComponent.  The next render frame focuses an InputText on
    /// the row; Enter commits, Esc/blur cancels.
    void begin_rename(u32 entity);
    bool is_renaming() const { return is_renaming_; }
    u32 renaming_entity() const { return renaming_; }

    /// Host callback invoked when an asset is dropped on the hierarchy panel
    /// (drag from AssetBrowserPanel, payload type "NEXUS_ASSET_PATH").  The
    /// callback receives the absolute asset path and the target entity (0 if
    /// dropped on empty space).  Default: nullptr — drop is ignored.
    using AssetDropCallback = std::function<void(const std::string&, u32)>;
    void set_on_asset_drop(AssetDropCallback cb) { on_asset_drop_ = std::move(cb); }

private:
    Scene* scene_{nullptr};
    class EditorSelection* ext_selection_{nullptr};
    UndoRedoManager* undo_mgr_{nullptr};
    u32 selected_{0};
    bool has_selection_{false};
    std::string filter_;
    std::vector<u32> multi_selection_;
    u32 renaming_{0};
    bool is_renaming_{false};
    bool rename_focus_pending_{false};
    char rename_buffer_[128]{0};
    AssetDropCallback on_asset_drop_;
    u32 primitive_cube_id_{0};
    u32 primitive_plane_id_{0};
    u32 primitive_sphere_id_{0};
};

// ─────────────────────────────────────────────────────────────────────────────
// InspectorPanel — property editor for selected entity
// ─────────────────────────────────────────────────────────────────────────────

struct PropertyEdit {
    std::string component;
    std::string property;
    std::string old_value;
    std::string new_value;
};

class InspectorPanel : public Panel {
public:
    InspectorPanel() : Panel("Inspector") {}

    void on_render() override;
    const char* type_id() const override { return "InspectorPanel"; }

    void bind_scene(Scene* scene) { scene_ = scene; }

    /// Bind to the editor's central UndoRedoManager so transform / property
    /// edits become Ctrl+Z-able.  When unbound the inspector still works
    /// but its edits are not reversible.
    void bind_undo_manager(UndoRedoManager* mgr) { undo_mgr_ = mgr; }

    /// Bind to the editor's central EditorSelection so multi-select edits
    /// broadcast — a drag on the primary entity's field is mirrored to every
    /// other selected entity in a single undo step (Unity behavior).
    void bind_selection(class EditorSelection* sel) { ext_selection_ = sel; }

    void set_target_entity(u32 entity) { target_ = entity; has_target_ = true; }
    void clear_target() { has_target_ = false; target_ = 0; }
    bool has_target() const { return has_target_; }
    u32 target_entity() const { return target_; }

    /// Bind the component registry that drives the "Add Component" menu.
    /// Optional — when unbound the button is hidden.  The registry is owned
    /// by the host (editor_main); the panel only borrows.
    void bind_component_registry(class ComponentRegistry* reg) {
        component_registry_ = reg;
    }
    class ComponentRegistry* component_registry() const {
        return component_registry_;
    }

    /// Asset id → human-readable path resolvers, one per cache family.
    /// Inspector uses these to render `{Label}: {basename}` next to the
    /// raw integer id slot of MeshRenderer.material_id / AudioSource.clip_id
    /// / AnimatorComponent.clip_id.  Each callback returns "" when the id
    /// is unknown — Inspector falls back to the integer in that case.
    /// All four are independent — wire only what the host project needs.
    struct AssetPathResolvers {
        std::function<std::string(u32)> material;   // MaterialAssetCache::path_for
        std::function<std::string(u32)> animation;  // AnimationAssetCache::path_for
        std::function<std::string(u32)> audio;      // AudioAssetCache::path_for
        std::function<std::string(u32)> prefab;     // PrefabAssetCache::path_for
    };
    void bind_asset_path_resolvers(AssetPathResolvers r) {
        asset_paths_ = std::move(r);
    }
    const AssetPathResolvers& asset_path_resolvers() const {
        return asset_paths_;
    }

    // ── Script hot-import (M33) ────────────────────────────────────────
    //
    // The Script inspector grows an "Import .lua" row that lets the
    // user point at a script asset on disk and bind it to the
    // selected entity's ScriptComponent in one click.  The host wires
    // this to a callback that:
    //   1. Reads the file
    //   2. Registers it with ScriptEngine for hot-reload
    //   3. Returns the canonical script_name (file stem) the panel
    //      assigns onto the component.
    // Callback signature:
    //   bool import(const std::string& path, std::string& out_name);
    // On `true`, the panel writes out_name into ScriptComponent.script_name
    // and clears `initialized` so ScriptSystem rebinds next tick.
    using ScriptImportCallback =
        std::function<bool(const std::string& path, std::string& out_name)>;
    void set_on_script_import(ScriptImportCallback cb) {
        on_script_import_ = std::move(cb);
    }
    bool has_script_import_callback() const {
        return static_cast<bool>(on_script_import_);
    }

    /// Headless entry point for tests.  Returns true when the callback
    /// fired *and* succeeded *and* the target's ScriptComponent was
    /// updated.  Validates: target alive, ScriptComponent present,
    /// callback bound.  Empty path is rejected without firing the cb.
    bool request_script_import(u32 entity, const std::string& path);

    /// Track pending edits.
    void push_edit(const PropertyEdit& edit) { pending_edits_.push_back(edit); }
    std::vector<PropertyEdit> drain_edits();

    /// Lock the inspector to current target (don't follow selection).
    bool is_locked() const { return locked_; }
    void set_locked(bool l) { locked_ = l; }

    /// Target list for broadcast edits.  Returns the bound selection's list
    /// when it has 2+ entries and the primary matches target_; otherwise just
    /// the single target.  Captured at drag-begin and used to compose the
    /// undo command's apply callback so every entity in the set updates
    /// atomically.
    std::vector<u32> current_edit_targets() const;

private:
    Scene* scene_{nullptr};
    UndoRedoManager* undo_mgr_{nullptr};
    class EditorSelection* ext_selection_{nullptr};
    class ComponentRegistry* component_registry_{nullptr};
    AssetPathResolvers asset_paths_{};
    ScriptImportCallback on_script_import_;
    // Script-import path buffer kept across frames so the user's typed
    // path persists while the .lua field is open.
    char script_import_path_[512] = {};
    // Add-component popup state.  The search buffer is kept across frames
    // so the user's typed query persists while the popup is open.
    char add_component_search_[128] = {};
    u32 target_{0};
    bool has_target_{false};
    bool locked_{false};
    std::vector<PropertyEdit> pending_edits_;
    std::vector<u32> drag_begin_targets_;

    // ── Drag-edit undo capture ────────────────────────────────────────
    // ImGui's DragFloat issues one update per mouse-move pixel — we don't
    // want one undo step per pixel.  These three fields cache the value at
    // drag-begin (IsItemActivated) so we can push exactly one command at
    // drag-end (IsItemDeactivatedAfterEdit).  Only one widget can be active
    // at a time so a single slot suffices.  Holds floats, colors, OR ints
    // (reinterpreted) — ImGui guarantees one active widget across all types.
    u64 drag_widget_id_{0};
    f32 drag_begin_f_[4]{0, 0, 0, 0};
    i32 drag_begin_i_{0};
};

// ─────────────────────────────────────────────────────────────────────────────
// ConsolePanel — log output and command input
// ─────────────────────────────────────────────────────────────────────────────

enum class LogLevel : u8 {
    Info, Warning, Error, Debug
};

// ── Source classification (M34) ─────────────────────────────────────────
//
// Every ConsoleMessage is tagged with the subsystem that produced it so
// the panel can show per-source filter chips ([Engine] / [Lua] /
// [Lua print] / [Script error]).  Existing call sites that use only the
// `add_message(text, level)` overload have their source auto-classified
// by prefix-sniffing the text — see `classify_console_source()` and the
// editor_main wiring (M29 / M31 / M32) that already chose those prefixes.
// New call sites should pass the source explicitly.
enum class LogSource : u8 {
    Engine,       // spdlog → ConsolePanelSink (no prefix)
    Lua,          // Lua Console panel results — "[Lua] ..."
    LuaPrint,     // Lua print() output (M32) — "[Lua print] ..."
    ScriptError,  // ScriptEngine::set_error_handler (M31) — "[Script] ..."
    Editor        // status messages emitted from the editor itself
};

/// Auto-classify a message's source from its leading prefix.  Used when
/// callers invoke the legacy `add_message(text, level)` overload — the
/// existing prefixes from M29/M31/M32 are stable so this stays a pure
/// string-pattern match.  Returns LogSource::Engine for unknown prefixes
/// so spdlog-routed messages keep their "Engine" classification.
LogSource classify_console_source(const std::string& text);

struct ConsoleMessage {
    std::string text;
    LogLevel level{LogLevel::Info};
    LogSource source{LogSource::Engine};
    /// Wall-clock seconds since the editor process began — set by
    /// add_message at insertion time.  0 means "unset" (legacy callers).
    f64 timestamp{0.0};
    /// When Collapse mode is on, repeats are merged into the first occurrence
    /// and `count` is incremented instead of appending.
    u32 count{1};
    /// SHA-1-style 64-bit hash of (text, level) used for collapse lookup.
    /// Computed once at insertion to avoid restringing on every frame.
    u64 dedup_key{0};
    /// Optional source-file pointer (M36).  Populated for ScriptError
    /// messages so a double-click can jump back to the offending line
    /// without re-parsing the formatted text.  Empty file or
    /// source_line == 0 ⇒ row has no jump target.
    std::string source_file;
    u32 source_line{0};
};

class ConsolePanel : public Panel {
public:
    ConsolePanel() : Panel("Console") {}

    void on_render() override;
    const char* type_id() const override { return "ConsolePanel"; }

    void add_message(const std::string& text, LogLevel level = LogLevel::Info);
    /// Source-aware overload (M34).  Caller supplies the producing
    /// subsystem so the panel doesn't have to prefix-sniff.  Used by
    /// editor_main when wiring Lua / Script error / Lua print sinks.
    void add_message(const std::string& text, LogLevel level, LogSource source);
    /// Full overload (M36) — also carries a jump target so script
    /// runtime errors stay clickable.  source_line == 0 marks the row
    /// as non-clickable.  source_file is copied verbatim; resolution is
    /// the host's job (the editor walks AssetRegistry to find it).
    void add_message(const std::string& text, LogLevel level, LogSource source,
                     std::string source_file, u32 source_line);
    void clear();

    const std::vector<ConsoleMessage>& messages() const { return messages_; }
    u32 message_count() const { return static_cast<u32>(messages_.size()); }

    /// Running counts by severity — O(1) lookup for status-bar badges and
    /// per-level segmented buttons in the toolbar.
    u32 info_count() const { return info_count_; }
    u32 warning_count() const { return warning_count_; }
    u32 error_count() const { return error_count_; }
    u32 debug_count() const { return debug_count_; }

    /// Per-source counters (M34) — feed the source filter chip badges.
    u32 source_count(LogSource s) const;

    /// Filter by log level.
    void set_level_filter(LogLevel level, bool show);
    bool is_level_shown(LogLevel level) const;

    /// Filter by source (M34) — multi-source chips, all sources visible
    /// by default so existing UX is unchanged.  When every source is
    /// hidden the panel still renders the toolbar so the user can
    /// re-enable a chip without going through a menu.
    void set_source_filter(LogSource source, bool show);
    bool is_source_shown(LogSource source) const;

    /// Smart auto-scroll: scrolls to bottom only when the user was already at
    /// the bottom on the previous frame.  Toggling this off disables the
    /// behavior entirely (manual control).
    bool auto_scroll() const { return auto_scroll_; }
    void set_auto_scroll(bool s) { auto_scroll_ = s; }

    /// Collapse mode: repeated identical messages fold into a single entry
    /// with a count badge — Unity-parity.
    bool collapse_mode() const { return collapse_; }
    void set_collapse_mode(bool c);

    /// Clear the console at the moment the editor enters Play.  Wired by the
    /// host via the SceneBridge::on_play hook.
    bool clear_on_play() const { return clear_on_play_; }
    void set_clear_on_play(bool c) { clear_on_play_ = c; }

    /// Pause editor Play on the first new error.  Owned by the host: the
    /// panel only flips a request flag; the editor loop polls and acts.
    bool error_pause() const { return error_pause_; }
    void set_error_pause(bool p) { error_pause_ = p; }
    /// Returns true at most once per error: signals "host should pause now".
    bool consume_error_pause_request() {
        bool req = error_pause_request_;
        error_pause_request_ = false;
        return req;
    }

    /// Host hook: called when the editor enters Play.  Honors clear_on_play_.
    void on_enter_play();

    /// Max messages before pruning.
    void set_max_messages(u32 max) { max_messages_ = max; }
    u32 max_messages() const { return max_messages_; }

    /// Filter displayed messages by substring match on the message text.
    /// Match is case-insensitive (Unity-parity).
    void set_search(const std::string& s) { search_ = s; }
    const std::string& search() const { return search_; }

    // ── Jump-to-source (M36) ───────────────────────────────────────────
    //
    // When the user double-clicks a console row that carries a non-empty
    // (source_file, source_line) target, the panel fires this callback.
    // The host wires it to LuaConsolePanel::set_source so script
    // runtime errors land the user back on the offending line.  When no
    // handler is registered, double-clicking is a silent no-op (parity
    // with engine logs which never carry a target).
    using JumpHandler =
        std::function<void(const std::string& file, u32 line)>;
    void set_on_jump_to_source(JumpHandler h) { on_jump_ = std::move(h); }
    bool has_jump_handler() const { return static_cast<bool>(on_jump_); }

    /// Test-friendly: simulate a double-click on the message at `index`.
    /// Returns true if the row had a target *and* a handler fired.
    bool request_jump_at(u32 index);

    // ── Right-click context menu (M37) ─────────────────────────────────
    //
    // Three host-bound actions that complement the M36 double-click
    // jump.  Reveal asks the asset browser to scroll/expand to the
    // row's source_file; Open externally hands the path off to the OS
    // shell (Finder / xdg-open / explorer).  Both no-op silently when
    // the row has no source_file.  Copy is panel-local — formats the
    // row using format_row_for_clipboard().
    using RevealHandler = std::function<void(const std::string& file)>;
    using OpenHandler   = std::function<void(const std::string& file)>;
    void set_on_reveal_in_browser(RevealHandler h) { on_reveal_ = std::move(h); }
    void set_on_open_externally(OpenHandler h)     { on_open_   = std::move(h); }
    bool has_reveal_handler() const { return static_cast<bool>(on_reveal_); }
    bool has_open_handler()   const { return static_cast<bool>(on_open_);   }

    /// Build the text the Copy action puts on the clipboard.  Mirrors
    /// the format used by the toolbar Copy button so users can paste
    /// either into the same destination.  When `with_timestamp` is
    /// false the leading "[hh:mm:ss.mmm] " prefix is stripped.  Empty
    /// string when index is out of range.
    std::string format_row_for_clipboard(u32 index,
                                          bool with_timestamp = true) const;

    /// Test-friendly: simulate the Reveal action on row `index`.
    /// Returns true if the row had a source_file *and* a handler fired.
    bool request_reveal_at(u32 index);

    /// Test-friendly: simulate the Open Externally action on row `index`.
    /// Returns true if the row had a source_file *and* a handler fired.
    bool request_open_at(u32 index);

    // ── Export to file (M40) ───────────────────────────────────────────
    //
    // Two granularities so users can share either the full transcript
    // or just what's currently visible in the panel.  `filtered` honours
    // the active level chips (M-pre), source chips (M34), and search
    // box; `all` ignores them and dumps the full ring.  Format matches
    // the toolbar Copy button so a saved log pastes into the same
    // tools as a clipboard copy.
    enum class ExportScope : u8 { All, Filtered };
    std::string export_to_string(ExportScope scope = ExportScope::Filtered) const;
    /// Write export_to_string() to disk.  Returns true on success;
    /// failure to open the path is reported to NX_WARN by the caller
    /// (the panel itself is silent).  Empty panels produce empty files
    /// — that's intentional so a "Save" pressed on a fresh launch
    /// doesn't surprise the user with leftover content.
    bool export_to_file(const std::string& path,
                        ExportScope scope = ExportScope::Filtered) const;

private:
    std::vector<ConsoleMessage> messages_;
    bool show_info_{true};
    bool show_warning_{true};
    bool show_error_{true};
    bool show_debug_{true};
    bool auto_scroll_{true};
    bool collapse_{false};
    bool clear_on_play_{true};
    bool error_pause_{false};
    bool error_pause_request_{false};
    bool was_at_bottom_{true};
    u32 max_messages_{1000};
    u32 info_count_{0};
    u32 warning_count_{0};
    u32 error_count_{0};
    u32 debug_count_{0};

    // Per-source state (M34).  Aligned with LogSource enum order; new
    // sources added to the enum must extend both arrays.
    static constexpr u32 kSourceCount = 5;
    bool show_source_[kSourceCount]{true, true, true, true, true};
    u32  source_counts_[kSourceCount]{0, 0, 0, 0, 0};

    JumpHandler   on_jump_;
    RevealHandler on_reveal_;
    OpenHandler   on_open_;
    std::string search_;
};

// ─────────────────────────────────────────────────────────────────────────────
// AssetBrowserPanel — browse and manage project assets
// ─────────────────────────────────────────────────────────────────────────────

struct AssetBrowserEntry {
    std::string name;
    std::string path;
    std::string extension;
    bool is_directory{false};
    u64 file_size{0};
};

class AssetBrowserPanel : public Panel {
public:
    AssetBrowserPanel() : Panel("Asset Browser") {}

    void on_render() override;
    const char* type_id() const override { return "AssetBrowserPanel"; }

    /// Set the root directory for browsing.  navigate_up never climbs above
    /// this path — the user can't accidentally navigate into the parent FS.
    void set_root_path(const std::string& path) { root_path_ = path; }
    const std::string& root_path() const { return root_path_; }

    /// Navigate into a directory.
    void navigate_to(const std::string& path);
    void navigate_up();
    const std::string& current_path() const { return current_path_; }

    /// Get directory history.
    const std::vector<std::string>& history() const { return history_; }
    bool can_go_back() const { return history_index_ > 0; }
    bool can_go_forward() const {
        return history_index_ + 1 < static_cast<i32>(history_.size());
    }
    void go_back();
    void go_forward();

    /// Selected asset.
    void set_selected(const std::string& path) { selected_ = path; }
    const std::string& selected() const { return selected_; }

    /// Search/filter.
    void set_search(const std::string& query) { search_ = query; }
    const std::string& search() const { return search_; }

    /// Recursive search opt-in.  When true and search is non-empty, the
    /// panel asks the host (via on_search_request_) for a project-wide list.
    bool recursive_search() const { return recursive_search_; }
    void set_recursive_search(bool r) { recursive_search_ = r; }

    /// Set entries for current directory (or the recursive search result).
    void set_entries(std::vector<AssetBrowserEntry> entries) {
        entries_ = std::move(entries);
    }
    const std::vector<AssetBrowserEntry>& entries() const { return entries_; }

    /// View mode.
    enum class ViewMode : u8 { Grid, List };
    ViewMode view_mode() const { return view_mode_; }
    void set_view_mode(ViewMode mode) { view_mode_ = mode; }

    /// Thumbnail size for grid view.
    u32 thumbnail_size() const { return thumbnail_size_; }
    void set_thumbnail_size(u32 size) { thumbnail_size_ = size; }

    // ── Project sidebar (Unity-style left rail) ──────────────────────────
    //
    // None        → browse current_path_ as a regular directory.
    // Favorites   → entries are the favorites_ set (resolved by host scan).
    // AllMaterials/AllModels/AllPrefabs → entries are project-wide files of
    // the corresponding extension family.  When the panel switches filter,
    // it raises navigation_dirty_ so the host re-populates entries_ via the
    // filter callback or its own scanner.  The filter is sticky until the
    // user clicks "Assets" / a folder breadcrumb to clear it.
    enum class SidebarFilter : u8 {
        None,
        Favorites,
        AllMaterials,
        AllModels,
        AllPrefabs,
    };
    SidebarFilter current_filter() const { return current_filter_; }
    void set_filter(SidebarFilter f);

    using FilterCallback = std::function<void(SidebarFilter)>;
    void set_on_filter_request(FilterCallback cb) {
        on_filter_request_ = std::move(cb);
    }

    // Favorites — host persists across sessions if it wants; the panel just
    // owns the in-memory set so the sidebar can render it and the context
    // menu can toggle membership.
    void add_favorite(const std::string& path)    { favorites_.insert(path); }
    void remove_favorite(const std::string& path) { favorites_.erase(path);  }
    bool is_favorite(const std::string& path) const {
        return favorites_.find(path) != favorites_.end();
    }
    const std::unordered_set<std::string>& favorites() const { return favorites_; }
    u32 favorite_count() const { return static_cast<u32>(favorites_.size()); }

    /// Host callbacks — kept as std::function so they're easy to bind without
    /// dragging Scene/AssetRegistry into editor_panels.h.
    using OpenCallback   = std::function<void(const AssetBrowserEntry&)>;
    using ActionCallback = std::function<void(const AssetBrowserEntry&)>;
    void set_on_open(OpenCallback cb)        { on_open_   = std::move(cb); }
    void set_on_reveal(ActionCallback cb)    { on_reveal_ = std::move(cb); }
    void set_on_delete(ActionCallback cb)    { on_delete_ = std::move(cb); }
    void set_on_refresh(std::function<void()> cb) { on_refresh_ = std::move(cb); }

    /// Returns true at most once per actual change: the host polls this and
    /// rescans the directory only when the panel has navigated (no per-frame
    /// FS work).  Cleared once read.
    bool consume_navigation_dirty() {
        bool d = navigation_dirty_;
        navigation_dirty_ = false;
        return d;
    }

    /// Bind a thumbnail (already-uploaded ImGui texture id) to an asset path.
    /// Pass `0` to drop the binding.  Hosts populate this from a real image
    /// decoder (stb_image + RHI upload) and the panel renders the texture in
    /// place of the text glyph for matching entries.  When a path has no
    /// thumbnail, the panel falls back to a per-extension colored tile so
    /// the user still gets a visual cue.
    void set_thumbnail(const std::string& path, std::uintptr_t tex_id) {
        if (tex_id == 0) thumbnails_.erase(path);
        else             thumbnails_[path] = tex_id;
    }

    /// Material color sampler — host hook that returns the albedo color for
    /// a `.mat` path, e.g. via MaterialAssetCache.  When bound, the
    /// AssetBrowserPanel paints material tiles using the file's actual
    /// color instead of the generic "material teal" extension tint, giving
    /// users a Unity-style at-a-glance read.  Return false from the
    /// callback to fall back to the extension color (e.g. file failed to
    /// parse).
    using MaterialColorSampler =
        std::function<bool(const std::string& path, f32 rgba[4])>;
    void set_material_color_sampler(MaterialColorSampler s) {
        material_color_sampler_ = std::move(s);
    }
    bool has_material_color_sampler() const {
        return static_cast<bool>(material_color_sampler_);
    }

    /// Pure helpers exposed for tests so the visual contract for asset
    /// classes can be verified headlessly without ImGui.  `tile_color`
    /// returns 32-bit RGBA in ImU32 layout (R, G, B, A from low byte).
    static const char* default_icon_for(const std::string& extension);
    static u32 default_tile_color_for(const std::string& extension);
    std::uintptr_t thumbnail(const std::string& path) const {
        auto it = thumbnails_.find(path);
        return it == thumbnails_.end() ? 0u : it->second;
    }
    u32 thumbnail_count() const { return static_cast<u32>(thumbnails_.size()); }

private:
    std::string root_path_;
    std::string current_path_;
    std::string selected_;
    std::string search_;
    std::vector<std::string> history_;
    i32 history_index_{-1};
    std::vector<AssetBrowserEntry> entries_;
    ViewMode view_mode_{ViewMode::Grid};
    u32 thumbnail_size_{64};
    bool recursive_search_{false};
    bool navigation_dirty_{true};

    OpenCallback on_open_;
    ActionCallback on_reveal_;
    ActionCallback on_delete_;
    std::function<void()> on_refresh_;
    std::unordered_map<std::string, std::uintptr_t> thumbnails_;
    MaterialColorSampler material_color_sampler_;

    SidebarFilter current_filter_{SidebarFilter::None};
    FilterCallback on_filter_request_;
    std::unordered_set<std::string> favorites_;
};

// ─────────────────────────────────────────────────────────────────────────────
// UndoHistoryPanel — Unity-style Edit > Undo History window
// ─────────────────────────────────────────────────────────────────────────────
//
// Shows both the undo stack (applied, reverse chronological) and redo stack
// (available, forward chronological).  Clicking any row revert/advances the
// UndoRedoManager to that exact history point via jump_to_undo().  Highlights
// the dirty indicator (asterisk on current item) the same way Unity does.
class UndoHistoryPanel : public Panel {
public:
    UndoHistoryPanel() : Panel("Undo History") {}

    void on_render() override;
    const char* type_id() const override { return "UndoHistoryPanel"; }

    void bind_undo_manager(UndoRedoManager* mgr) { mgr_ = mgr; }

private:
    UndoRedoManager* mgr_{nullptr};
};

// ─────────────────────────────────────────────────────────────────────────────
// ProfilerPanel — Unity-style Window > Analysis > Profiler (CPU)
// ─────────────────────────────────────────────────────────────────────────────
//
// Draws an FPS/frame-time histogram over the last N frames and a flat list of
// the most recent frame's CPU samples (name, duration, percent of frame).
// Reads directly from an externally-owned Profiler — presence of the pointer
// is the enable switch.  The panel does not own any sampling.
class ProfilerPanel : public Panel {
public:
    ProfilerPanel() : Panel("Profiler") {}

    void on_render() override;
    const char* type_id() const override { return "ProfilerPanel"; }

    void bind_profiler(Profiler* p) { profiler_ = p; }

    bool is_paused() const { return paused_; }
    void set_paused(bool p) { paused_ = p; }

private:
    Profiler* profiler_{nullptr};
    bool paused_{false};
    // Frozen snapshot taken at the moment the user pauses so every UI region
    // (histogram + sample table) shows the same instant.  Without this the
    // graph would freeze but the sample list would keep updating, making the
    // numbers visibly inconsistent with the chart.
    std::vector<f32> frozen_frame_times_ms_;
    FrameProfile     frozen_last_frame_{};
    bool             frozen_has_frame_{false};
};

// ─────────────────────────────────────────────────────────────────────────────
// AnimationPanel — Unity-style Animation window (read-only dopesheet)
// ─────────────────────────────────────────────────────────────────────────────
//
// Visualizes the keyframes of an `anim::AnimationClip` as horizontal tracks
// (one row per channel × component) with diamond markers at each keyframe's
// `time`.  The current playhead is drawn as a vertical line that the user
// can scrub by clicking inside the timeline area.
//
// Scope (M15):
//   • Read-only display + scrub.  Edits (add / move / delete keyframe) are
//     deferred until later milestones — the data model (BoneChannel) is
//     already mutable, so editing slots in cleanly later.
//   • Clip is resolved through a `ClipResolver` callback so the panel
//     stays decoupled from AnimationAssetCache.
//   • Selection follow: when the editor's selected entity has an
//     AnimatorComponent, the panel auto-binds to that component's clip_id.
//     Hosts call `set_clip_id()` directly when navigating from the Project
//     view (double-click .anim).
//
// Pure helpers (`time_to_x` / `x_to_time` / `clip_total_keys`) are static so
// the panel can be unit-tested headlessly.

class AnimationPanel : public Panel {
public:
    AnimationPanel() : Panel("Animation") {}

    void on_render() override;
    const char* type_id() const override { return "AnimationPanel"; }

    using ClipResolver =
        std::function<const ::nexus::anim::AnimationClip*(u32)>;
    void set_clip_resolver(ClipResolver r) { resolver_ = std::move(r); }
    bool has_clip_resolver() const { return static_cast<bool>(resolver_); }

    /// Mutable resolver — when bound the dopesheet exposes editing
    /// affordances (Add Position/Rotation/Scale Key at playhead, Delete
    /// selected key).  Decoupled from the const resolver so the host can
    /// gate editing per-cache (e.g. read-only when the file is on a
    /// version-controlled path).
    using MutableClipResolver =
        std::function<::nexus::anim::AnimationClip*(u32)>;
    void set_mutable_clip_resolver(MutableClipResolver r) {
        mutable_resolver_ = std::move(r);
    }
    bool has_mutable_clip_resolver() const {
        return static_cast<bool>(mutable_resolver_);
    }

    /// Bone index targeted by Add Key / Delete Key actions.  Default 0
    /// (root bone).  The toolbar exposes an int input for this.
    i32 active_bone() const { return active_bone_; }
    void set_active_bone(i32 b) { active_bone_ = b < 0 ? 0 : b; }

    /// Selected key index for Delete Key — paired with selected_kind_.
    /// (-1 means "no selection").
    enum class KeyKind : u8 { None, Position, Rotation, Scale };
    KeyKind selected_key_kind() const { return selected_kind_; }
    i32     selected_key_index() const { return selected_idx_; }
    void clear_key_selection() {
        selected_kind_ = KeyKind::None;
        selected_idx_  = -1;
    }
    void select_key(KeyKind kind, i32 index) {
        selected_kind_ = kind;
        selected_idx_  = index;
    }

    /// Bind / query the active clip id.  Setting 0 hides the dopesheet.
    void set_clip_id(u32 id) { clip_id_ = id; }
    u32 clip_id() const { return clip_id_; }

    /// Read-only playhead in seconds, clamped to [0, duration].  Hosts can
    /// drive this from AnimatorComponent.time so the panel reflects the
    /// runtime animator's current sample position.
    f32 playhead() const { return playhead_; }
    void set_playhead(f32 t) { playhead_ = t < 0.0f ? 0.0f : t; }

    /// Pixels-per-second zoom for the timeline.  Default suits a 1-2s
    /// clip in a typical dock width.
    f32 zoom() const { return zoom_; }
    void set_zoom(f32 z) {
        if (z < 16.0f)  z = 16.0f;
        if (z > 4096.0f) z = 4096.0f;
        zoom_ = z;
    }

    // ── Pure helpers (test-friendly) ─────────────────────────────────────

    /// Map a time in [0, duration] seconds onto a screen-x in
    /// [view_x_min, view_x_max].  Linear; clamps when t is out of range.
    static f32 time_to_x(f32 t, f32 view_x_min, f32 view_x_max,
                         f32 duration);

    /// Inverse — pixel x → time.  Out-of-range x clamps to [0, duration].
    static f32 x_to_time(f32 x, f32 view_x_min, f32 view_x_max,
                         f32 duration);

    /// Total keyframe count across all channels (positions + rotations +
    /// scales).  Used by the panel header for a quick read and by tests.
    static u32 clip_total_keys(const anim::AnimationClip& clip);

private:
    ClipResolver        resolver_;
    MutableClipResolver mutable_resolver_;
    u32 clip_id_{0};
    f32 playhead_{0.0f};
    f32 zoom_{120.0f};
    i32 active_bone_{0};
    KeyKind selected_kind_{KeyKind::None};
    i32     selected_idx_{-1};
};

// ─────────────────────────────────────────────────────────────────────────────
// LuaConsolePanel — in-editor Lua scripting workspace (M22)
// ─────────────────────────────────────────────────────────────────────────────
//
// A simple-but-real REPL inside the editor: the user types Lua source into
// a multi-line buffer, hits Run, and the panel feeds the buffer to the
// engine's LuaBackend.  Output (success message, error text, optional
// returned value) is appended to a capped history so users can iterate
// without re-typing.
//
// Decoupled from LuaBackend through a `Runner` callback so the panel
// stays testable without spinning up the scripting engine.  Production
// wiring (editor_main) hands a runner that calls
// LuaBackend::execute + last_error.
class LuaConsolePanel : public Panel {
public:
    LuaConsolePanel() : Panel("Lua Console") {}
    void on_render() override;
    const char* type_id() const override { return "LuaConsolePanel"; }

    /// Outcome of a single Run press.  Tests + the panel both consume
    /// this: success → green message, error → red plus error text.
    struct RunResult {
        bool        ok{true};
        std::string error;       // empty on success
        std::string output;      // optional explicit log line
    };
    using Runner = std::function<RunResult(const std::string& script)>;

    void set_runner(Runner r) { runner_ = std::move(r); }
    bool has_runner() const { return static_cast<bool>(runner_); }

    /// Buffer contents.  Bounded by a sane cap so a runaway paste can't
    /// blow up the panel.
    static constexpr u32 kBufferCap = 64u * 1024u;
    const std::string& source() const { return source_; }
    void set_source(std::string s);

    /// History — append-only ring of {timestamp_text, message, ok}.  Cap
    /// below means the oldest lines drop off when full.
    static constexpr u32 kHistoryCap = 256u;
    struct HistoryLine {
        std::string text;
        bool        ok{true};
    };
    const std::vector<HistoryLine>& history() const { return history_; }
    void clear_history() { history_.clear(); }

    /// Run the current buffer (or the override) through the bound
    /// runner and append the outcome to history.  Returns the result.
    /// When `runner_` is unbound, returns a synthetic error so callers
    /// can surface the missing wire-up.  Pure entry point for tests.
    RunResult run_now() { return run_source(source_); }
    RunResult run_source(const std::string& src);

    /// External log forwarder — fires once per Run result.  When the
    /// editor wires this to ConsolePanel::add_message (M29), every Lua
    /// run also surfaces in the main Console pane so users don't need
    /// the Lua panel open to see what scripts produced.  ok=true maps
    /// to Info, false to Error.  Optional — leaving it unbound just
    /// keeps results in the panel's local history.
    using LogSink =
        std::function<void(bool ok, const std::string& message)>;
    void set_on_log(LogSink s) { on_log_ = std::move(s); }
    bool has_on_log() const { return static_cast<bool>(on_log_); }

private:
    Runner runner_;
    std::string source_;
    std::vector<HistoryLine> history_;
    LogSink on_log_;
};

// ─────────────────────────────────────────────────────────────────────────────
// RuntimeStatsPanel — unified view across Animator / Particle / Profiler (M28)
// ─────────────────────────────────────────────────────────────────────────────
//
// Aggregates the editor's runtime systems into one observation surface so
// users don't need three separate panels open to read live numbers.
// Built around a `Snapshot` POD that the editor refreshes each frame from
// supplier callbacks.  Tests inject snapshots directly so they don't need
// the systems running.
class RuntimeStatsPanel : public Panel {
public:
    RuntimeStatsPanel() : Panel("Runtime Stats") {}
    void on_render() override;
    const char* type_id() const override { return "RuntimeStatsPanel"; }

    /// Live counters — all signed only because we never decrement past 0.
    /// Defaults are explicitly 0 / 0.0 so a panel rendered before the
    /// first refresh shows "0/0/0.0 ms" rather than garbage.
    struct Snapshot {
        // Animator
        u32 animator_components{0};   // count with AnimatorComponent
        u32 animator_playing{0};      // subset that's actively playing

        // Particles
        u32 particle_emitters{0};     // count with ParticleEmitterComponent
        u32 particles_alive{0};       // sum across every emitter

        // Profiler / frame
        f32 fps{0.0f};
        f32 frame_ms{0.0f};
        u32 entity_count{0};
    };

    /// Pure setter — tests use this to drive the panel headlessly.  In
    /// production, editor_main calls this per-frame inside the dock-pump
    /// loop with values pulled fresh from each system.
    void set_snapshot(const Snapshot& s) { snapshot_ = s; }
    const Snapshot& snapshot() const { return snapshot_; }

    /// Optional supplier — when bound, the panel calls it once per
    /// render and overwrites the cached snapshot.  Convenience for
    /// hosts that don't want to push manually.
    using Supplier = std::function<Snapshot()>;
    void set_supplier(Supplier s) { supplier_ = std::move(s); }
    bool has_supplier() const { return static_cast<bool>(supplier_); }

private:
    Snapshot snapshot_{};
    Supplier supplier_;
};

// ─────────────────────────────────────────────────────────────────────────────
// WatchPanel — pinned Lua expression evaluator (M35)
// ─────────────────────────────────────────────────────────────────────────────
//
// Lets the user pin Lua-side expressions ("entity_count()",
// "Math.PI", "tostring(player.health)") and see their value
// re-evaluated every frame — Unity's "Watch" / VSCode's debugger watch
// pane in spirit.  Decoupled from LuaBackend through an Evaluator
// callback so the panel stays library-agnostic and headless-testable.
class WatchPanel : public Panel {
public:
    WatchPanel() : Panel("Watch") {}
    void on_render() override;
    const char* type_id() const override { return "WatchPanel"; }

    /// Outcome of one expression evaluation.  Mirrors
    /// LuaConsolePanel::RunResult so the editor can wire either panel
    /// to the same backend with minimal adapters.
    struct EvalResult {
        bool        ok{true};
        std::string value;       // empty when !ok
        std::string error;       // empty when ok
    };
    using Evaluator = std::function<EvalResult(const std::string& expr)>;

    void set_evaluator(Evaluator e) { evaluator_ = std::move(e); }
    bool has_evaluator() const { return static_cast<bool>(evaluator_); }

    /// One pinned watch.  `name` is the user-facing label (defaults to
    /// the expression itself), `expression` is the Lua text.  The
    /// `last_*` fields are written by tick() so the renderer doesn't
    /// re-evaluate on every ImGui call.
    struct WatchEntry {
        std::string name;
        std::string expression;
        std::string last_value;
        std::string last_error;
        bool        ok{true};
        bool        evaluated{false};  // false until the first tick()
    };

    /// Append a new watch.  `name` empty ⇒ name = expression.  Returns
    /// the index of the new entry so the host can address it.
    u32 add_watch(std::string expression, std::string name = "");

    /// Remove watch at index.  No-op when out of range.  Returns true
    /// when removal happened so the host knows whether to refresh.
    bool remove_watch(u32 index);

    /// Replace the expression of an existing watch in-place.  Clears
    /// the cached evaluation so the next tick() refreshes.
    bool set_expression(u32 index, std::string expression);

    /// Replace the friendly name of an existing watch (does not
    /// affect evaluation).
    bool set_name(u32 index, std::string name);

    void clear_watches() { watches_.clear(); dirty_ = true; }
    u32 watch_count() const { return static_cast<u32>(watches_.size()); }
    const std::vector<WatchEntry>& watches() const { return watches_; }
    const WatchEntry* watch_at(u32 index) const {
        return index < watches_.size() ? &watches_[index] : nullptr;
    }

    /// Evaluate every watch once through the bound Evaluator.  Safe to
    /// call without an evaluator: every entry stays in its cached
    /// state.  Throttle via `set_eval_interval()` so the editor doesn't
    /// thrash the script engine on every frame.
    void tick();

    /// Frame-rate-bounded variant: invoked with the elapsed time since
    /// the last tick; eval only fires when the accumulator crosses
    /// `eval_interval()`.  Lets editor_main call tick_interval(dt) in
    /// the docked-loop without bookkeeping.
    void tick_interval(f32 dt);

    /// Re-evaluate immediately on the next tick(), bypassing the
    /// throttle.  Used by the inline "Refresh" button.
    void mark_dirty() { eval_accumulator_ = eval_interval_; }

    /// Build the clipboard text for one row (M39).  Format:
    ///   "<name> = <last_value>"           when ok
    ///   "<name>: <last_error>"            when !ok
    ///   "<name> = (not evaluated yet)"    before the first tick
    /// Empty string when index is out of range.  Pure helper so tests
    /// can verify the format without exercising ImGui's clipboard.
    std::string format_row_for_clipboard(u32 index) const;

    f32 eval_interval() const { return eval_interval_; }
    void set_eval_interval(f32 sec) { eval_interval_ = sec < 0.0f ? 0.0f : sec; }

    // ── Persistence (parity with ParticleEditorPanel / LuaConsolePanel) ──
    /// Serialise the watch list (names + expressions) into JSON.  The
    /// last_value / last_error fields are deliberately *not* serialised
    /// — they're transient runtime state.  Round-trip-safe with
    /// load_from_json().
    std::string save_to_json() const;
    bool        load_from_json(const std::string& json);
    bool        save_to_file(const std::string& path) const;
    bool        load_from_file(const std::string& path);

    // ── Dirty-flag persistence (M38) ───────────────────────────────────
    //
    // The panel marks itself dirty whenever the watch list mutates
    // (add/remove/set_expression/set_name/clear).  Successful
    // save_to_file / load_from_file / load_from_json clear the flag.
    // editor_main checks is_dirty() on shutdown (and at a periodic
    // throttle) to decide whether the on-disk watches.json needs to
    // be rewritten — this keeps pinned expressions across crashes
    // without doing disk I/O on every keystroke.
    bool is_dirty() const { return dirty_; }
    void clear_dirty() { dirty_ = false; }
    void mark_persistence_dirty() { dirty_ = true; }

private:
    std::vector<WatchEntry> watches_;
    Evaluator               evaluator_;
    f32                     eval_interval_{0.1f};   // 10 Hz default
    f32                     eval_accumulator_{0.0f};
    bool                    dirty_{false};
};

} // namespace nexus::editor
