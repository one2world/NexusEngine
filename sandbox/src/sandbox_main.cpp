// NexusEngine Sandbox — exercises the engine end-to-end with Scene + ECS
// + real Lua scripts driving entity transforms.
//
// Scene composition (built once at startup):
//   - Sun         : directional light entity, fixed direction
//   - PointLight  : orbiting point light (animated by C++ for now —
//                    light components don't expose script transforms yet)
//   - Ground      : static plane mesh
//   - Cube        : MeshRenderer, animated by `spinner.lua`     (yaw spin)
//   - Sphere      : MeshRenderer, animated by `bouncer.lua`     (vertical bounce)
//   - Cube2       : MeshRenderer, animated by `pulse_scaler.lua` (uniform pulse)
//   - 2D shapes   : a small Quad/Circle/Rect group rendered when Space toggles
//                    to 2D mode.  Same renderer surface as the old demo,
//                    just behind a Scene-aware iteration.
//
// Per-frame loop:
//   1. Poll input
//   2. Run ScriptSystem.update_scripts(registry, dt) — every entity with a
//      ScriptComponent fires `<script_name>.on_update(entity, dt)`, which is
//      defined in Lua.  The script mutates Transform3DComponent.position /
//      .rotation / .scale.
//   3. Hierarchy::propagate_transforms_3d — recompute world_matrix from
//      the script-modified locals.  Renderer reads world_matrix.
//   4. Walk the registry to drive renderer_3d (lights + meshes).
//
// Demonstrates: ScriptComponent + Lua dispatch, Entity.set_rotation_euler,
// Entity.set_position, Entity.set_scale, Math.sin/cos/abs, the Lua
// stdlib (math.* / tostring / print) — all running through PUC-Rio
// Lua 5.4.

#include "nexus/core/log.h"
#include "nexus/core/timer.h"
#include "nexus/core/math.h"
#include "nexus/platform/input.h"
#include "nexus/platform/window.h"
#include "nexus/rhi/rhi.h"
#include "nexus/rhi/gl_functions.h"
#include "nexus/renderer/batch_renderer_2d.h"
#include "nexus/renderer/forward_renderer_3d.h"
#include "nexus/renderer/shadow_system.h"
#include "nexus/scene/scene.h"
#include "nexus/scene/registry.h"
#include "nexus/scene/components.h"
#include "nexus/scene/hierarchy.h"
#include "nexus/scripting/script_engine.h"
#include "nexus/scripting/lua_backend.h"
#include "nexus/scripting/script_component.h"
#include "nexus/scripting/engine_bindings.h"

#include <filesystem>
#include <unordered_map>

// Forward declare GLFW proc address getter
struct GLFWwindow;
extern "C" {
    typedef void (*GLFWglproc)(void);
    GLFWglproc glfwGetProcAddress(const char* procname);
}

namespace {

// Resolve a script asset path.  We look for sandbox/assets/scripts/
// relative to (a) the executable's directory and (b) the source-tree
// fallback so `./build/sandbox/nexus-sandbox` works whether run from the
// repo root or the build dir.
std::string resolve_script(const std::string& name) {
    namespace fs = std::filesystem;
    const std::string filename = name + ".lua";
    const fs::path candidates[] = {
        fs::path("sandbox") / "assets" / "scripts" / filename,
        fs::path("../sandbox") / "assets" / "scripts" / filename,
        fs::path("../../sandbox") / "assets" / "scripts" / filename,
        fs::path("assets") / "scripts" / filename,
    };
    std::error_code ec;
    for (const auto& p : candidates) {
        if (fs::exists(p, ec)) return fs::absolute(p, ec).string();
    }
    return filename;  // last resort — execute_file will report the failure
}

} // namespace

