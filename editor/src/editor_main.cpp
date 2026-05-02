// ============================================================================
// editor_main.cpp — NexusEngine Editor entry point
// ============================================================================

#include "nexus/core/log.h"
#include "nexus/core/timer.h"
#include "nexus/core/math.h"
#include "nexus/platform/window.h"
#include "nexus/platform/input.h"
#include "nexus/rhi/rhi.h"
#include "nexus/rhi/gl_functions.h"
#include "nexus/scene/scene.h"
#include "nexus/scene/components.h"
#include "nexus/scene/hierarchy.h"
#include "nexus/scene/scene_serializer.h"
#include "nexus/renderer/forward_renderer_3d.h"
#include "nexus/renderer/batch_renderer_2d.h"
#include "nexus/renderer/debug_renderer.h"
#include "nexus/audio/audio_engine.h"
#include "nexus/audio/audio_device.h"
#include "nexus/editor/editor_state.h"
#include "nexus/editor/editor_panels.h"
#include "nexus/editor/asset_thumbnail_cache.h"
#include "nexus/editor/component_registry.h"
#include "nexus/editor/asset_drop_importer.h"
#include "nexus/editor/layer_registry.h"
#include "nexus/editor/material_asset.h"
#include "nexus/editor/prefab_asset.h"
#include "nexus/editor/animation_asset.h"
#include "nexus/editor/audio_asset.h"
#include "nexus/editor/build_scenes.h"
#include "nexus/animation/animator_system.h"
#include "nexus/perf/profiler.h"
#include "nexus/assets/asset_registry.h"

#define GLFW_INCLUDE_NONE
#include "imgui.h"
#include "ImGuizmo.h"
#include <GLFW/glfw3.h>

#include <spdlog/sinks/base_sink.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>
#include <stdexcept>

// Forward declare GLFW proc address getter and window type
struct GLFWwindow;
extern "C" {
    typedef void (*GLFWglproc)(void);
    GLFWglproc glfwGetProcAddress(const char* procname);
}

namespace nexus::editor {

namespace {

// Numeric mesh ids the editor pre-registers with the viewport so
// MeshRendererComponent records produced by the Hierarchy "Create" menu and
// the default-scene seeder resolve to real GPU meshes.  These ids form the
// editor's small built-in primitive library — equivalent to Unity's hidden
// "Cube.fbx" / "Sphere.fbx" / "Plane.fbx".  User-imported meshes use higher ids.
constexpr u32 kPrimitiveCubeMeshId   = 1;
constexpr u32 kPrimitivePlaneMeshId  = 2;
constexpr u32 kPrimitiveSphereMeshId = 3;

/// spdlog sink that forwards engine log messages to the editor's Console
/// panel.  Registered on the engine logger so every NX_INFO / NX_WARN / etc.
/// call surfaces inside the editor UI instead of only the terminal.
class ConsolePanelSink : public spdlog::sinks::base_sink<std::mutex> {
public:
    explicit ConsolePanelSink(ConsolePanel* panel) : panel_(panel) {}

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        if (!panel_) return;
        const std::string text(msg.payload.data(), msg.payload.size());
        LogLevel level = LogLevel::Info;
        switch (msg.level) {
            case spdlog::level::trace:
            case spdlog::level::debug:    level = LogLevel::Debug;   break;
            case spdlog::level::info:     level = LogLevel::Info;    break;
            case spdlog::level::warn:     level = LogLevel::Warning; break;
            case spdlog::level::err:
            case spdlog::level::critical: level = LogLevel::Error;   break;
            default:                      level = LogLevel::Info;    break;
        }
        panel_->add_message(text, level);
    }
    void flush_() override {}

private:
    ConsolePanel* panel_{nullptr};
};

// `register_dropped_asset` was the placeholder that just allocated an id
// without decoding bytes.  AssetDropImporter now performs real decode +
// upload, so the helper is gone.  Stable AssetIds for dropped paths are
// still recorded by AssetDropImporter::ensure_registered() so scene
// save/load round-trips remain stable.

/// Walk a directory (one level) and fill the asset browser's entry list.  The
/// panel's search/grid UI already handles filtering and rendering — it just
/// needs entries to display.
void populate_asset_browser(AssetBrowserPanel& panel, const std::string& dir) {
    namespace fs = std::filesystem;
    std::vector<AssetBrowserEntry> entries;
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
        panel.set_entries(std::move(entries));
        return;
    }
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        AssetBrowserEntry e;
        e.name         = entry.path().filename().string();
        e.path         = entry.path().string();
        e.is_directory = entry.is_directory(ec);
        e.extension    = e.is_directory ? "" : entry.path().extension().string();
        e.file_size    = e.is_directory ? 0u : fs::file_size(entry.path(), ec);
        entries.push_back(std::move(e));
    }
    // Directories first, then alphabetical.
    std::sort(entries.begin(), entries.end(),
              [](const AssetBrowserEntry& a, const AssetBrowserEntry& b) {
                  if (a.is_directory != b.is_directory) return a.is_directory;
                  return a.name < b.name;
              });
    panel.set_entries(std::move(entries));
}

/// Recursively walk a directory subtree and produce a flat entry list.  Used
/// when the panel's "Recursive" toggle is on AND a search term is set so
/// users can find files anywhere in the project — Unity Project view parity.
/// Capped at `max_entries` so a misconfigured root (e.g. CWD == $HOME) cannot
/// hang the editor.  Hidden files / dirs (leading dot) are skipped to keep
/// .git, build/, and editor caches out of the listing.
void populate_asset_browser_recursive(AssetBrowserPanel& panel,
                                      const std::string& root,
                                      size_t max_entries = 4000) {
    namespace fs = std::filesystem;
    std::vector<AssetBrowserEntry> entries;
    std::error_code ec;
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) {
        panel.set_entries(std::move(entries));
        return;
    }
    fs::recursive_directory_iterator it(
        root,
        fs::directory_options::skip_permission_denied,
        ec);
    fs::recursive_directory_iterator end;
    while (!ec && it != end && entries.size() < max_entries) {
        const auto& path = it->path();
        const std::string name = path.filename().string();
        if (!name.empty() && name[0] == '.') {
            it.disable_recursion_pending();
            it.increment(ec);
            continue;
        }
        AssetBrowserEntry e;
        e.name         = name;
        e.path         = path.string();
        e.is_directory = it->is_directory(ec);
        e.extension    = e.is_directory ? "" : path.extension().string();
        e.file_size    = e.is_directory ? 0u : fs::file_size(path, ec);
        entries.push_back(std::move(e));
        it.increment(ec);
    }
    // For recursive listings, sort alphabetically by full path so co-located
    // files cluster — directories-first ordering loses meaning across nesting.
    std::sort(entries.begin(), entries.end(),
              [](const AssetBrowserEntry& a, const AssetBrowserEntry& b) {
                  return a.path < b.path;
              });
    panel.set_entries(std::move(entries));
}

/// Populate the panel with project-wide files matching `predicate`.  Used by
/// the sidebar virtual collections (Favorites / All Materials / All Models /
/// All Prefabs) — entries are flat and sorted by path.  Same hidden-file and
/// max-count guards as the recursive walker so a runaway scan can't lock the
/// editor.
template <typename Predicate>
void populate_asset_browser_filtered(AssetBrowserPanel& panel,
                                     const std::string& root,
                                     Predicate&& predicate,
                                     size_t max_entries = 4000) {
    namespace fs = std::filesystem;
    std::vector<AssetBrowserEntry> entries;
    std::error_code ec;
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) {
        panel.set_entries(std::move(entries));
        return;
    }
    fs::recursive_directory_iterator it(
        root,
        fs::directory_options::skip_permission_denied,
        ec);
    fs::recursive_directory_iterator end;
    while (!ec && it != end && entries.size() < max_entries) {
        const auto& path = it->path();
        const std::string name = path.filename().string();
        if (!name.empty() && name[0] == '.') {
            it.disable_recursion_pending();
            it.increment(ec);
            continue;
        }
        const bool is_dir = it->is_directory(ec);
        if (!is_dir && predicate(path)) {
            AssetBrowserEntry e;
            e.name         = name;
            e.path         = path.string();
            e.is_directory = false;
            e.extension    = path.extension().string();
            e.file_size    = fs::file_size(path, ec);
            entries.push_back(std::move(e));
        }
        it.increment(ec);
    }
    std::sort(entries.begin(), entries.end(),
              [](const AssetBrowserEntry& a, const AssetBrowserEntry& b) {
                  return a.path < b.path;
              });
    panel.set_entries(std::move(entries));
}

/// Materialize the panel's Favorites set into a flat entry list.  Drops paths
/// that no longer exist on disk so the Favorites view never shows stale rows.
void populate_asset_browser_favorites(AssetBrowserPanel& panel) {
    namespace fs = std::filesystem;
    std::vector<AssetBrowserEntry> entries;
    std::error_code ec;
    for (const auto& path : panel.favorites()) {
        if (!fs::exists(path, ec)) continue;
        AssetBrowserEntry e;
        e.path         = path;
        e.name         = fs::path(path).filename().string();
        e.is_directory = fs::is_directory(path, ec);
        e.extension    = e.is_directory ? "" : fs::path(path).extension().string();
        e.file_size    = e.is_directory ? 0u : fs::file_size(path, ec);
        entries.push_back(std::move(e));
    }
    std::sort(entries.begin(), entries.end(),
              [](const AssetBrowserEntry& a, const AssetBrowserEntry& b) {
                  if (a.is_directory != b.is_directory) return a.is_directory;
                  return a.name < b.name;
              });
    panel.set_entries(std::move(entries));
}

