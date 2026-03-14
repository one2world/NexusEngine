#include "nexus/core/log.h"
#include "nexus/core/timer.h"
#include "nexus/core/math.h"
#include "nexus/platform/input.h"
#include "nexus/platform/window.h"
#include "nexus/rhi/rhi.h"
#include "nexus/rhi/gl_functions.h"
#include "nexus/renderer/batch_renderer_2d.h"
#include "nexus/renderer/forward_renderer_3d.h"

// Forward declare GLFW proc address getter
struct GLFWwindow;
extern "C" {
    typedef void (*GLFWglproc)(void);
    GLFWglproc glfwGetProcAddress(const char* procname);
}

int main() {
    nexus::Log::init();
    NX_APP_INFO("Sandbox starting...");

    nexus::WindowConfig config;
    config.title = "NexusEngine Sandbox";
    nexus::Window window(config);
    NX_APP_INFO("Window created: {}x{}", config.width, config.height);

    nexus::Input::init(window.native_handle());

    // Load OpenGL functions
    if (!nexus::rhi::gl::load(reinterpret_cast<nexus::rhi::gl::GLLoadProc>(glfwGetProcAddress))) {
        NX_ERROR("Failed to load OpenGL functions");
        return 1;
    }

    // Create RHI
    auto rhi = nexus::rhi::RHI::create();
    rhi->init();

    // Initialize renderers
    nexus::BatchRenderer2D renderer_2d;
    renderer_2d.init(rhi.get());

    nexus::ForwardRenderer3D renderer_3d;
    renderer_3d.init(rhi.get());

    // Create primitive meshes
    auto cube_mesh = nexus::create_cube_mesh();
    renderer_3d.upload_mesh(cube_mesh);

    auto plane_mesh = nexus::create_plane_mesh(20.0f, 4);
    renderer_3d.upload_mesh(plane_mesh);

    auto sphere_mesh = nexus::create_sphere_mesh(0.5f, 16, 32);
    renderer_3d.upload_mesh(sphere_mesh);

    // Set up cameras
    nexus::Camera2D camera_2d;
    camera_2d.set_projection(static_cast<float>(config.width),
                             static_cast<float>(config.height));

    nexus::Camera3D camera_3d;
    camera_3d.position = nexus::Vec3(0.0f, 3.0f, 8.0f);
    camera_3d.look_at(nexus::Vec3(0.0f, 0.0f, 0.0f));
    camera_3d.set_perspective(window.aspect_ratio());

    nexus::Timer timer;
    bool show_3d = true;

    NX_APP_INFO("Press SPACE to toggle 2D/3D view");
    NX_APP_INFO("Press ESC to exit");

    while (!window.should_close()) {
        window.poll_events();
        nexus::Input::update();
        timer.tick();

        if (nexus::Input::key_pressed(nexus::Key::Escape)) break;

        // Toggle 2D/3D mode
        if (nexus::Input::key_pressed(nexus::Key::Space)) {
            show_3d = !show_3d;
            NX_APP_INFO("Switched to {} mode", show_3d ? "3D" : "2D");
        }

        float time = static_cast<float>(timer.elapsed());

        rhi->begin_frame();
        rhi->set_viewport(0, 0, window.width(), window.height());

        if (show_3d) {
            // ── 3D Scene ────────────────────────────────────────────────
            rhi->clear(nexus::Vec4{0.1f, 0.1f, 0.15f, 1.0f});

            // Slowly orbit camera
            float cam_angle = time * 0.3f;
            camera_3d.position = nexus::Vec3(
                std::sin(cam_angle) * 8.0f,
                3.0f,
                std::cos(cam_angle) * 8.0f
            );
            camera_3d.look_at(nexus::Vec3(0.0f, 0.0f, 0.0f));
            camera_3d.set_perspective(window.aspect_ratio());

            renderer_3d.begin(camera_3d);

            // Directional light
            nexus::DirectionalLight sun;
            sun.direction = nexus::Vec3(-0.5f, -1.0f, -0.3f);
            sun.color     = nexus::Vec3(1.0f, 0.95f, 0.8f);
            sun.intensity = 1.0f;
            renderer_3d.set_directional_light(sun);

            // Orbiting point light
            nexus::PointLight point;
            point.position = nexus::Vec3(std::cos(time) * 3.0f, 1.5f,
                                         std::sin(time) * 3.0f);
            point.color    = nexus::Vec3(0.2f, 0.5f, 1.0f);
            point.intensity = 2.0f;
            point.radius   = 8.0f;
            renderer_3d.add_point_light(point);

            // Ground plane
            nexus::Mat4 plane_xform(1.0f);
            plane_xform = glm::translate(plane_xform, nexus::Vec3(0.0f, -1.0f, 0.0f));
            renderer_3d.draw_mesh(plane_mesh, plane_xform,
                                  nexus::Vec4(0.3f, 0.5f, 0.3f, 1.0f));

            // Rotating cube
            nexus::Mat4 cube_xform(1.0f);
            cube_xform = glm::translate(cube_xform, nexus::Vec3(0.0f, 0.5f, 0.0f));
            cube_xform = glm::rotate(cube_xform, time * 0.5f, nexus::Vec3(0.0f, 1.0f, 0.0f));
            cube_xform = glm::rotate(cube_xform, time * 0.3f, nexus::Vec3(1.0f, 0.0f, 0.0f));
            renderer_3d.draw_mesh(cube_mesh, cube_xform,
                                  nexus::Vec4(0.8f, 0.3f, 0.2f, 1.0f));

            // Sphere
            nexus::Mat4 sphere_xform(1.0f);
            sphere_xform = glm::translate(sphere_xform, nexus::Vec3(3.0f, 0.0f, 0.0f));
            float bounce = std::abs(std::sin(time * 2.0f)) * 1.5f;
            sphere_xform = glm::translate(sphere_xform, nexus::Vec3(0.0f, bounce, 0.0f));
            renderer_3d.draw_mesh(sphere_mesh, sphere_xform,
                                  nexus::Vec4(0.2f, 0.6f, 0.9f, 1.0f));

            // Second cube
            nexus::Mat4 cube2_xform(1.0f);
            cube2_xform = glm::translate(cube2_xform, nexus::Vec3(-3.0f, 0.0f, -2.0f));
            cube2_xform = glm::rotate(cube2_xform, -time * 0.7f, nexus::Vec3(0.0f, 1.0f, 0.0f));
            cube2_xform = glm::scale(cube2_xform, nexus::Vec3(1.5f));
            renderer_3d.draw_mesh(cube_mesh, cube2_xform,
                                  nexus::Vec4(0.9f, 0.8f, 0.2f, 1.0f));

            renderer_3d.end();
        } else {
            // ── 2D Scene ────────────────────────────────────────────────
            rhi->clear(nexus::Vec4{0.15f, 0.15f, 0.2f, 1.0f});

            renderer_2d.reset_stats();
            renderer_2d.begin(camera_2d);

            // Grid of colored quads
            for (int y = 0; y < 8; ++y) {
                for (int x = 0; x < 10; ++x) {
                    float r = static_cast<float>(x) / 10.0f;
                    float g = static_cast<float>(y) / 8.0f;
                    float b = 0.5f + 0.5f * std::sin(time + static_cast<float>(x + y));
                    renderer_2d.draw_quad(
                        nexus::Vec2(100.0f + static_cast<float>(x) * 110.0f,
                                    80.0f + static_cast<float>(y) * 75.0f),
                        nexus::Vec2(100.0f, 65.0f),
                        nexus::Vec4(r, g, b, 1.0f)
                    );
                }
            }

            // Rotating quad
            renderer_2d.draw_quad(
                nexus::Vec2(640.0f, 400.0f),
                nexus::Vec2(80.0f, 80.0f),
                time,
                nexus::Vec4(1.0f, 0.3f, 0.3f, 0.9f)
            );

            // Shapes
            renderer_2d.draw_circle(nexus::Vec2(200.0f, 600.0f), 40.0f,
                                    nexus::Vec4(0.2f, 0.8f, 0.4f, 1.0f));
            renderer_2d.draw_rect(nexus::Vec2(400.0f, 580.0f),
                                  nexus::Vec2(150.0f, 50.0f),
                                  nexus::Vec4(0.9f, 0.9f, 0.2f, 1.0f), 2.0f);

            renderer_2d.end();
        }

        rhi->end_frame();
        window.swap_buffers();

        if (timer.frame_count() % 120 == 0 && timer.frame_count() > 0) {
            NX_APP_INFO("FPS: {:.1f}", timer.fps());
        }
    }

    // Cleanup
    renderer_3d.destroy_mesh(sphere_mesh);
    renderer_3d.destroy_mesh(plane_mesh);
    renderer_3d.destroy_mesh(cube_mesh);
    renderer_3d.shutdown();
    renderer_2d.shutdown();
    rhi->shutdown();

    NX_APP_INFO("Sandbox shutting down...");
    nexus::Log::shutdown();
    return 0;
}
