// ============================================================================
// runtime_main.cpp — NexusEngine standalone game runtime entry point
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
#include "nexus/physics/physics_system.h"
#include "nexus/audio/audio_engine.h"
#include "nexus/scripting/script_engine.h"
#include "nexus/scripting/engine_bindings.h"

#include <string>

// Forward declare GLFW proc address getter
struct GLFWwindow;
extern "C" {
    typedef void (*GLFWglproc)(void);
    GLFWglproc glfwGetProcAddress(const char* procname);
}

static void print_usage() {
    NX_APP_INFO("Usage: nexus-runtime [options]");
    NX_APP_INFO("  --scene <path>   Load scene file at startup");
    NX_APP_INFO("  --width <n>      Window width  (default: 1280)");
    NX_APP_INFO("  --height <n>     Window height (default: 720)");
    NX_APP_INFO("  --title <name>   Window title  (default: NexusEngine)");
    NX_APP_INFO("  --vsync          Enable vertical sync (default: on)");
    NX_APP_INFO("  --no-vsync       Disable vertical sync");
}

struct RuntimeConfig {
    std::string scene_path;
    nexus::WindowConfig window;
};

static RuntimeConfig parse_args(int argc, char* argv[]) {
    RuntimeConfig cfg;
    cfg.window.title = "NexusEngine";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--scene" && i + 1 < argc)       { cfg.scene_path = argv[++i]; }
        else if (arg == "--width" && i + 1 < argc)   { cfg.window.width = std::stoi(argv[++i]); }
        else if (arg == "--height" && i + 1 < argc)  { cfg.window.height = std::stoi(argv[++i]); }
        else if (arg == "--title" && i + 1 < argc)   { cfg.window.title = argv[++i]; }
        else if (arg == "--vsync")                    { cfg.window.vsync = true; }
        else if (arg == "--no-vsync")                 { cfg.window.vsync = false; }
        else if (arg == "--help" || arg == "-h") {
            print_usage();
        }
    }
    return cfg;
}

int main(int argc, char* argv[]) {
    // ── Initialize core systems ─────────────────────────────────────────
    nexus::Log::init();
    NX_APP_INFO("NexusEngine Runtime starting...");

    RuntimeConfig cfg = parse_args(argc, argv);

    nexus::Window window(cfg.window);
    NX_APP_INFO("Window created: {}x{}", cfg.window.width, cfg.window.height);

    nexus::Input::init(window.native_handle());

    // Load OpenGL functions
    if (!nexus::rhi::gl::load(
            reinterpret_cast<nexus::rhi::gl::GLLoadProc>(glfwGetProcAddress))) {
        NX_ERROR("Failed to load OpenGL functions");
        return 1;
    }

    // ── Create RHI ──────────────────────────────────────────────────────
    auto rhi = nexus::rhi::RHI::create();
    if (!rhi || !rhi->init()) {
        NX_ERROR("Failed to initialize RHI");
        return 1;
    }

    // ── Create scene & subsystems ───────────────────────────────────────
    nexus::Registry registry;
    nexus::physics::PhysicsSystem physics;
    nexus::audio::AudioEngine audio;

    // ── Create scripting engine with live bindings ───────────────────────
    nexus::scripting::ScriptEngine script_engine;
    // Input is a static-only class; the reference is required by bind_all's
    // signature for API consistency but only static methods are invoked.
    nexus::Input input_handle;
    nexus::scripting::bind_all(script_engine, registry, input_handle, audio, physics);
    NX_APP_INFO("Scripting engine initialized with live bindings");

    // ── Load scene if provided ──────────────────────────────────────────
    if (!cfg.scene_path.empty()) {
        NX_APP_INFO("Loading scene: {}", cfg.scene_path);
        // Scene loading would go here via SceneSerializer
    }

    // ── Fixed timestep for physics ──────────────────────────────────────
    nexus::Timer timer;
    nexus::FixedTimestep fixed_step(1.0f / 60.0f);

    NX_APP_INFO("Entering main loop...");

    while (!window.should_close()) {
        window.poll_events();
        nexus::Input::update();
        timer.tick();

        float dt = timer.delta_time();

        // ── Fixed-rate physics update ───────────────────────────────────
        fixed_step.accumulate(dt);
        while (fixed_step.should_step()) {
            physics.update(registry, fixed_step.step());
            fixed_step.consume();
        }

        // ── Audio update ────────────────────────────────────────────────
        audio.update();

        // ── Render ──────────────────────────────────────────────────────
        rhi->begin_frame();
        rhi->set_viewport(0, 0, window.width(), window.height());
        rhi->clear(nexus::Vec4{0.0f, 0.0f, 0.0f, 1.0f});

        // Rendering would go here (2D/3D renderers, scene traversal, etc.)

        rhi->end_frame();
        window.swap_buffers();

        if (nexus::Input::key_pressed(nexus::Key::Escape)) break;
    }

    // ── Shutdown ────────────────────────────────────────────────────────
    audio.stop_all();
    rhi->shutdown();
    NX_APP_INFO("NexusEngine Runtime shutting down...");
    nexus::Log::shutdown();
    return 0;
}
