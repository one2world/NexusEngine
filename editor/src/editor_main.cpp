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
#include "nexus/scene/registry.h"
#include "nexus/scene/scene.h"
#include "nexus/editor/editor_state.h"
#include "nexus/editor/editor_panels.h"

#include <string>
#include <vector>
#include <stdexcept>

// Forward declare GLFW proc address getter
struct GLFWwindow;
extern "C" {
    typedef void (*GLFWglproc)(void);
    GLFWglproc glfwGetProcAddress(const char* procname);
}

namespace nexus::editor {

static void register_default_panels(EditorState& state) {
    auto& panels = state.panels();
    panels.add_panel(std::make_unique<ViewportPanel>());
    panels.add_panel(std::make_unique<HierarchyPanel>());
    panels.add_panel(std::make_unique<InspectorPanel>());
    panels.add_panel(std::make_unique<ConsolePanel>());
    panels.add_panel(std::make_unique<AssetBrowserPanel>());

    // Set up default dock layout
    auto& dock = panels.dock_space();
    dock.dock("Viewport",      DockPosition::Center);
    dock.dock("Hierarchy",     DockPosition::Left,   0.20f);
    dock.dock("Inspector",     DockPosition::Right,  0.25f);
    dock.dock("Console",       DockPosition::Bottom, 0.25f);
    dock.dock("Asset Browser", DockPosition::Bottom, 0.25f);
}

static int run(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

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

        // ── Create scene ────────────────────────────────────────────────
        Registry registry;

        // ── Create editor state ─────────────────────────────────────────
        EditorState editor_state;
        register_default_panels(editor_state);
        editor_state.set_status("Ready");

        NX_INFO("Editor initialized with {} panels", editor_state.panels().count());

        // ── Main loop ───────────────────────────────────────────────────
        Timer timer;

        while (!window.should_close()) {
            window.poll_events();
            Input::update();
            timer.tick();

            float dt = timer.delta_time();
            editor_state.tick(dt);

            // Handle global shortcuts
            if (Input::key_pressed(Key::Escape)) {
                if (editor_state.is_playing()) {
                    editor_state.stop();
                    NX_INFO("Play mode stopped");
                }
            }

            if (Input::key_down(Key::LeftControl) && Input::key_pressed(Key::Z)) {
                editor_state.undo_redo().undo();
            }
            if (Input::key_down(Key::LeftControl) && Input::key_pressed(Key::Y)) {
                editor_state.undo_redo().redo();
            }

            // ── Update logic ────────────────────────────────────────────
            if (editor_state.is_playing() || editor_state.is_paused()) {
                u32 steps = editor_state.consume_step_requests();
                if (editor_state.is_playing() || steps > 0) {
                    // In a full implementation, step the game systems here
                }
            }

            // ── Render ──────────────────────────────────────────────────
            rhi->begin_frame();
            rhi->set_viewport(0, 0, window.width(), window.height());
            rhi->clear(Vec4{0.12f, 0.12f, 0.14f, 1.0f});

            // Update and render editor panels
            editor_state.panels().update(dt);
            editor_state.panels().render();

            rhi->end_frame();
            window.swap_buffers();

            // Periodic status
            if (timer.frame_count() % 300 == 0 && timer.frame_count() > 0) {
                NX_TRACE("Editor FPS: {:.1f}", timer.fps());
            }
        }

        // ── Shutdown (reverse init order) ───────────────────────────────
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