/// Configure a single entity as the scene's primary perspective camera, aimed
/// at the origin from a comfortable orbit position.  Pulled out of
/// `populate_default_scene` so File > New Scene can re-use it.
void configure_default_camera(Scene& scene, Entity e) {
    auto& reg = scene.registry();
    auto& tc = reg.get_component<Transform3DComponent>(e);
    tc.position = Vec3(6.0f, 4.0f, 6.0f);

    // Aim the camera at the origin using the same YXZ Tait-Bryan convention
    // ViewportPanel decodes back into yaw/pitch.
    Vec3 dir = glm::normalize(-tc.position);
    float pitch = std::asin(math::clamp(dir.y, -0.9999f, 0.9999f));
    float yaw   = std::atan2(dir.z, dir.x);
    Quat q_yaw   = glm::angleAxis(yaw,   Vec3(0.0f, 1.0f, 0.0f));
    Quat q_pitch = glm::angleAxis(pitch, Vec3(1.0f, 0.0f, 0.0f));

    auto& cc = reg.add_component<CameraComponent>(e, CameraComponent{});
    cc.is_primary      = true;
    cc.is_orthographic = false;
    cc.fov             = 60.0f;
    cc.near_clip       = 0.1f;
    cc.far_clip        = 200.0f;
    cc.orientation     = q_yaw * q_pitch;
}

/// Seed an empty scene with the standard set of objects every fresh scene
/// gets in Unity / Godot: a Main Camera, a Sun (directional light), and a
/// ground plane the user can build on top of.  Everything is created through
/// the same `create_entity_3d` + `add_component` API the Hierarchy panel uses,
/// so there is no special-case update path — `Hierarchy::propagate_transforms_3d`
/// drives `world_matrix` for these entities the same as for user-created ones.
void populate_default_scene(Scene& scene) {
    auto& reg = scene.registry();

    Entity camera = scene.create_entity_3d("Main Camera");
    configure_default_camera(scene, camera);

    Entity sun = scene.create_entity_3d("Directional Light");
    {
        auto& tc = reg.get_component<Transform3DComponent>(sun);
        tc.position = Vec3(0.0f, 5.0f, 0.0f);
        auto& dl = reg.add_component<DirectionalLightComponent>(
            sun, DirectionalLightComponent{});
        dl.direction = Vec3(-0.5f, -1.0f, -0.3f);
        dl.color     = Vec3(1.0f, 0.95f, 0.85f);
        dl.intensity = 1.0f;
    }

    Entity ground = scene.create_entity_3d("Ground");
    {
        auto& tc = reg.get_component<Transform3DComponent>(ground);
        tc.position = Vec3(0.0f, 0.0f, 0.0f);
        auto& mr = reg.add_component<MeshRendererComponent>(
            ground, MeshRendererComponent{});
        mr.mesh_id = kPrimitivePlaneMeshId;
        mr.tint    = Vec4(0.45f, 0.5f, 0.45f, 1.0f);
    }
}

/// File-dialog wrapper that prefers GLFW's native dialog when present and
/// falls back to a hard-coded "scene.json" so the menu still "works" in
/// minimal environments.  Returning empty string means "cancelled".
/// We intentionally avoid pulling in a third-party file-dialog dependency —
/// for now the menu writes to a deterministic path next to the executable.
/// TODO: integrate nfd/portable-file-dialogs once a dependency budget allows.
std::string default_scene_save_path() {
    namespace fs = std::filesystem;
    fs::path p = fs::current_path() / "scene.json";
    return p.string();
}

} // namespace (anonymous)

static void register_default_panels(EditorState& state) {
    auto& panels = state.panels();
    // Two viewports — Unity-style Scene + Game tabs.  Both render through
    // their own off-screen FBO and display via ImGui::Image, so they dock and
    // resize like any other panel.
    panels.add_panel(std::make_unique<ViewportPanel>("Scene",
        ViewportCameraMode::SceneView));
    panels.add_panel(std::make_unique<ViewportPanel>("Game",
        ViewportCameraMode::GameView));
    panels.add_panel(std::make_unique<HierarchyPanel>());
    panels.add_panel(std::make_unique<InspectorPanel>());
    panels.add_panel(std::make_unique<ConsolePanel>());
    panels.add_panel(std::make_unique<AssetBrowserPanel>());
    // Phase B Unity-parity panels — hidden by default so the first-launch
    // layout stays clean; toggled via View menu.
    {
        auto undo_hist = std::make_unique<UndoHistoryPanel>();
        undo_hist->set_visible(false);
        panels.add_panel(std::move(undo_hist));
    }
    {
        auto profiler_panel = std::make_unique<ProfilerPanel>();
        profiler_panel->set_visible(false);
        panels.add_panel(std::move(profiler_panel));
    }

    // Set up default dock layout.
    auto& dock = panels.dock_space();
    dock.dock("Scene",         DockPosition::Center);
    dock.dock("Game",          DockPosition::Center);
    dock.dock("Hierarchy",     DockPosition::Left,   0.20f);
    dock.dock("Inspector",     DockPosition::Right,  0.25f);
    dock.dock("Console",       DockPosition::Bottom, 0.25f);
    dock.dock("Asset Browser", DockPosition::Bottom, 0.25f);
    dock.dock("Undo History",  DockPosition::Right,  0.25f);
    dock.dock("Profiler",      DockPosition::Bottom, 0.30f);
}

