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
#include "nexus/scene/scene_serializer.h"
#include "nexus/scene/hierarchy.h"
#include "nexus/renderer/batch_renderer_2d.h"
#include "nexus/renderer/forward_renderer_3d.h"
#include "nexus/renderer/camera.h"
#include "nexus/physics/physics_system.h"
#include "nexus/audio/audio_engine.h"
#include "nexus/scripting/script_engine.h"
#include "nexus/scripting/engine_bindings.h"

#include <cmath>
#include <string>
#include <stdexcept>
#include <unordered_map>

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
    bool show_help{false};
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
            cfg.show_help = true;
        }
    }
    return cfg;
}

int main(int argc, char* argv[]) {
    // ── Initialize core systems ─────────────────────────────────────────
    nexus::Log::init();
    NX_APP_INFO("NexusEngine Runtime starting...");

    RuntimeConfig cfg;
    try {
        cfg = parse_args(argc, argv);
    } catch (const std::exception& e) {
        NX_ERROR("Invalid command-line arguments: {}", e.what());
        print_usage();
        nexus::Log::shutdown();
        return 1;
    }

    if (cfg.show_help) {
        print_usage();
        nexus::Log::shutdown();
        return 0;
    }

    try {
        nexus::Window window(cfg.window);
        NX_APP_INFO("Window created: {}x{}", cfg.window.width, cfg.window.height);

        nexus::Input::init(window.native_handle());

        // Load OpenGL functions
        if (!nexus::rhi::gl::load(
                reinterpret_cast<nexus::rhi::gl::GLLoadProc>(glfwGetProcAddress))) {
            NX_ERROR("Failed to load OpenGL functions");
            nexus::Log::shutdown();
            return 1;
        }

        // ── Create RHI ──────────────────────────────────────────────────
        auto rhi = nexus::rhi::RHI::create();
        if (!rhi || !rhi->init()) {
            NX_ERROR("Failed to initialize RHI");
            nexus::Log::shutdown();
            return 1;
        }

        // ── Create scene & subsystems ───────────────────────────────────
        nexus::Scene scene;
        auto& registry = scene.registry();
        nexus::physics::PhysicsSystem physics;
        nexus::audio::AudioEngine audio;

        // ── Initialize renderers ────────────────────────────────────────
        nexus::BatchRenderer2D renderer_2d;
        renderer_2d.init(rhi.get());

        nexus::ForwardRenderer3D renderer_3d;
        renderer_3d.init(rhi.get());

        // Mesh cache: mesh_id -> uploaded Mesh for 3D rendering.
        // Default meshes are uploaded so entities with mesh_id 0 get a cube.
        std::unordered_map<nexus::u32, nexus::Mesh> mesh_cache;
        {
            auto cube = nexus::create_cube_mesh();
            renderer_3d.upload_mesh(cube);
            mesh_cache[0] = std::move(cube);
        }

        // ── Create scripting engine with live bindings ───────────────────
        nexus::scripting::ScriptEngine script_engine;
        nexus::Input input_handle;
        nexus::scripting::bind_all(script_engine, registry, input_handle, audio, physics);
        NX_APP_INFO("Scripting engine initialized with live bindings");

        // ── Load scene if provided ──────────────────────────────────────
        if (!cfg.scene_path.empty()) {
            NX_APP_INFO("Loading scene: {}", cfg.scene_path);
            nexus::SceneSerializer serializer(scene);
            if (!serializer.load(cfg.scene_path)) {
                NX_WARN("Failed to load scene: {}", cfg.scene_path);
            }
        }

        // ── Fixed timestep for physics ──────────────────────────────────
        nexus::Timer timer;
        nexus::FixedTimestep fixed_step(1.0f / 60.0f);

        NX_APP_INFO("Entering main loop...");

        while (!window.should_close()) {
            window.poll_events();
            nexus::Input::update();
            timer.tick();

            float dt = timer.delta_time();

            // ── Scene update (transform propagation + system scheduler) ──
            scene.update(dt);

            // ── Fixed-rate physics update ───────────────────────────────
            fixed_step.accumulate(dt);
            while (fixed_step.should_step()) {
                physics.update(registry, fixed_step.step());
                fixed_step.consume();
            }

            // ── Audio update ────────────────────────────────────────────
            audio.update();

            // ── Render ──────────────────────────────────────────────────
            rhi->begin_frame();
            rhi->set_viewport(0, 0, window.width(), window.height());
            rhi->clear(nexus::Vec4{0.1f, 0.1f, 0.12f, 1.0f});

            float aspect = static_cast<float>(window.width()) /
                           static_cast<float>(std::max(window.height(), 1));

            // ── Find the primary camera ─────────────────────────────────
            bool has_3d_camera = false;
            bool has_2d_camera = false;
            nexus::Camera3D cam_3d;
            nexus::Camera2D cam_2d;

            registry.each<nexus::CameraComponent>([&](nexus::Entity e,
                                                       nexus::CameraComponent& cc) {
                if (!cc.is_primary) return;
                if (cc.is_orthographic) {
                    float w = cc.ortho_size * aspect;
                    float h = cc.ortho_size;
                    cam_2d.set_projection(w, h);
                    if (registry.has_component<nexus::Transform2DComponent>(e)) {
                        auto& t = registry.get_component<nexus::Transform2DComponent>(e);
                        cam_2d.position = t.world_position;
                        cam_2d.rotation = t.world_rotation;
                    }
                    has_2d_camera = true;
                } else {
                    cam_3d.fov = cc.fov;
                    cam_3d.near_clip = cc.near_clip;
                    cam_3d.far_clip = cc.far_clip;
                    cam_3d.set_perspective(aspect);
                    if (registry.has_component<nexus::Transform3DComponent>(e)) {
                        auto& t = registry.get_component<nexus::Transform3DComponent>(e);
                        cam_3d.position = t.position;
                        // Convert quaternion to yaw/pitch for Camera3D
                        nexus::Vec3 fwd = glm::rotate(t.rotation,
                                                        nexus::Vec3{0.0f, 0.0f, -1.0f});
                        cam_3d.pitch = glm::degrees(std::asin(fwd.y));
                        cam_3d.yaw = glm::degrees(std::atan2(fwd.z, fwd.x));
                    }
                    has_3d_camera = true;
                }
            });

            // ── 3D rendering pass ───────────────────────────────────────
            if (has_3d_camera) {
                renderer_3d.begin(cam_3d);

                // Set directional lights
                registry.each<nexus::DirectionalLightComponent, nexus::Transform3DComponent>(
                    [&](nexus::Entity, nexus::DirectionalLightComponent& dl,
                        nexus::Transform3DComponent& t) {
                        nexus::DirectionalLight light;
                        light.direction = glm::rotate(t.rotation,
                                                       nexus::Vec3{0.0f, -1.0f, 0.0f});
                        light.color = dl.color;
                        light.intensity = dl.intensity;
                        renderer_3d.set_directional_light(light);
                    });

                // Add point lights
                registry.each<nexus::PointLightComponent, nexus::Transform3DComponent>(
                    [&](nexus::Entity, nexus::PointLightComponent& pl,
                        nexus::Transform3DComponent& t) {
                        nexus::PointLight light;
                        light.position = t.position;
                        light.color = pl.color;
                        light.intensity = pl.intensity;
                        light.radius = pl.radius;
                        renderer_3d.add_point_light(light);
                    });

                // Draw meshes using the mesh registry
                registry.each<nexus::MeshRendererComponent, nexus::Transform3DComponent>(
                    [&](nexus::Entity, nexus::MeshRendererComponent& mr,
                        nexus::Transform3DComponent& t) {
                        auto it = mesh_cache.find(mr.mesh_id);
                        if (it != mesh_cache.end()) {
                            renderer_3d.draw_mesh(it->second, t.world_matrix);
                        }
                    });

                renderer_3d.end();
            }

            // ── 2D rendering pass ───────────────────────────────────────
            if (has_2d_camera) {
                renderer_2d.begin(cam_2d);

                registry.each<nexus::SpriteRendererComponent, nexus::Transform2DComponent>(
                    [&](nexus::Entity, nexus::SpriteRendererComponent& sr,
                        nexus::Transform2DComponent& t) {
                        renderer_2d.draw_quad(
                            t.world_position, t.world_scale,
                            t.world_rotation, sr.color);
                    });

                renderer_2d.end();
            }

            rhi->end_frame();
            window.swap_buffers();

            if (nexus::Input::key_pressed(nexus::Key::Escape)) break;
        }

        // ── Shutdown (reverse init order) ───────────────────────────────
        audio.stop_all();
        for (auto& [id, mesh] : mesh_cache) {
            renderer_3d.destroy_mesh(mesh);
        }
        mesh_cache.clear();
        renderer_2d.shutdown();
        renderer_3d.shutdown();
        rhi->shutdown();
        NX_APP_INFO("NexusEngine Runtime shutting down...");

    } catch (const std::exception& e) {
        NX_ERROR("Fatal error: {}", e.what());
        nexus::Log::shutdown();
        return 1;
    }

    nexus::Log::shutdown();
    return 0;
}