int main() {
    nexus::Log::init();
    NX_APP_INFO("Sandbox starting...");

    nexus::WindowConfig config;
    config.title = "NexusEngine Sandbox — Lua-driven scene";
    nexus::Window window(config);
    NX_APP_INFO("Window created: {}x{}", config.width, config.height);

    nexus::Input::init(window.native_handle());

    if (!nexus::rhi::gl::load(reinterpret_cast<nexus::rhi::gl::GLLoadProc>(glfwGetProcAddress))) {
        NX_ERROR("Failed to load OpenGL functions");
        return 1;
    }

    auto rhi = nexus::rhi::RHI::create();
    rhi->init();

    nexus::BatchRenderer2D renderer_2d;
    renderer_2d.init(rhi.get());

    nexus::ForwardRenderer3D renderer_3d;
    renderer_3d.init(rhi.get());

    // Cascaded shadow mapping — 3 cascades at 2048² is the modern default
    // for sun-driven shadows.  Splits favour log distribution so near-
    // camera detail dominates; bias / normal_bias tuned to kill acne on
    // the ground plane without producing visible peter-panning.
    nexus::CascadedShadowMap::Config csm_cfg;
    csm_cfg.resolution           = 2048;
    csm_cfg.num_cascades         = 3;
    csm_cfg.cascade_split_lambda = 0.85f;
    csm_cfg.shadow_distance      = 60.0f;
    csm_cfg.bias                 = 0.0015f;
    csm_cfg.normal_bias          = 0.05f;
    renderer_3d.enable_shadows(csm_cfg);

    // Primitive meshes uploaded once; we keep handles to them by id so
    // MeshRendererComponent.mesh_id can route entities to their geometry.
    auto cube_mesh   = nexus::create_cube_mesh();
    auto plane_mesh  = nexus::create_plane_mesh(20.0f, 4);
    auto sphere_mesh = nexus::create_sphere_mesh(0.5f, 16, 32);
    renderer_3d.upload_mesh(cube_mesh);
    renderer_3d.upload_mesh(plane_mesh);
    renderer_3d.upload_mesh(sphere_mesh);

    // ── Scene + ECS ─────────────────────────────────────────────────────
    nexus::Scene scene;
    auto& reg = scene.registry();

    // Mesh-id table: MeshRendererComponent.mesh_id is just a u32 the
    // sandbox owns; map back to the actual Mesh object during render.
    constexpr nexus::u32 kMeshCube   = 1;
    constexpr nexus::u32 kMeshPlane  = 2;
    constexpr nexus::u32 kMeshSphere = 3;
    std::unordered_map<nexus::u32, const nexus::Mesh*> mesh_table {
        { kMeshCube,   &cube_mesh   },
        { kMeshPlane,  &plane_mesh  },
        { kMeshSphere, &sphere_mesh },
    };

    // ── Material catalog ───────────────────────────────────────────────
    // Each surface gets its own SurfaceMaterial.  Lighting "knobs"
    // (specular weight, shininess, diffuse wrap, ambient response,
    // emissive) live here, NOT in the shader.  A polished sphere can
    // sit next to a matte ground plane without one preset flattening
    // the other.
    constexpr nexus::u32 kMatGround = 100;
    constexpr nexus::u32 kMatRedPlastic = 101;
    constexpr nexus::u32 kMatBlueSphere = 102;
    constexpr nexus::u32 kMatGoldCube   = 103;

    {
        nexus::SurfaceMaterial m;
        // Matte ground — wide diffuse wrap so it reads as a soft fill,
        // very low specular so footprints don't shine.
        m.albedo            = nexus::Vec4(0.30f, 0.50f, 0.30f, 1.0f);
        m.specular_strength = 0.05f;
        m.shininess         = 8.0f;
        m.diffuse_wrap      = 0.20f;
        m.ambient_response  = 1.0f;
        renderer_3d.upload_material(kMatGround, m);
    }
    {
        nexus::SurfaceMaterial m;
        // Glossy red plastic — tight highlight, mild wrap.
        m.albedo            = nexus::Vec4(0.80f, 0.30f, 0.20f, 1.0f);
        m.specular_color    = nexus::Vec3(1.0f, 1.0f, 1.0f);
        m.specular_strength = 0.6f;
        m.shininess         = 96.0f;
        m.diffuse_wrap      = 0.10f;
        m.ambient_response  = 0.85f;
        renderer_3d.upload_material(kMatRedPlastic, m);
    }
    {
        nexus::SurfaceMaterial m;
        // Smooth blue ceramic — wide wrap to make the bouncing sphere
        // read as evenly lit (the demo that motivated this rewrite).
        m.albedo            = nexus::Vec4(0.20f, 0.60f, 0.90f, 1.0f);
        m.specular_color    = nexus::Vec3(1.0f, 1.0f, 1.0f);
        m.specular_strength = 0.4f;
        m.shininess         = 48.0f;
        m.diffuse_wrap      = 0.30f;
        m.ambient_response  = 1.0f;
        renderer_3d.upload_material(kMatBlueSphere, m);
    }
    {
        nexus::SurfaceMaterial m;
        // Warm gold — coloured specular, very tight highlight, slight
        // emissive tint so it reads as "metal".
        m.albedo            = nexus::Vec4(0.90f, 0.80f, 0.20f, 1.0f);
        m.specular_color    = nexus::Vec3(1.0f, 0.85f, 0.45f);
        m.specular_strength = 0.9f;
        m.shininess         = 128.0f;
        m.diffuse_wrap      = 0.05f;
        m.ambient_response  = 0.7f;
        m.emissive          = nexus::Vec3(0.10f, 0.08f, 0.02f);
        m.emissive_strength = 1.0f;
        renderer_3d.upload_material(kMatGoldCube, m);
    }

    // Sun (directional light).
    {
        nexus::Entity sun = scene.create_entity_3d("Sun");
        auto& dl = reg.add_component<nexus::DirectionalLightComponent>(sun, {});
        dl.direction = nexus::Vec3(-0.5f, -1.0f, -0.3f);
        dl.color     = nexus::Vec3(1.0f, 0.95f, 0.8f);
        dl.intensity = 1.0f;
    }

    // Orbiting point light — driven by C++ each frame because there's no
    // script binding for PointLightComponent yet.  Position lives in the
    // entity's Transform3DComponent and the renderer pulls
    // tc.world_matrix[3] as the light's world position.
    nexus::Entity point_light = nexus::INVALID_ENTITY;
    {
        point_light = scene.create_entity_3d("PointLight");
        auto& pl = reg.add_component<nexus::PointLightComponent>(point_light, {});
        pl.color     = nexus::Vec3(0.2f, 0.5f, 1.0f);
        pl.intensity = 2.0f;
        pl.radius    = 8.0f;
    }

    // Ground plane.
    {
        nexus::Entity ground = scene.create_entity_3d("Ground");
        auto& gt = reg.get_component<nexus::Transform3DComponent>(ground);
        gt.position = nexus::Vec3(0.0f, -1.0f, 0.0f);
        auto& mr = reg.add_component<nexus::MeshRendererComponent>(ground, {});
        mr.mesh_id     = kMeshPlane;
        mr.material_id = kMatGround;
        // Tint stays at default (1,1,1,1) — material's albedo is the
        // single source of truth for surface colour.
    }

    // Cube — driven by spinner.lua, glossy red plastic.
    {
        nexus::Entity cube = scene.create_entity_3d("SpinningCube");
        auto& tc = reg.get_component<nexus::Transform3DComponent>(cube);
        tc.position = nexus::Vec3(0.0f, 0.5f, 0.0f);
        auto& mr = reg.add_component<nexus::MeshRendererComponent>(cube, {});
        mr.mesh_id     = kMeshCube;
        mr.material_id = kMatRedPlastic;
        nexus::scripting::ScriptComponent sc;
        sc.script_name = "spinner";
        sc.enabled     = true;
        reg.add_component<nexus::scripting::ScriptComponent>(cube, std::move(sc));
    }

    // Sphere — driven by bouncer.lua, smooth blue ceramic.
    {
        nexus::Entity ball = scene.create_entity_3d("BouncingSphere");
        auto& tc = reg.get_component<nexus::Transform3DComponent>(ball);
        tc.position = nexus::Vec3(3.0f, 0.0f, 0.0f);
        auto& mr = reg.add_component<nexus::MeshRendererComponent>(ball, {});
        mr.mesh_id     = kMeshSphere;
        mr.material_id = kMatBlueSphere;
        nexus::scripting::ScriptComponent sc;
        sc.script_name = "bouncer";
        sc.enabled     = true;
        reg.add_component<nexus::scripting::ScriptComponent>(ball, std::move(sc));
    }

    // Cube2 — driven by pulse_scaler.lua, warm gold.
    {
        nexus::Entity cube2 = scene.create_entity_3d("PulsingCube");
        auto& tc = reg.get_component<nexus::Transform3DComponent>(cube2);
        tc.position = nexus::Vec3(-3.0f, 0.0f, -2.0f);
        auto& mr = reg.add_component<nexus::MeshRendererComponent>(cube2, {});
        mr.mesh_id     = kMeshCube;
        mr.material_id = kMatGoldCube;
        nexus::scripting::ScriptComponent sc;
        sc.script_name = "pulse_scaler";
        sc.enabled     = true;
        reg.add_component<nexus::scripting::ScriptComponent>(cube2, std::move(sc));
    }

    // ── Scripting ───────────────────────────────────────────────────────
    nexus::scripting::ScriptEngine script_engine;
    nexus::scripting::bind_all(script_engine, reg);  // boots Lua + binds API
    auto& lua_backend = script_engine.lua_backend();

    // Surface Lua errors / print() calls in the Sandbox console so
    // misbehaving scripts are visible without an editor.
    script_engine.set_error_handler(
        [](const nexus::scripting::ScriptError& err) {
            NX_ERROR("[Lua] {}", err.message);
        });
    script_engine.set_print_sink(
        [](const std::string& msg) { NX_APP_INFO("[Lua] {}", msg); });

    // Load each script asset.  Each .lua file defines its module table
    // with .on_create / .on_update / .on_destroy hooks; ScriptSystem
    // dispatches into them by script_name.
    for (const char* name : { "spinner", "bouncer", "pulse_scaler" }) {
        const std::string path = resolve_script(name);
        if (!lua_backend.execute_file(path)) {
            NX_ERROR("Failed to load script '{}': {}", path, lua_backend.last_error());
        } else {
            NX_APP_INFO("Loaded script: {}", path);
        }
    }

    nexus::scripting::ScriptSystem script_system(&script_engine);
    script_system.initialize_scripts(reg);

    // ── Cameras ─────────────────────────────────────────────────────────
    // ShadowSystem orchestrates the depth pass — collects caster
    // meshes from the registry, drives CSM for the active directional
    // light, then re-binds the depth maps + cascade matrices to the
    // main forward shader.  The mesh resolver maps mesh_id → Mesh*.
    nexus::ShadowSystem shadow_system;
    shadow_system.set_mesh_resolver([&mesh_table](nexus::u32 id) {
        auto it = mesh_table.find(id);
        return it != mesh_table.end() ? it->second : nullptr;
    });

    nexus::Camera2D camera_2d;
    camera_2d.set_projection(static_cast<float>(config.width),
                              static_cast<float>(config.height));

    nexus::Camera3D camera_3d;
    camera_3d.position = nexus::Vec3(0.0f, 3.0f, 8.0f);
    camera_3d.look_at(nexus::Vec3(0.0f, 0.0f, 0.0f));
    camera_3d.set_perspective(window.aspect_ratio());

    nexus::Timer timer;
    bool show_3d = true;

    NX_APP_INFO("SPACE: toggle 2D/3D    ESC: exit");

    while (!window.should_close()) {
        window.poll_events();
        nexus::Input::update();
        timer.tick();

        if (nexus::Input::key_pressed(nexus::Key::Escape)) break;
        if (nexus::Input::key_pressed(nexus::Key::Space)) {
            show_3d = !show_3d;
            NX_APP_INFO("Switched to {} mode", show_3d ? "3D" : "2D");
        }

        const float dt   = timer.delta_time();
        const float time = static_cast<float>(timer.elapsed());

        // ── Run scripts ─────────────────────────────────────────────────
        // Each ScriptComponent's <script_name>.on_update(entity, dt) is
        // called; the Lua side mutates Transform3DComponent fields in
        // place via Entity.set_position / set_rotation_euler / set_scale.
        script_system.update_scripts(reg, dt);

        // C++-driven point light orbit (no script binding for lights).
        if (point_light != nexus::INVALID_ENTITY &&
            reg.has_component<nexus::Transform3DComponent>(point_light)) {
            auto& pt = reg.get_component<nexus::Transform3DComponent>(point_light);
            pt.position = nexus::Vec3(std::cos(time) * 3.0f, 1.5f,
                                       std::sin(time) * 3.0f);
        }

        // ── Propagate scripts' local-space edits into world_matrix ──────
        nexus::Hierarchy::propagate_transforms_3d(reg);

        rhi->begin_frame();
        rhi->set_viewport(0, 0, window.width(), window.height());

        if (show_3d) {
            rhi->clear(nexus::Vec4{0.1f, 0.1f, 0.15f, 1.0f});

            // Slowly orbit camera around origin so the user sees motion
            // even before scripts kick in.
            const float cam_angle = time * 0.3f;
            camera_3d.position = nexus::Vec3(
                std::sin(cam_angle) * 8.0f, 3.0f, std::cos(cam_angle) * 8.0f);
            camera_3d.look_at(nexus::Vec3(0.0f, 0.0f, 0.0f));
            camera_3d.set_perspective(window.aspect_ratio());

            renderer_3d.begin(camera_3d);

            // Shadow depth pass — runs BEFORE the colour-pass draws.
            // ShadowSystem renders each cascade's depth FBO then
            // restores the main-pass viewport + framebuffer so the
            // colour-pass draw_mesh calls land on the host's intended
            // target (without restoration the GL viewport would be
            // stuck at the shadow resolution and entities would pop
            // in/out of frame as the camera orbits).
            nexus::ShadowSystem::MainPassTarget target;
            target.viewport_w = window.width();
            target.viewport_h = window.height();
            target.fbo        = nexus::rhi::INVALID_HANDLE;  // default FB
            shadow_system.render(renderer_3d, reg, target);

            // Lights — pulled from ECS so any future script that
            // mutates them is honoured.  Pushed *after* the shadow
            // pass so cast_shadows flags reach the shader uniforms.
            reg.each<nexus::DirectionalLightComponent>(
                [&](nexus::Entity, nexus::DirectionalLightComponent& dl) {
                    nexus::DirectionalLight light;
                    light.direction    = dl.direction;
                    light.color        = dl.color;
                    light.intensity    = dl.intensity;
                    light.cast_shadows = dl.cast_shadows;
                    renderer_3d.set_directional_light(light);
                });
            reg.each<nexus::PointLightComponent, nexus::Transform3DComponent>(
                [&](nexus::Entity, nexus::PointLightComponent& pl,
                    nexus::Transform3DComponent& tc) {
                    nexus::PointLight light;
                    light.position  = nexus::Vec3(tc.world_matrix[3]);
                    light.color     = pl.color;
                    light.intensity = pl.intensity;
                    light.radius    = pl.radius;
                    renderer_3d.add_point_light(light);
                });

            // Mesh draws — Transform3DComponent.world_matrix already has
            // the script-driven values folded in.
            reg.each<nexus::MeshRendererComponent, nexus::Transform3DComponent>(
                [&](nexus::Entity, nexus::MeshRendererComponent& mr,
                    nexus::Transform3DComponent& tc) {
                    auto it = mesh_table.find(mr.mesh_id);
                    if (it == mesh_table.end() || it->second == nullptr) return;
                    renderer_3d.draw_mesh(*it->second, tc.world_matrix,
                                            mr.material_id, mr.receive_shadows,
                                            mr.tint);
                });

            renderer_3d.end();
        } else {
            rhi->clear(nexus::Vec4{0.15f, 0.15f, 0.2f, 1.0f});
            renderer_2d.reset_stats();
            renderer_2d.begin(camera_2d);

            // Procedural grid + accent shapes — pure visual filler so
            // the 2D toggle remains useful while the 3D scene shows the
            // Lua-driven entities.
            for (int y = 0; y < 8; ++y) {
                for (int x = 0; x < 10; ++x) {
                    const float r = static_cast<float>(x) / 10.0f;
                    const float g = static_cast<float>(y) / 8.0f;
                    const float b = 0.5f + 0.5f *
                        std::sin(time + static_cast<float>(x + y));
                    renderer_2d.draw_quad(
                        nexus::Vec2(100.0f + static_cast<float>(x) * 110.0f,
                                    80.0f  + static_cast<float>(y) * 75.0f),
                        nexus::Vec2(100.0f, 65.0f),
                        nexus::Vec4(r, g, b, 1.0f));
                }
            }
            renderer_2d.draw_quad(nexus::Vec2(640.0f, 400.0f),
                                   nexus::Vec2(80.0f, 80.0f), time,
                                   nexus::Vec4(1.0f, 0.3f, 0.3f, 0.9f));
            renderer_2d.draw_circle(nexus::Vec2(200.0f, 600.0f), 40.0f,
                                     nexus::Vec4(0.2f, 0.8f, 0.4f, 1.0f));
            renderer_2d.draw_rect(nexus::Vec2(400.0f, 580.0f),
                                   nexus::Vec2(150.0f, 50.0f),
                                   nexus::Vec4(0.9f, 0.9f, 0.2f, 1.0f), 2.0f);
            renderer_2d.end();
        }

        rhi->end_frame();
        window.swap_buffers();
    }

    // Drain on_destroy hooks before the registry tears down, so scripts
    // can release per-entity state cleanly.
    script_system.destroy_scripts(reg);

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