static int run(int /*argc*/, char* /*argv*/[]) {
    // ── Initialize core systems ─────────────────────────────────────────
    Log::init();
    NX_INFO("NexusEngine Editor starting...");

    try {
        WindowConfig window_config;
        window_config.title  = "NexusEngine Editor";
        window_config.width  = 1600;
        window_config.height = 900;
        window_config.vsync  = true;

        Window window(window_config);
        NX_INFO("Window created: {}x{}", window_config.width, window_config.height);

        Input::init(window.native_handle());

        // Load OpenGL functions
        if (!rhi::gl::load(reinterpret_cast<rhi::gl::GLLoadProc>(glfwGetProcAddress))) {
            NX_ERROR("Failed to load OpenGL functions");
            Log::shutdown();
            return 1;
        }

        // ── Create RHI ──────────────────────────────────────────────────
        auto rhi = rhi::RHI::create();
        if (!rhi || !rhi->init()) {
            NX_ERROR("Failed to initialize RHI");
            Log::shutdown();
            return 1;
        }

        // ── Initialize Dear ImGui ───────────────────────────────────────
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;        // dockable panels
        // ConfigFlags_ViewportsEnable would let panels become OS windows; we
        // skip that to keep the GLFW backend single-window for now.
        ImGui::StyleColorsDark();

        // Custom theme tuned to match the EditorPreview reference: Unity-like
        // dark grey panels (#2D3035), cool-blue selection (#3D6AA8), tight
        // corners, slightly larger row spacing.  Keeps ImGui defaults for any
        // colors not overridden so future widgets get sensible behavior.
        {
            ImGuiStyle& s = ImGui::GetStyle();
            s.WindowRounding    = 4.0f;
            s.ChildRounding     = 3.0f;
            s.FrameRounding     = 3.0f;
            s.GrabRounding      = 3.0f;
            s.PopupRounding     = 4.0f;
            s.TabRounding       = 3.0f;
            s.ScrollbarRounding = 4.0f;
            s.WindowBorderSize  = 1.0f;
            s.FrameBorderSize   = 0.0f;
            s.WindowPadding     = ImVec2(8, 8);
            s.FramePadding      = ImVec2(6, 4);
            s.ItemSpacing       = ImVec2(6, 4);
            s.ItemInnerSpacing  = ImVec2(4, 4);
            s.IndentSpacing     = 18.0f;
            s.ScrollbarSize     = 12.0f;
            s.GrabMinSize       = 10.0f;

            ImVec4* c = s.Colors;
            c[ImGuiCol_Text]                  = ImVec4(0.86f, 0.87f, 0.89f, 1.0f);
            c[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.52f, 0.55f, 1.0f);
            c[ImGuiCol_WindowBg]              = ImVec4(0.176f, 0.184f, 0.20f, 1.0f);
            c[ImGuiCol_ChildBg]               = ImVec4(0.165f, 0.173f, 0.188f, 1.0f);
            c[ImGuiCol_PopupBg]               = ImVec4(0.16f, 0.17f, 0.19f, 0.98f);
            c[ImGuiCol_Border]                = ImVec4(0.10f, 0.11f, 0.12f, 1.0f);
            c[ImGuiCol_BorderShadow]          = ImVec4(0, 0, 0, 0);
            c[ImGuiCol_FrameBg]               = ImVec4(0.118f, 0.125f, 0.137f, 1.0f);
            c[ImGuiCol_FrameBgHovered]        = ImVec4(0.220f, 0.235f, 0.255f, 1.0f);
            c[ImGuiCol_FrameBgActive]         = ImVec4(0.260f, 0.282f, 0.310f, 1.0f);
            c[ImGuiCol_TitleBg]               = ImVec4(0.145f, 0.153f, 0.165f, 1.0f);
            c[ImGuiCol_TitleBgActive]         = ImVec4(0.165f, 0.173f, 0.188f, 1.0f);
            c[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.118f, 0.125f, 0.137f, 1.0f);
            c[ImGuiCol_MenuBarBg]             = ImVec4(0.145f, 0.153f, 0.165f, 1.0f);
            c[ImGuiCol_ScrollbarBg]           = ImVec4(0.118f, 0.125f, 0.137f, 1.0f);
            c[ImGuiCol_ScrollbarGrab]         = ImVec4(0.30f, 0.32f, 0.35f, 1.0f);
            c[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.38f, 0.41f, 0.45f, 1.0f);
            c[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.46f, 0.50f, 0.55f, 1.0f);
            c[ImGuiCol_CheckMark]             = ImVec4(0.36f, 0.66f, 1.0f, 1.0f);
            c[ImGuiCol_SliderGrab]            = ImVec4(0.38f, 0.55f, 0.85f, 1.0f);
            c[ImGuiCol_SliderGrabActive]      = ImVec4(0.46f, 0.65f, 0.95f, 1.0f);
            c[ImGuiCol_Button]                = ImVec4(0.235f, 0.247f, 0.275f, 1.0f);
            c[ImGuiCol_ButtonHovered]         = ImVec4(0.30f, 0.32f, 0.36f, 1.0f);
            c[ImGuiCol_ButtonActive]          = ImVec4(0.36f, 0.39f, 0.44f, 1.0f);
            c[ImGuiCol_Header]                = ImVec4(0.235f, 0.247f, 0.275f, 1.0f);
            c[ImGuiCol_HeaderHovered]         = ImVec4(0.30f, 0.40f, 0.55f, 1.0f);
            c[ImGuiCol_HeaderActive]          = ImVec4(0.36f, 0.50f, 0.70f, 1.0f);
            c[ImGuiCol_Separator]             = ImVec4(0.10f, 0.11f, 0.12f, 1.0f);
            c[ImGuiCol_SeparatorHovered]      = ImVec4(0.30f, 0.40f, 0.55f, 1.0f);
            c[ImGuiCol_SeparatorActive]       = ImVec4(0.36f, 0.50f, 0.70f, 1.0f);
            c[ImGuiCol_ResizeGrip]            = ImVec4(0.235f, 0.247f, 0.275f, 0.5f);
            c[ImGuiCol_ResizeGripHovered]     = ImVec4(0.30f, 0.40f, 0.55f, 0.78f);
            c[ImGuiCol_ResizeGripActive]      = ImVec4(0.36f, 0.50f, 0.70f, 1.0f);
            c[ImGuiCol_Tab]                   = ImVec4(0.165f, 0.173f, 0.188f, 1.0f);
            c[ImGuiCol_TabHovered]            = ImVec4(0.30f, 0.40f, 0.55f, 1.0f);
            c[ImGuiCol_TabActive]             = ImVec4(0.235f, 0.247f, 0.275f, 1.0f);
            c[ImGuiCol_TabUnfocused]          = ImVec4(0.145f, 0.153f, 0.165f, 1.0f);
            c[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.20f, 0.21f, 0.23f, 1.0f);
            c[ImGuiCol_DockingPreview]        = ImVec4(0.36f, 0.50f, 0.70f, 0.7f);
            c[ImGuiCol_DockingEmptyBg]        = ImVec4(0.118f, 0.125f, 0.137f, 1.0f);
            c[ImGuiCol_TableHeaderBg]         = ImVec4(0.165f, 0.173f, 0.188f, 1.0f);
            c[ImGuiCol_TableBorderStrong]     = ImVec4(0.10f, 0.11f, 0.12f, 1.0f);
            c[ImGuiCol_TableBorderLight]      = ImVec4(0.16f, 0.17f, 0.19f, 1.0f);
            c[ImGuiCol_TableRowBg]            = ImVec4(0, 0, 0, 0);
            c[ImGuiCol_TableRowBgAlt]         = ImVec4(1, 1, 1, 0.025f);
            c[ImGuiCol_TextSelectedBg]        = ImVec4(0.30f, 0.40f, 0.55f, 0.65f);
        }

        // Hand off both ImGui platform + renderer binding to the active RHI
        // backend.  This keeps the editor backend-agnostic — we never touch
        // imgui_impl_*.h directly.  The RHI knows whether to install
        // GLFW+OpenGL3, GLFW+Metal, GLFW+Vulkan, or the WebGL pair.
        if (!rhi->imgui_init(window.native_handle())) {
            NX_ERROR("RHI failed to install ImGui backend — editor cannot run "
                     "on this graphics backend.");
            ImGui::DestroyContext();
            Log::shutdown();
            return 1;
        }
        NX_INFO("Dear ImGui initialized (version {})", IMGUI_VERSION);

        // ── Create audio subsystem with device output ───────────────────
        audio::AudioEngine audio;
        audio::AudioDevice audio_device;
        if (!audio_device.open(&audio)) {
            NX_WARN("Audio device failed to open — audio will be silent");
        }

        // ── Create scene ────────────────────────────────────────────────
        Scene scene;

        // ── Create renderers ───────────────────────────────────────────
        ForwardRenderer3D renderer_3d;
        renderer_3d.init(rhi.get());
        BatchRenderer2D renderer_2d;
        renderer_2d.init(rhi.get());
        DebugRenderer debug_renderer;
        debug_renderer.init(rhi.get());

        // ── Built-in primitive meshes ──────────────────────────────────
        // Owned here so they outlive every panel that borrows pointers.
        // The viewport's mesh registry only stores raw pointers, so dropping
        // these earlier than the panel would dangle on shutdown.
        Mesh primitive_cube   = create_cube_mesh();
        Mesh primitive_plane  = create_plane_mesh(20.0f, 4);
        Mesh primitive_sphere = create_sphere_mesh(0.5f, 16, 32);
        renderer_3d.upload_mesh(primitive_cube);
        renderer_3d.upload_mesh(primitive_plane);
        renderer_3d.upload_mesh(primitive_sphere);

        // ── Create editor state ─────────────────────────────────────────
        // editor_profiler must outlive editor_state: ProfilerPanel (owned by
        // editor_state.panels()) borrows a raw pointer to it.  C++ destroys
        // locals in reverse order, so declaring the profiler *first* here
        // guarantees the panel is torn down before the profiler.
        Profiler editor_profiler;
        // Shared asset registry for the editor session — survives scene
        // save/load round-trips so dropped assets keep the same handle.
        nexus::assets::AssetRegistry editor_assets;
        // Async thumbnail cache.  Constructed before EditorState so it
        // outlives the panels that consult it; bound to the live RHI right
        // after rhi->init() so create_texture is safe to call from drain().
        nexus::editor::AssetThumbnailCache thumbnail_cache(nullptr, 256);
        thumbnail_cache.set_rhi(rhi.get());

        // Drop importer — owns the live engine resources (RHI textures,
        // engine Mesh objects) for assets dragged into the editor.  Bound
        // after viewports are constructed below.
        nexus::editor::AssetDropImporter drop_importer;
        drop_importer.set_rhi(rhi.get());
        drop_importer.set_renderer_3d(&renderer_3d);
        drop_importer.set_asset_registry(&editor_assets);

        // Component metadata registry for the Inspector's Add Component menu.
        // Built-ins registered up-front; user code can call register_type()
        // for custom components without modifying this file.
        nexus::editor::ComponentRegistry component_registry;
        nexus::editor::register_builtin_components(component_registry);

        // Layer registry — names + visibility mask for the engine's 32
        // layers.  Defaults match Unity's reserved set.  Toolbar's "Layers"
        // dropdown reads/writes this; the viewport panel honours the mask
        // when walking entities (TODO: wire when renderer-level filtering
        // lands).  For now the editor surfaces the UX so projects can
        // configure layer names that scenes serialise.
        nexus::editor::LayerRegistry layer_registry;

        // Active layout preset — drives the Window > Layout menu.  The
        // preset id lives in memory; switching presets reloads ImGui's ini
        // from a per-preset file so each layout has independent dock state.
        nexus::editor::LayoutPreset layout_preset =
            nexus::editor::LayoutPreset::Default;

        // Material asset cache — owns MaterialData by path, JSON read/write,
        // stable id allocation.  Drop a .mat onto the viewport / hierarchy
        // and the cache persists the binding across scene save/load.
        nexus::editor::MaterialAssetCache material_cache;
        material_cache.set_asset_registry(&editor_assets);

        // Prefab asset cache — owns Prefab JSON by path, instantiates on
        // demand.  Drag a .prefab onto the viewport to spawn an instance;
        // the editor's "Save as Prefab" path also routes through here.
        nexus::editor::PrefabAssetCache prefab_cache;
        prefab_cache.set_asset_registry(&editor_assets);

        // Animation asset cache — owns AnimationClip JSON by path.  Drop a
        // .anim file to register it for later binding to an AnimatorComponent
        // (component slot wiring lands when AnimatorComponent itself does).
        nexus::editor::AnimationAssetCache animation_cache;
        animation_cache.set_asset_registry(&editor_assets);

        // Audio asset cache — bridges drag-dropped .wav/.ogg files to
        // AudioEngine's clip registry and exposes a stable AudioClipId
        // for AudioSourceComponent.clip_id binding.
        nexus::editor::AudioAssetCache audio_cache;
        audio_cache.set_audio_engine(&audio);
        audio_cache.set_asset_registry(&editor_assets);

        // Build scenes — Unity-style "Scenes In Build" list.  Persisted as
        // a sibling JSON file so the project's build manifest survives
        // editor restarts.  open_scene below auto-adds new paths so the
        // user's recent-scenes story stays maintained without a panel UI.
        nexus::editor::BuildScenes build_scenes;
        const std::string build_scenes_path = "build_scenes.json";
        // Best-effort load; absence is normal for a fresh project.
        (void)build_scenes.load_from_file(build_scenes_path);

        // Animator system — drives AnimatorComponent playback every Play
        // mode tick.  Resolver hands the system a way to look up the
        // engine-side AnimationClip from the editor cache's stable id.
        nexus::anim::AnimatorSystem animator_system(
            [&animation_cache](u32 id) {
                return animation_cache.get_by_id(id);
            });

        EditorState editor_state;
        register_default_panels(editor_state);
        editor_state.set_status("Ready");

        // Wire Scene + Game viewports to the scene/renderers/RHI.
        ViewportPanel* scene_vp = editor_state.panels()
            .find_typed<ViewportPanel>("Scene");
        ViewportPanel* game_vp  = editor_state.panels()
            .find_typed<ViewportPanel>("Game");
        for (ViewportPanel* vp : { scene_vp, game_vp }) {
            if (!vp) continue;
            vp->bind_scene(&scene);
            vp->bind_renderer_3d(&renderer_3d);
            vp->bind_renderer_2d(&renderer_2d);
            vp->bind_debug_renderer(&debug_renderer);
            vp->bind_rhi(rhi.get());
            vp->set_viewport_size(static_cast<u32>(window_config.width),
                                  static_cast<u32>(window_config.height));
            vp->register_mesh(kPrimitiveCubeMeshId,   &primitive_cube);
            vp->register_mesh(kPrimitivePlaneMeshId,  &primitive_plane);
            vp->register_mesh(kPrimitiveSphereMeshId, &primitive_sphere);
            drop_importer.add_viewport(vp);
        }

        // Wire Hierarchy + Inspector to the scene so the editor shows the
        // entity tree and component editors rather than "No scene loaded".
        HierarchyPanel* hierarchy = editor_state.panels()
            .find_typed<HierarchyPanel>("Hierarchy");
        InspectorPanel* inspector = editor_state.panels()
            .find_typed<InspectorPanel>("Inspector");
        if (hierarchy) {
            hierarchy->bind_scene(&scene);
            // Centralize selection in EditorState — Hierarchy now reads/writes
            // through EditorSelection so Inspector / Viewport / Snapshot all
            // share one source of truth.
            hierarchy->bind_selection(&editor_state.selection());
            hierarchy->bind_undo_manager(&editor_state.undo_redo());
            hierarchy->set_primitive_mesh_ids(kPrimitiveCubeMeshId,
                                              kPrimitivePlaneMeshId,
                                              kPrimitiveSphereMeshId);
            // Asset-drop callback wired below alongside open_scene/create_object.
            // Let viewports draw an ImGuizmo handle on the hierarchy's selected
            // entity, and accept F-to-frame on it.
            for (ViewportPanel* vp : { scene_vp, game_vp }) {
                if (vp) vp->bind_hierarchy(hierarchy);
            }
        }
        if (inspector) {
            inspector->bind_scene(&scene);
            inspector->bind_undo_manager(&editor_state.undo_redo());
            inspector->bind_selection(&editor_state.selection());
            inspector->bind_component_registry(&component_registry);
            // Hand the Inspector a way to translate raw asset ids back into
            // human-readable filenames.  Each cache exposes a stable
            // path_for(id) that the picker calls when rendering the slot.
            nexus::editor::InspectorPanel::AssetPathResolvers resolvers;
            resolvers.material  = [&](u32 id) { return material_cache.path_for(id); };
            resolvers.animation = [&](u32 id) { return animation_cache.path_for(id); };
            resolvers.audio     = [&](u32 id) { return audio_cache.path_for(id); };
            resolvers.prefab    = [&](u32 id) { return prefab_cache.path_for(id); };
            inspector->bind_asset_path_resolvers(std::move(resolvers));
        }

        // Route engine log output (NX_INFO / NX_WARN / …) into the Console
        // panel — otherwise the panel would always be empty at runtime.
        ConsolePanel* console = editor_state.panels()
            .find_typed<ConsolePanel>("Console");
        if (console) {
            if (auto& eng = Log::engine_logger()) {
                eng->sinks().push_back(
                    std::make_shared<ConsolePanelSink>(console));
            }
            if (auto& app = Log::app_logger()) {
                app->sinks().push_back(
                    std::make_shared<ConsolePanelSink>(console));
            }
        }

        // RAII guard: strip ConsolePanelSink entries from all loggers on any
        // exit path (normal return, std::exception, uncaught abort during
        // unwind).  This local must be declared *after* editor_state so its
        // destructor runs *before* the ConsolePanel is freed; otherwise
        // Log::shutdown()'s final NX_INFO triggers a heap-use-after-free via
        // the dangling raw pointer inside the sink.
        struct ConsoleSinkGuard {
            ~ConsoleSinkGuard() {
                auto strip = [](std::shared_ptr<spdlog::logger>& logger) {
                    if (!logger) return;
                    auto& sinks = logger->sinks();
                    sinks.erase(
                        std::remove_if(
                            sinks.begin(), sinks.end(),
                            [](const spdlog::sink_ptr& s) {
                                return dynamic_cast<ConsolePanelSink*>(s.get())
                                       != nullptr;
                            }),
                        sinks.end());
                };
                strip(Log::engine_logger());
                strip(Log::app_logger());
            }
        } console_sink_guard;

        // Wire the Undo History + Profiler panels.  editor_profiler was
        // declared above so that it outlives editor_state.
        if (auto* uh = editor_state.panels()
                .find_typed<UndoHistoryPanel>("Undo History")) {
            uh->bind_undo_manager(&editor_state.undo_redo());
        }
        if (auto* pp = editor_state.panels()
                .find_typed<ProfilerPanel>("Profiler")) {
            pp->bind_profiler(&editor_profiler);
        }

        // Populate the Asset Browser from the project's assets/ directory so
        // it shows real files (shaders, scenes, textures) on startup.
        AssetBrowserPanel* assets_panel = editor_state.panels()
            .find_typed<AssetBrowserPanel>("Asset Browser");
        if (assets_panel) {
            namespace fs = std::filesystem;
            const char* candidates[] = {
                "assets", "../assets", "../../assets",
                "examples", "../examples",
                ".",
            };
            std::string root = ".";
            for (const char* c : candidates) {
                if (fs::exists(c) && fs::is_directory(c)) { root = c; break; }
            }
            // Resolve to an absolute path so the breadcrumb trail and the
            // navigate_to() root cap are stable regardless of CWD changes.
            std::error_code ec;
            const auto canonical_root = fs::canonical(root, ec);
            if (!ec) root = canonical_root.string();
            assets_panel->set_root_path(root);
            assets_panel->navigate_to(root);
            populate_asset_browser(*assets_panel, root);
            // Host callbacks (Open/Reveal/Delete/Refresh) are wired below,
            // after `open_scene` is declared so we can route .nxs/.json
            // double-clicks straight into the loader.
        }

        // Seed the empty scene with Camera + Sun + Ground so the viewports
        // show useful content on first launch — same default-scene contract
        // Unity / Godot ship with.  Everything goes through the regular
        // create_entity_3d / add_component pipeline so the unconditional
        // Hierarchy::propagate_transforms_3d call below feeds world_matrix
        // for these entities the same as for user-created ones.
        populate_default_scene(scene);
        NX_INFO("Default scene loaded (Main Camera, Directional Light, Ground)");

        // ── GameObject menu factory ─────────────────────────────────────
        // Mirrors HierarchyPanel's spawn_primitive but lives here so the
        // top-level GameObject menu (Unity-style) can call it.  The menu
        // creates the entity, selects it via the hierarchy panel, and frames
        // the active viewport on it so the user immediately sees the result.
        auto create_object = [&](const char* kind) -> Entity {
            auto& reg = scene.registry();
            Entity e = INVALID_ENTITY;
            if (std::string(kind) == "Empty") {
                e = scene.create_entity("Empty");
            } else if (std::string(kind) == "Cube") {
                e = scene.create_entity_3d("Cube");
                auto& mr = reg.add_component<MeshRendererComponent>(
                    e, MeshRendererComponent{});
                mr.mesh_id = kPrimitiveCubeMeshId;
                mr.tint    = Vec4(0.8f, 0.8f, 0.85f, 1.0f);
            } else if (std::string(kind) == "Sphere") {
                e = scene.create_entity_3d("Sphere");
                auto& mr = reg.add_component<MeshRendererComponent>(
                    e, MeshRendererComponent{});
                mr.mesh_id = kPrimitiveSphereMeshId;
                mr.tint    = Vec4(0.7f, 0.8f, 0.9f, 1.0f);
            } else if (std::string(kind) == "Plane") {
                e = scene.create_entity_3d("Plane");
                auto& mr = reg.add_component<MeshRendererComponent>(
                    e, MeshRendererComponent{});
                mr.mesh_id = kPrimitivePlaneMeshId;
                mr.tint    = Vec4(0.5f, 0.5f, 0.55f, 1.0f);
            } else if (std::string(kind) == "Camera") {
                e = scene.create_entity_3d("Camera");
                configure_default_camera(scene, e);
                // Make new cameras non-primary to avoid hijacking the Game view.
                reg.get_component<CameraComponent>(e).is_primary = false;
            } else if (std::string(kind) == "Directional Light") {
                e = scene.create_entity_3d("Directional Light");
                reg.add_component<DirectionalLightComponent>(
                    e, DirectionalLightComponent{});
            } else if (std::string(kind) == "Point Light") {
                e = scene.create_entity_3d("Point Light");
                reg.add_component<PointLightComponent>(
                    e, PointLightComponent{});
            } else if (std::string(kind) == "Spot Light") {
                e = scene.create_entity_3d("Spot Light");
                reg.add_component<SpotLightComponent>(
                    e, SpotLightComponent{});
            } else if (std::string(kind) == "Sprite") {
                e = scene.create_entity_2d("Sprite");
                reg.add_component<SpriteRendererComponent>(
                    e, SpriteRendererComponent{});
            }
            if (e != INVALID_ENTITY && hierarchy) {
                hierarchy->set_selected_entity(static_cast<u32>(e));
            }
            return e;
        };

        // ── Scene file operations ──────────────────────────────────────
        // Single source of truth for File menu actions.  Each action sets the
        // editor's scene path and status so the title bar / status bar reflect
        // the current document.
        auto new_scene = [&]() {
            scene.clear();
            scene.clear_snapshot();
            populate_default_scene(scene);
            editor_state.set_scene_path("");
            editor_state.set_status("New scene");
            if (hierarchy) hierarchy->clear_selection();
            if (inspector) inspector->clear_target();
            NX_INFO("New scene created");
        };
        auto save_scene_as = [&](const std::string& path) {
            if (path.empty()) return false;
            SceneSerializer ser(scene);
            if (!ser.save(path)) {
                NX_ERROR("Failed to save scene to {}", path);
                editor_state.set_status("Save failed");
                return false;
            }
            editor_state.set_scene_path(path);
            editor_state.set_status("Saved: " + path);
            NX_INFO("Scene saved to {}", path);
            // Saving a fresh path adds it to the Build Scenes list so it
            // shows up next time the user opens the build settings UI.
            // Re-adding an existing path is a no-op; persistence is
            // best-effort (failure logs but doesn't fail the save).
            if (build_scenes.add(path)) {
                (void)build_scenes.save_to_file(build_scenes_path);
            }
            return true;
        };
        auto save_scene = [&]() {
            const std::string& path = editor_state.scene_path();
            if (path.empty()) return save_scene_as(default_scene_save_path());
            return save_scene_as(path);
        };
        auto duplicate_selected = [&]() -> bool {
            if (!hierarchy || !hierarchy->has_selection()) return false;
            Entity src = hierarchy->selected_entity();
            if (!scene.registry().alive(src)) return false;
            SceneSerializer ser(scene);
            Entity dup = ser.duplicate_entity(src);
            if (dup == INVALID_ENTITY) {
                editor_state.set_status("Duplicate failed");
                return false;
            }
            hierarchy->set_selected_entity(static_cast<u32>(dup));
            editor_state.set_status("Duplicated entity");
            NX_INFO("Duplicated entity {} -> {}", static_cast<u32>(src),
                    static_cast<u32>(dup));
            return true;
        };
        auto open_scene = [&](const std::string& path) {
            if (path.empty()) return false;
            SceneSerializer ser(scene);
            scene.clear();
            scene.clear_snapshot();
            if (!ser.load(path)) {
                NX_ERROR("Failed to load scene from {}", path);
                // Don't leave the user with a blank scene — re-seed defaults.
                populate_default_scene(scene);
                editor_state.set_status("Load failed");
                return false;
            }
            editor_state.set_scene_path(path);
            editor_state.set_status("Opened: " + path);
            if (build_scenes.add(path)) {
                (void)build_scenes.save_to_file(build_scenes_path);
            }
            if (hierarchy) hierarchy->clear_selection();
            if (inspector) inspector->clear_target();
            NX_INFO("Scene loaded from {}", path);
            return true;
        };

        // ── Play/Pause/Stop via SceneBridge ────────────────────────────
        // EditorState now owns the full lifecycle.  The bridge gives it the
        // hooks it needs (scene snapshot + simulate) without dragging Scene
        // into the editor layer's public API.  Every caller path — menu,
        // toolbar, Ctrl+P, Step-from-Editing — goes through one code path.
        {
            SceneBridge bridge;
            bridge.take_snapshot = [&]() { return scene.take_snapshot(); };
            bridge.restore_snapshot = [&]() { return scene.restore_snapshot(); };
            bridge.clear_snapshot = [&]() { scene.clear_snapshot(); };
            bridge.simulate = [&](f32 dt) {
                // Animator system runs BEFORE scene.update so transforms it
                // writes are visible to physics / rendering this same frame.
                animator_system.tick(scene.registry(), dt);
                scene.update(dt);
            };
            bridge.on_play = [&]() {
                // Animators with `play_on_start` flip to `playing` when the
                // scene enters Play mode — Unity convention.
                nexus::anim::AnimatorSystem::start_autoplay(scene.registry());
                // Clear transient editor UI state tied to entities that may
                // have been re-created by snapshot restore (inspector target
                // holds an Entity id that's invalidated across restore).
                if (inspector) inspector->clear_target();
                // Clear-on-Play in the console is a user-toggled option —
                // ConsolePanel decides whether to honor it.
                if (auto* c = editor_state.panels()
                        .find_typed<ConsolePanel>("Console")) {
                    c->on_enter_play();
                }
            };
            bridge.on_stop = [&]() {
                if (hierarchy) hierarchy->clear_selection();
                if (inspector) inspector->clear_target();
            };
            editor_state.set_scene_bridge(std::move(bridge));
        }

        auto enter_play = [&]() { editor_state.play(); };
        auto leave_play = [&]() { editor_state.stop(); };

        // Cross-panel asset drop on Viewport — same payload contract as the
        // hierarchy drop, but with no target entity.  Scene files load;
        // textures spawn a sprite at the editor camera target so the user
        // sees an immediate result.
        for (ViewportPanel* vp : { scene_vp, game_vp }) {
            if (!vp) continue;
            vp->set_on_asset_drop(
                [&](const std::string& path) {
                    namespace fs = std::filesystem;
                    const std::string ext = fs::path(path).extension().string();
                    if (ext == ".nxs" || ext == ".json") {
                        open_scene(path);
                    } else if (ext == ".png" || ext == ".jpg" ||
                               ext == ".jpeg" || ext == ".tga" ||
                               ext == ".bmp" || ext == ".hdr") {
                        const auto h = drop_importer.import_texture(path);
                        if (h == nexus::rhi::INVALID_HANDLE) {
                            editor_state.set_status("Texture decode failed: " +
                                                    fs::path(path).filename().string());
                            NX_ERROR("Viewport texture drop failed: {}", path);
                        } else {
                            Entity e = create_object("Sprite");
                            if (e != INVALID_ENTITY) {
                                auto& reg = scene.registry();
                                if (!reg.has_component<SpriteRendererComponent>(e)) {
                                    reg.add_component<SpriteRendererComponent>(
                                        e, SpriteRendererComponent{});
                                }
                                reg.get_component<SpriteRendererComponent>(e).texture_id = h;
                            }
                            editor_state.set_status("Texture: " +
                                                    fs::path(path).filename().string());
                        }
                    } else if (ext == ".obj" || ext == ".gltf" ||
                               ext == ".glb") {
                        const u32 mesh_id = drop_importer.import_mesh(path);
                        if (mesh_id == 0) {
                            editor_state.set_status("Mesh decode failed: " +
                                                    fs::path(path).filename().string());
                            NX_ERROR("Viewport mesh drop failed: {}", path);
                        } else {
                            Entity e = create_object("Imported Mesh");
                            if (e != INVALID_ENTITY) {
                                auto& reg = scene.registry();
                                if (!reg.has_component<MeshRendererComponent>(e)) {
                                    reg.add_component<MeshRendererComponent>(
                                        e, MeshRendererComponent{});
                                }
                                reg.get_component<MeshRendererComponent>(e).mesh_id = mesh_id;
                            }
                            editor_state.set_status("Mesh: " +
                                                    fs::path(path).filename().string());
                        }
                    } else if (ext == ".wav" || ext == ".ogg") {
                        // Audio drop loads the clip into AudioEngine and
                        // binds to the selection's AudioSourceComponent
                        // (auto-adds when missing).  No selection just
                        // leaves the clip cached for later reuse.
                        const auto clip_id = audio_cache.import(path);
                        if (clip_id == nexus::editor::INVALID_CLIP_ID) {
                            editor_state.set_status("Audio decode failed: " +
                                fs::path(path).filename().string());
                        } else {
                            const auto& sel = editor_state.selection();
                            if (sel.has_selection()) {
                                Entity e = static_cast<Entity>(sel.primary());
                                auto& r = scene.registry();
                                if (!r.has_component<AudioSourceComponent>(e)) {
                                    r.add_component<AudioSourceComponent>(
                                        e, AudioSourceComponent{});
                                }
                                r.get_component<AudioSourceComponent>(e).clip_id = clip_id;
                                editor_state.set_status("Bound audio: " +
                                    fs::path(path).filename().string());
                            } else {
                                editor_state.set_status("Audio loaded: " +
                                    fs::path(path).filename().string());
                            }
                        }
                    } else if (ext == ".anim") {
                        // Animation drop loads the clip and binds it to
                        // the currently-selected entity's AnimatorComponent
                        // (auto-adds if missing).  No selection just leaves
                        // it cached for later reuse.
                        if (!animation_cache.load(path)) {
                            editor_state.set_status("Animation parse failed: " +
                                fs::path(path).filename().string());
                        } else {
                            const u32 anim_id = animation_cache.id_for(path);
                            const auto& sel = editor_state.selection();
                            if (sel.has_selection()) {
                                Entity e = static_cast<Entity>(sel.primary());
                                auto& r = scene.registry();
                                if (!r.has_component<AnimatorComponent>(e)) {
                                    r.add_component<AnimatorComponent>(
                                        e, AnimatorComponent{});
                                }
                                r.get_component<AnimatorComponent>(e).clip_id = anim_id;
                                editor_state.set_status("Bound animation: " +
                                    fs::path(path).filename().string());
                            } else {
                                editor_state.set_status("Animation loaded: " +
                                    fs::path(path).filename().string());
                            }
                        }
                    } else if (ext == ".prefab" || ext == ".nexusprefab") {
                        // Prefab drop on viewport spawns an instance into
                        // the live scene at world origin (the editor camera
                        // target would require gizmo math; spawning at the
                        // prefab's authored transform is closer to Unity's
                        // behaviour).  Status reflects success / failure
                        // without crashing on parse errors.
                        const Entity root = prefab_cache.instantiate(path, scene);
                        if (root == INVALID_ENTITY) {
                            editor_state.set_status("Prefab failed: " +
                                fs::path(path).filename().string());
                        } else {
                            editor_state.selection().select(static_cast<u32>(root));
                            editor_state.set_status("Instantiated: " +
                                fs::path(path).filename().string());
                        }
                    } else if (ext == ".mat" || ext == ".material") {
                        // Materials don't have a position in the scene —
                        // drop binds to the currently-selected entity's
                        // MeshRendererComponent (if any).  A drop with no
                        // selection just loads the material into the cache
                        // for later reuse.
                        if (!material_cache.load(path)) {
                            editor_state.set_status("Material parse failed: " +
                                                    fs::path(path).filename().string());
                        } else {
                            const u32 mat_id = material_cache.id_for(path);
                            const auto& sel = editor_state.selection();
                            if (sel.has_selection()) {
                                Entity e = static_cast<Entity>(sel.primary());
                                auto& r = scene.registry();
                                if (r.has_component<MeshRendererComponent>(e)) {
                                    r.get_component<MeshRendererComponent>(e).material_id = mat_id;
                                    editor_state.set_status("Bound material: " +
                                        fs::path(path).filename().string());
                                } else {
                                    editor_state.set_status("Material loaded (no MeshRenderer on selection): " +
                                        fs::path(path).filename().string());
                                }
                            } else {
                                editor_state.set_status("Material loaded: " +
                                    fs::path(path).filename().string());
                            }
                        }
                    } else {
                        editor_state.set_status("Asset drop: " +
                                                fs::path(path).filename().string());
                        NX_INFO("Viewport asset drop: {}", path);
                    }
                });
        }

        // Cross-panel asset drop on Hierarchy:
        //   .nxs/.json → open the scene (replaces current);
        //   .png/.jpg/.tga/.bmp dropped on empty area → create Sprite entity;
        //   anything else → log + status.
        if (hierarchy) {
            hierarchy->set_on_asset_drop(
                [&](const std::string& path, u32 target_entity) {
                    namespace fs = std::filesystem;
                    const std::string ext = fs::path(path).extension().string();
                    auto& reg = scene.registry();
                    auto bind_texture = [&](Entity e, nexus::rhi::TextureHandle h) {
                        if (e == INVALID_ENTITY ||
                            h == nexus::rhi::INVALID_HANDLE) return;
                        if (!reg.has_component<SpriteRendererComponent>(e)) {
                            reg.add_component<SpriteRendererComponent>(
                                e, SpriteRendererComponent{});
                        }
                        reg.get_component<SpriteRendererComponent>(e).texture_id = h;
                    };
                    auto bind_mesh = [&](Entity e, u32 id) {
                        if (e == INVALID_ENTITY || id == 0u) return;
                        if (!reg.has_component<MeshRendererComponent>(e)) {
                            reg.add_component<MeshRendererComponent>(
                                e, MeshRendererComponent{});
                        }
                        reg.get_component<MeshRendererComponent>(e).mesh_id = id;
                    };

                    if (ext == ".nxs" || ext == ".json") {
                        open_scene(path);
                    } else if (ext == ".png" || ext == ".jpg" ||
                               ext == ".jpeg" || ext == ".tga" ||
                               ext == ".bmp" || ext == ".hdr") {
                        const auto h = drop_importer.import_texture(path);
                        if (h == nexus::rhi::INVALID_HANDLE) {
                            editor_state.set_status("Texture decode failed: " +
                                                    fs::path(path).filename().string());
                            NX_ERROR("Hierarchy texture drop failed: {}", path);
                        } else {
                            Entity e = (target_entity == 0)
                                ? create_object("Sprite")
                                : static_cast<Entity>(target_entity);
                            bind_texture(e, h);
                            editor_state.set_status("Texture: " +
                                                    fs::path(path).filename().string());
                        }
                    } else if (ext == ".obj" || ext == ".gltf" ||
                               ext == ".glb") {
                        const u32 mesh_id = drop_importer.import_mesh(path);
                        if (mesh_id == 0u) {
                            editor_state.set_status("Mesh decode failed: " +
                                                    fs::path(path).filename().string());
                            NX_ERROR("Hierarchy mesh drop failed: {}", path);
                        } else {
                            Entity e = (target_entity == 0)
                                ? create_object("Imported Mesh")
                                : static_cast<Entity>(target_entity);
                            bind_mesh(e, mesh_id);
                            editor_state.set_status("Mesh: " +
                                                    fs::path(path).filename().string());
                        }
                    } else if (ext == ".mat" || ext == ".material") {
                        if (!material_cache.load(path)) {
                            editor_state.set_status("Material parse failed: " +
                                                    fs::path(path).filename().string());
                        } else {
                            const u32 mat_id = material_cache.id_for(path);
                            Entity e = static_cast<Entity>(target_entity);
                            if (e != INVALID_ENTITY &&
                                reg.has_component<MeshRendererComponent>(e)) {
                                reg.get_component<MeshRendererComponent>(e).material_id = mat_id;
                                editor_state.set_status("Bound material: " +
                                    fs::path(path).filename().string());
                            } else {
                                editor_state.set_status("Material loaded: " +
                                    fs::path(path).filename().string());
                            }
                        }
                    } else if (ext == ".wav" || ext == ".ogg") {
                        const auto clip_id = audio_cache.import(path);
                        if (clip_id == nexus::editor::INVALID_CLIP_ID) {
                            editor_state.set_status("Audio decode failed: " +
                                fs::path(path).filename().string());
                        } else {
                            Entity e = static_cast<Entity>(target_entity);
                            if (e != INVALID_ENTITY && reg.alive(e)) {
                                if (!reg.has_component<AudioSourceComponent>(e)) {
                                    reg.add_component<AudioSourceComponent>(
                                        e, AudioSourceComponent{});
                                }
                                reg.get_component<AudioSourceComponent>(e).clip_id = clip_id;
                                editor_state.set_status("Bound audio: " +
                                    fs::path(path).filename().string());
                            } else {
                                editor_state.set_status("Audio loaded: " +
                                    fs::path(path).filename().string());
                            }
                        }
                    } else if (ext == ".anim") {
                        if (!animation_cache.load(path)) {
                            editor_state.set_status("Animation parse failed: " +
                                fs::path(path).filename().string());
                        } else {
                            const u32 anim_id = animation_cache.id_for(path);
                            Entity e = static_cast<Entity>(target_entity);
                            if (e != INVALID_ENTITY && reg.alive(e)) {
                                if (!reg.has_component<AnimatorComponent>(e)) {
                                    reg.add_component<AnimatorComponent>(
                                        e, AnimatorComponent{});
                                }
                                reg.get_component<AnimatorComponent>(e).clip_id = anim_id;
                                editor_state.set_status("Bound animation: " +
                                    fs::path(path).filename().string());
                            } else {
                                editor_state.set_status("Animation loaded: " +
                                    fs::path(path).filename().string());
                            }
                        }
                    } else if (ext == ".prefab" || ext == ".nexusprefab") {
                        // Hierarchy drop instantiates the prefab.  Target
                        // entity (when present) is reparented under via
                        // Hierarchy::set_parent so the user can drop a
                        // prefab onto a folder/parent entity to nest it.
                        const Entity root = prefab_cache.instantiate(path, scene);
                        if (root == INVALID_ENTITY) {
                            editor_state.set_status("Prefab failed: " +
                                fs::path(path).filename().string());
                        } else {
                            if (target_entity != 0 &&
                                reg.alive(static_cast<Entity>(target_entity))) {
                                Hierarchy::set_parent(reg, root,
                                    static_cast<Entity>(target_entity));
                            }
                            editor_state.selection().select(static_cast<u32>(root));
                            editor_state.set_status("Instantiated: " +
                                fs::path(path).filename().string());
                        }
                    } else {
                        editor_state.set_status("Asset drop: " +
                                                fs::path(path).filename().string());
                        NX_INFO("Asset drop on entity {}: {}",
                                target_entity, path);
                    }
                });
        }

        // Asset browser host callbacks — declared here because `open_scene`
        // and the asset-browser populate routine are both visible.
        if (assets_panel) {
            assets_panel->set_on_open([&](const AssetBrowserEntry& e) {
                if (e.is_directory) return;
                if (e.extension == ".nxs" || e.extension == ".json") {
                    open_scene(e.path);
                } else {
                    NX_INFO("Asset opened: {}", e.path);
                    editor_state.set_status("Opened " + e.name);
                }
            });
            assets_panel->set_on_reveal([&](const AssetBrowserEntry& e) {
#if defined(__APPLE__)
                std::string cmd = "open -R \"" + e.path + "\"";
#elif defined(_WIN32)
                std::string cmd = "explorer /select,\"" + e.path + "\"";
#else
                std::string cmd = "xdg-open \"" +
                    std::filesystem::path(e.path).parent_path().string() + "\"";
#endif
                if (std::system(cmd.c_str()) != 0) {
                    NX_WARN("Reveal failed for: {}", e.path);
                }
            });
            assets_panel->set_on_delete([&](const AssetBrowserEntry& e) {
                std::error_code rec;
                std::filesystem::remove_all(e.path, rec);
                if (rec) {
                    NX_ERROR("Delete failed: {}", rec.message());
                    editor_state.set_status("Delete failed");
                } else {
                    NX_INFO("Deleted: {}", e.path);
                    editor_state.set_status("Deleted " + e.name);
                    populate_asset_browser(*assets_panel,
                                           assets_panel->current_path());
                }
            });
            assets_panel->set_on_refresh([&]() {
                populate_asset_browser(*assets_panel,
                                       assets_panel->current_path());
            });
        }

        NX_INFO("Editor initialized with {} panels", editor_state.panels().count());

        // ── Main loop ───────────────────────────────────────────────────
        Timer timer;

        while (!window.should_close()) {
            editor_profiler.begin_frame();
            {
                ScopedProfile _p(editor_profiler, "poll_events");
                window.poll_events();
                Input::update();
            }
            timer.tick();

            float dt = timer.delta_time();
            editor_state.tick(dt);

            // ── Global shortcuts ────────────────────────────────────────
            // Stop play mode on Escape — universal escape hatch.
            if (Input::key_pressed(Key::Escape) && !editor_state.is_editing()) {
                leave_play();
            }
            const bool ctrl = Input::key_down(Key::LeftControl) ||
                              Input::key_down(Key::RightControl) ||
                              Input::key_down(Key::LeftSuper) ||
                              Input::key_down(Key::RightSuper);
            if (ctrl && Input::key_pressed(Key::Z)) {
                editor_state.undo_redo().undo();
            }
            if (ctrl && Input::key_pressed(Key::Y)) {
                editor_state.undo_redo().redo();
            }
            if (ctrl && Input::key_pressed(Key::S)) save_scene();
            if (ctrl && Input::key_pressed(Key::N)) new_scene();
            if (ctrl && Input::key_pressed(Key::D)) duplicate_selected();
            if (ctrl && Input::key_pressed(Key::P)) {
                editor_state.is_editing() ? enter_play() : leave_play();
            }

            // ── Console-driven Error Pause ──────────────────────────────
            // Honor the Console panel's "Error Pause" toggle: if a new error
            // arrived since last frame and we're playing, pause immediately.
            if (console && console->consume_error_pause_request() &&
                editor_state.is_playing()) {
                editor_state.pause();
                editor_state.set_status("Paused on error");
            }

            // ── Per-frame scene update ──────────────────────────────────
            // In Editing mode: only propagate transforms so Inspector /
            // ImGuizmo edits reach the renderer.  In Playing / Paused-step
            // mode: EditorState::advance_simulation runs scene.update with
            // a canonical fixed dt (no wall-clock jitter).
            bool ran_full_update = false;
            {
                ScopedProfile _p(editor_profiler, "scene_update");
                ran_full_update = editor_state.advance_simulation(dt);
                if (!ran_full_update) {
                    Hierarchy::propagate_transforms_3d(scene.registry());
                    Hierarchy::propagate_transforms_2d(scene.registry());
                }
            }

            // Propagate Hierarchy selection → Inspector target (unless the
            // inspector is locked to a specific entity).  Reads from the
            // unified EditorSelection so multi-select primary stays in sync.
            if (inspector && !inspector->is_locked()) {
                const auto& sel = editor_state.selection();
                if (sel.has_selection()) {
                    inspector->set_target_entity(sel.primary());
                } else {
                    inspector->clear_target();
                }
            }

            // Rescan when nav changes OR when the user toggled the Recursive
            // flag / changed the search box.  Recursive + non-empty search
            // walks the whole project tree (capped); otherwise we list only
            // the current folder.
            if (assets_panel) {
                static bool s_last_recursive = false;
                static std::string s_last_search;
                static AssetBrowserPanel::SidebarFilter s_last_filter =
                    AssetBrowserPanel::SidebarFilter::None;
                const bool recursive = assets_panel->recursive_search();
                const auto filter    = assets_panel->current_filter();
                const bool search_changed = assets_panel->search() != s_last_search;
                const bool flag_changed   = recursive != s_last_recursive;
                const bool filter_changed = filter != s_last_filter;
                if (assets_panel->consume_navigation_dirty() ||
                    search_changed || flag_changed || filter_changed) {
                    s_last_recursive = recursive;
                    s_last_search    = assets_panel->search();
                    s_last_filter    = filter;

                    using SF = AssetBrowserPanel::SidebarFilter;
                    auto ext_eq = [](const std::string& ext,
                                     std::initializer_list<const char*> list) {
                        for (const char* c : list) {
                            if (ext == c) return true;
                        }
                        return false;
                    };
                    auto path_ext = [](const std::filesystem::path& p) {
                        std::string e = p.extension().string();
                        for (auto& c : e) c = static_cast<char>(std::tolower(c));
                        return e;
                    };
                    switch (filter) {
                    case SF::Favorites:
                        populate_asset_browser_favorites(*assets_panel);
                        break;
                    case SF::AllMaterials:
                        populate_asset_browser_filtered(
                            *assets_panel, assets_panel->root_path(),
                            [&](const std::filesystem::path& p) {
                                return ext_eq(path_ext(p),
                                              {".mat", ".material"});
                            });
                        break;
                    case SF::AllModels:
                        populate_asset_browser_filtered(
                            *assets_panel, assets_panel->root_path(),
                            [&](const std::filesystem::path& p) {
                                return ext_eq(path_ext(p),
                                              {".obj", ".gltf", ".glb",
                                               ".fbx", ".dae"});
                            });
                        break;
                    case SF::AllPrefabs:
                        populate_asset_browser_filtered(
                            *assets_panel, assets_panel->root_path(),
                            [&](const std::filesystem::path& p) {
                                return ext_eq(path_ext(p),
                                              {".prefab", ".nexusprefab"});
                            });
                        break;
                    case SF::None:
                    default:
                        if (recursive && !s_last_search.empty()) {
                            populate_asset_browser_recursive(
                                *assets_panel, assets_panel->root_path());
                        } else {
                            populate_asset_browser(*assets_panel,
                                                   assets_panel->current_path());
                        }
                        break;
                    }
                }

                // ── Thumbnail bridge ────────────────────────────────────
                // Pump completed decode jobs onto the GPU and request
                // thumbnails for any new image entries.  drain() is bounded
                // O(in_flight); request() returns immediately if already
                // pending, so the per-frame cost is one small map walk.
                thumbnail_cache.drain();
                for (const auto& entry : assets_panel->entries()) {
                    if (entry.is_directory) continue;
                    std::string ext = entry.extension;
                    for (auto& c : ext) c = static_cast<char>(std::tolower(c));
                    const bool is_image =
                        ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
                        ext == ".bmp" || ext == ".tga" || ext == ".hdr" ||
                        ext == ".gif" || ext == ".psd";
                    if (!is_image) continue;
                    const auto h = thumbnail_cache.request(entry.path);
                    if (h != nexus::rhi::INVALID_HANDLE) {
                        // GL backend: TextureHandle == GL texture name; ImGui
                        // accepts that directly via static_cast<ImTextureID>.
                        assets_panel->set_thumbnail(
                            entry.path, static_cast<std::uintptr_t>(h));
                    }
                }
            }

            // ── Render ──────────────────────────────────────────────────
            rhi->begin_frame();
            rhi->set_viewport(0, 0, window.width(), window.height());
            rhi->clear(Vec4{0.12f, 0.12f, 0.14f, 1.0f});

            // ── ImGui frame begin ──────────────────────────────────────
            rhi->imgui_new_frame();
            ImGui::NewFrame();
            ImGuizmo::BeginFrame();

            // ── Host DockSpace + main menu bar ─────────────────────────
            // A full-window invisible host window owns the dock node and the
            // main menu bar.  Panels then dock into the central node.
            {
                const ImGuiViewport* vp = ImGui::GetMainViewport();
                ImGui::SetNextWindowPos(vp->WorkPos);
                ImGui::SetNextWindowSize(vp->WorkSize);
                ImGui::SetNextWindowViewport(vp->ID);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                ImGuiWindowFlags host_flags =
                    ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                    ImGuiWindowFlags_NoBackground;
                ImGui::Begin("##NexusEditorHost", nullptr, host_flags);
                ImGui::PopStyleVar(3);

                ImGui::DockSpace(ImGui::GetID("NexusDockSpace"),
                                 ImVec2(0.0f, 0.0f),
                                 ImGuiDockNodeFlags_PassthruCentralNode);

                // ── Main menu bar ───────────────────────────────────────
                if (ImGui::BeginMenuBar()) {
                    // ── File ────────────────────────────────────────────
                    if (ImGui::BeginMenu("File")) {
                        if (ImGui::MenuItem("New Scene", "Ctrl+N")) new_scene();
                        if (ImGui::MenuItem("Open Scene…", "Ctrl+O")) {
                            // Without a native file dialog we fall back to the
                            // canonical default path — the AssetBrowser panel
                            // handles richer browsing.
                            open_scene(default_scene_save_path());
                        }
                        if (ImGui::MenuItem("Save Scene", "Ctrl+S")) save_scene();
                        if (ImGui::MenuItem("Save Scene As…", "Ctrl+Shift+S")) {
                            save_scene_as(default_scene_save_path());
                        }
                        ImGui::Separator();
                        if (ImGui::MenuItem("Exit", "Cmd+Q")) {
                            glfwSetWindowShouldClose(
                                static_cast<GLFWwindow*>(window.native_handle()), 1);
                        }
                        ImGui::EndMenu();
                    }
                    // ── Edit ────────────────────────────────────────────
                    if (ImGui::BeginMenu("Edit")) {
                        if (ImGui::MenuItem("Undo", "Ctrl+Z",
                                            false, editor_state.undo_redo().can_undo())) {
                            editor_state.undo_redo().undo();
                        }
                        if (ImGui::MenuItem("Redo", "Ctrl+Y",
                                            false, editor_state.undo_redo().can_redo())) {
                            editor_state.undo_redo().redo();
                        }
                        ImGui::Separator();
                        if (ImGui::MenuItem("Duplicate", "Ctrl+D",
                                            false, hierarchy && hierarchy->has_selection())) {
                            duplicate_selected();
                        }
                        if (ImGui::MenuItem("Delete", "Del",
                                            false, hierarchy && hierarchy->has_selection())) {
                            if (hierarchy && hierarchy->has_selection()) {
                                Entity sel = hierarchy->selected_entity();
                                scene.registry().destroy(sel);
                                hierarchy->clear_selection();
                            }
                        }
                        ImGui::EndMenu();
                    }
                    // ── GameObject ──────────────────────────────────────
                    // Top-level Unity-style menu mirroring the Hierarchy
                    // create popup so users discover the same actions from
                    // the menu bar.
                    if (ImGui::BeginMenu("GameObject")) {
                        if (ImGui::MenuItem("Create Empty")) create_object("Empty");
                        ImGui::Separator();
                        if (ImGui::BeginMenu("3D Object")) {
                            if (ImGui::MenuItem("Cube"))   create_object("Cube");
                            if (ImGui::MenuItem("Sphere")) create_object("Sphere");
                            if (ImGui::MenuItem("Plane"))  create_object("Plane");
                            ImGui::EndMenu();
                        }
                        if (ImGui::BeginMenu("2D Object")) {
                            if (ImGui::MenuItem("Sprite")) create_object("Sprite");
                            ImGui::EndMenu();
                        }
                        if (ImGui::BeginMenu("Light")) {
                            if (ImGui::MenuItem("Directional Light"))
                                create_object("Directional Light");
                            if (ImGui::MenuItem("Point Light"))
                                create_object("Point Light");
                            if (ImGui::MenuItem("Spot Light"))
                                create_object("Spot Light");
                            ImGui::EndMenu();
                        }
                        ImGui::Separator();
                        if (ImGui::MenuItem("Camera")) create_object("Camera");
                        ImGui::EndMenu();
                    }
                    // ── View ────────────────────────────────────────────
                    if (ImGui::BeginMenu("View")) {
                        // Toggle each registered panel's visibility.
                        for (auto& p : editor_state.panels().panels()) {
                            bool visible = p->is_visible();
                            if (ImGui::MenuItem(p->title().c_str(), nullptr, &visible)) {
                                p->set_visible(visible);
                            }
                        }
                        ImGui::EndMenu();
                    }
                    // ── Window ──────────────────────────────────────────
                    if (ImGui::BeginMenu("Window")) {
                        if (ImGui::MenuItem("Reset Layout")) {
                            // Clearing the .ini wipes user dock state on next
                            // frame; combined with the default dock seeding in
                            // register_default_panels() this restores defaults.
                            ImGui::LoadIniSettingsFromMemory("");
                        }
                        ImGui::EndMenu();
                    }
                    // ── Help ────────────────────────────────────────────
                    if (ImGui::BeginMenu("Help")) {
                        ImGui::MenuItem("NexusEngine Editor", nullptr, false, false);
                        ImGui::MenuItem("Build " __DATE__, nullptr, false, false);
                        ImGui::EndMenu();
                    }

                    // ── Layers dropdown ─────────────────────────────────
                    // Per-layer visibility checklist + Show All / Hide All.
                    // Each row is "Index Name" with the user-defined label
                    // (defaults match Unity's reserved layers).  The mask
                    // updates immediately so subsequent renderer-side
                    // filtering picks it up next frame.
                    if (ImGui::BeginMenu("Layers")) {
                        if (ImGui::MenuItem("Show All")) layer_registry.show_all();
                        if (ImGui::MenuItem("Hide All")) layer_registry.hide_all();
                        ImGui::Separator();
                        for (u32 i = 0; i < nexus::editor::LayerRegistry::kLayerCount; ++i) {
                            const std::string& name = layer_registry.name(i);
                            char label[64];
                            if (!name.empty()) {
                                std::snprintf(label, sizeof(label), "%2u  %s",
                                              i, name.c_str());
                            } else {
                                std::snprintf(label, sizeof(label), "%2u  Layer %u", i, i);
                            }
                            bool vis = layer_registry.is_visible(i);
                            if (ImGui::MenuItem(label, nullptr, &vis)) {
                                layer_registry.set_visible(i, vis);
                            }
                        }
                        ImGui::EndMenu();
                    }

                    // ── Layout dropdown ─────────────────────────────────
                    // Per-preset ImGui ini files keep dock state independent.
                    // Switching presets calls ImGui::LoadIniSettingsFromDisk
                    // — ImGui applies the dock node + window positions on
                    // the next frame, no manual re-docking required.
                    if (ImGui::BeginMenu("Layout")) {
                        auto preset = [&](nexus::editor::LayoutPreset p) {
                            const bool active = (layout_preset == p);
                            if (ImGui::MenuItem(
                                    nexus::editor::layout_preset_name(p),
                                    nullptr, active)) {
                                if (layout_preset != p) {
                                    // Save current layout into its preset file
                                    // first, then load the target preset's
                                    // file.  This preserves user tweaks
                                    // within each preset.
                                    ImGui::SaveIniSettingsToDisk(
                                        nexus::editor::layout_preset_ini_filename(layout_preset));
                                    layout_preset = p;
                                    ImGui::LoadIniSettingsFromDisk(
                                        nexus::editor::layout_preset_ini_filename(p));
                                }
                            }
                        };
                        preset(nexus::editor::LayoutPreset::Default);
                        preset(nexus::editor::LayoutPreset::TwoByThree);
                        preset(nexus::editor::LayoutPreset::FourSplit);
                        preset(nexus::editor::LayoutPreset::Tall);
                        preset(nexus::editor::LayoutPreset::Wide);
                        ImGui::Separator();
                        if (ImGui::MenuItem("Save Layout")) {
                            ImGui::SaveIniSettingsToDisk(
                                nexus::editor::layout_preset_ini_filename(layout_preset));
                            editor_state.set_status("Layout saved");
                        }
                        ImGui::EndMenu();
                    }

                    // ── Centered Play/Pause/Step group + right T/R/S gizmo
                    {
                        const float region_w =
                            ImGui::GetContentRegionAvail().x;
                        const float play_w  = 240.0f;
                        const float gizmo_w = 240.0f;
                        const float lead_pad =
                            (region_w - play_w - gizmo_w) * 0.5f;
                        if (lead_pad > 0.0f)
                            ImGui::Dummy(ImVec2(lead_pad, 0));
                        ImGui::SameLine();

                        const bool playing = editor_state.is_playing();
                        const bool paused  = editor_state.is_paused();
                        if (playing || paused) {
                            ImGui::PushStyleColor(ImGuiCol_Button,
                                ImVec4(0.30f, 0.55f, 0.85f, 1.0f));
                            if (ImGui::Button("Play")) leave_play();
                            ImGui::PopStyleColor();
                        } else {
                            if (ImGui::Button("Play")) enter_play();
                        }
                        ImGui::SameLine();
                        ImGui::BeginDisabled(!(playing || paused));
                        if (ImGui::Button(paused ? "Resume" : "Pause")) {
                            if (paused) editor_state.resume();
                            else        editor_state.pause();
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Step")) editor_state.step();
                        ImGui::EndDisabled();

                        ImGui::SameLine();
                        const float right_pad =
                            ImGui::GetContentRegionAvail().x - gizmo_w;
                        if (right_pad > 0.0f)
                            ImGui::Dummy(ImVec2(right_pad, 0));
                        ImGui::SameLine();

                        ViewportPanel* active_vp =
                            scene_vp ? scene_vp : game_vp;
                        if (active_vp) {
                            const GizmoMode mode = active_vp->gizmo_mode();
                            auto gizmo_btn = [&](const char* label,
                                                 GizmoMode m) {
                                const bool active = (mode == m);
                                if (active) {
                                    ImGui::PushStyleColor(ImGuiCol_Button,
                                        ImVec4(0.36f, 0.50f, 0.70f, 1.0f));
                                }
                                if (ImGui::Button(label))
                                    active_vp->set_gizmo_mode(m);
                                if (active) ImGui::PopStyleColor();
                                ImGui::SameLine();
                            };
                            gizmo_btn("Translate", GizmoMode::Translate);
                            gizmo_btn("Rotate",    GizmoMode::Rotate);
                            gizmo_btn("Scale",     GizmoMode::Scale);
                        }
                    }


                    ImGui::EndMenuBar();
                }

                ImGui::End(); // host
            }

            // Update and render editor panels (these call ImGui widgets)
            editor_state.panels().update(dt);
            editor_state.panels().render();

            // ── Status bar (pinned to bottom of main viewport) ─────────
            // A thin, non-dockable window rendered last so it overlays the
            // dockspace footer.  Shows FPS, entity count, Play state, and
            // the current status string from EditorState.
            {
                const float status_h = ImGui::GetFrameHeight();
                const ImGuiViewport* vp = ImGui::GetMainViewport();
                ImGui::SetNextWindowPos(
                    ImVec2(vp->WorkPos.x,
                           vp->WorkPos.y + vp->WorkSize.y - status_h));
                ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, status_h));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
                ImGuiWindowFlags sb_flags =
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                    ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;
                if (ImGui::Begin("##NexusStatusBar", nullptr, sb_flags)) {
                    const char* play_state =
                        editor_state.is_playing()
                            ? (editor_state.is_paused() ? "PAUSED" : "PLAYING")
                            : "STOPPED";
                    ImVec4 play_col =
                        editor_state.is_playing()
                            ? (editor_state.is_paused()
                                   ? ImVec4(1.0f, 0.8f, 0.2f, 1.0f)
                                   : ImVec4(0.3f, 1.0f, 0.3f, 1.0f))
                            : ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
                    ImGui::TextColored(play_col, "* %s", play_state);
                    ImGui::SameLine();
                    ImGui::TextDisabled(" | ");
                    ImGui::SameLine();
                    ImGui::Text("FPS: %.0f (%.2f ms)",
                                timer.fps(), dt * 1000.0f);
                    ImGui::SameLine();
                    ImGui::TextDisabled(" | ");
                    ImGui::SameLine();
                    ImGui::Text("Entities: %zu", scene.registry().size());
                    ImGui::SameLine();
                    ImGui::TextDisabled(" | ");
                    ImGui::SameLine();
                    ImGui::Text("Panels: %u", editor_state.panels().count());
                    ImGui::SameLine();
                    ImGui::TextDisabled(" | ");
                    ImGui::SameLine();
                    if (ConsolePanel* console_badge = editor_state.panels()
                            .find_typed<ConsolePanel>("Console")) {
                        const u32 warn = console_badge->warning_count();
                        const u32 err  = console_badge->error_count();
                        ImGui::TextColored(
                            err > 0 ? ImVec4(1.0f, 0.35f, 0.35f, 1.0f)
                                    : ImVec4(0.55f, 0.55f, 0.55f, 1.0f),
                            "%c %u", 'E', err);
                        ImGui::SameLine();
                        ImGui::TextColored(
                            warn > 0 ? ImVec4(1.0f, 0.85f, 0.25f, 1.0f)
                                     : ImVec4(0.55f, 0.55f, 0.55f, 1.0f),
                            "%c %u", 'W', warn);
                        ImGui::SameLine();
                        ImGui::TextDisabled(" | ");
                        ImGui::SameLine();
                    }
                    if (!editor_state.scene_path().empty()) {
                        ImGui::Text("Scene: %s",
                                    std::filesystem::path(editor_state.scene_path())
                                        .filename().string().c_str());
                        ImGui::SameLine();
                        ImGui::TextDisabled(" | ");
                        ImGui::SameLine();
                    }
                    // Layout + Layers compact summary — gives the user a
                    // quick read on the editor configuration without
                    // opening the dropdowns.
                    {
                        const u32 vmask = layer_registry.visible_mask();
                        u32 visible_count = 0;
                        for (u32 b = 0; b < 32u; ++b) {
                            if ((vmask >> b) & 1u) ++visible_count;
                        }
                        ImGui::TextDisabled(
                            "[%s | Layers %u/32]",
                            nexus::editor::layout_preset_name(layout_preset),
                            visible_count);
                        ImGui::SameLine();
                        ImGui::TextDisabled(" | ");
                        ImGui::SameLine();
                    }
                    ImGui::TextColored(ImVec4(0.7f, 0.85f, 1.0f, 0.9f),
                                       "%s", editor_state.status().c_str());
                }
                ImGui::End();
                ImGui::PopStyleVar(2);
            }

            // ── ImGui frame end + render draw data ─────────────────────
            {
                ScopedProfile _p(editor_profiler, "imgui_render");
                ImGui::Render();
                rhi->imgui_render_draw_data();
            }

            {
                ScopedProfile _p(editor_profiler, "swap");
                rhi->end_frame();
                window.swap_buffers();
            }

            // Periodic status
            if (timer.frame_count() % 300 == 0 && timer.frame_count() > 0) {
                NX_TRACE("Editor FPS: {:.1f}", timer.fps());
            }
            editor_profiler.end_frame();
        }

        // ── Shutdown (reverse init order) ───────────────────────────────
        renderer_3d.destroy_mesh(primitive_sphere);
        renderer_3d.destroy_mesh(primitive_plane);
        renderer_3d.destroy_mesh(primitive_cube);
        debug_renderer.shutdown();
        // RHI tears down both ImGui halves (renderer first, platform second)
        // before its own GPU resources go away, mirroring init order.
        rhi->imgui_shutdown();
        ImGui::DestroyContext();
        audio_device.close();
        audio.stop_all();
        rhi->shutdown();
        NX_INFO("NexusEngine Editor shutting down...");

    } catch (const std::exception& e) {
        NX_ERROR("Fatal error: {}", e.what());
        Log::shutdown();
        return 1;
    }

    Log::shutdown();
    return 0;
}

} // namespace nexus::editor

int main(int argc, char* argv[]) {
    return nexus::editor::run(argc, argv);
}
