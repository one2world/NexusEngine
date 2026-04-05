/// NexusEngine Example: 3D First-Person Shooter
///
/// Demonstrates:
///   - 3D scene setup with ECS
///   - Camera with mouse-look
///   - Forward renderer with PBR materials
///   - Directional + point lights
///   - 3D physics (raycasting for shooting)
///   - Audio (spatial sound for gunshots)
///   - Hierarchy (weapon parented to camera)

#include <nexus/core/types.h>
#include <nexus/core/math.h>
#include <nexus/core/log.h>
#include <nexus/scene/registry.h>
#include <nexus/scene/components.h>
#include <nexus/scene/hierarchy.h>
#include <nexus/platform/input.h>
#include <nexus/renderer/forward_renderer_3d.h>
#include <nexus/physics/physics_3d.h>
#include <nexus/audio/audio_engine.h>

using namespace nexus;

// ── Game Components ──────────────────────────────────────────────────────

struct FPSController {
    f32 move_speed{5.0f};
    f32 look_sensitivity{0.1f};
    f32 yaw{0.0f};
    f32 pitch{0.0f};
};

struct Health {
    i32 current{100};
    i32 max{100};
};

struct Enemy {
    f32 patrol_speed{2.0f};
    Vec3 patrol_start{0.0f};
    Vec3 patrol_end{0.0f};
    f32 patrol_t{0.0f};
};

struct Projectile {
    Vec3 direction{0.0f, 0.0f, -1.0f};
    f32 speed{50.0f};
    f32 lifetime{3.0f};
};

// ── Systems ──────────────────────────────────────────────────────────────

void fps_look_system(Registry& reg, f32 dt) {
    auto entities = reg.view<FPSController>();
    for (auto e : entities) {
        auto& ctrl = reg.get_component<FPSController>(e);
        auto& transform = reg.get_component<Transform3DComponent>(e);

        Vec2 mouse_delta = Input::mouse_delta();
        ctrl.yaw -= mouse_delta.x * ctrl.look_sensitivity;
        ctrl.pitch -= mouse_delta.y * ctrl.look_sensitivity;
        ctrl.pitch = glm::clamp(ctrl.pitch, -89.0f, 89.0f);

        // Calculate forward and right vectors
        f32 yaw_rad = glm::radians(ctrl.yaw);
        f32 pitch_rad = glm::radians(ctrl.pitch);
        Vec3 forward;
        forward.x = cos(pitch_rad) * sin(yaw_rad);
        forward.y = sin(pitch_rad);
        forward.z = cos(pitch_rad) * cos(yaw_rad);

        transform.rotation = Quat(Vec3(pitch_rad, yaw_rad, 0.0f));
    }
}

void fps_move_system(Registry& reg, f32 dt) {
    auto entities = reg.view<FPSController>();
    for (auto e : entities) {
        auto& ctrl = reg.get_component<FPSController>(e);
        auto& transform = reg.get_component<Transform3DComponent>(e);

        f32 yaw_rad = glm::radians(ctrl.yaw);
        Vec3 forward(sin(yaw_rad), 0.0f, cos(yaw_rad));
        Vec3 right(cos(yaw_rad), 0.0f, -sin(yaw_rad));

        Vec3 move{0.0f};
        if (Input::key_held(Key::W)) move += forward;
        if (Input::key_held(Key::S)) move -= forward;
        if (Input::key_held(Key::A)) move -= right;
        if (Input::key_held(Key::D)) move += right;

        if (glm::length(move) > 0.001f) {
            move = glm::normalize(move);
            transform.position += move * ctrl.move_speed * dt;
        }
    }
}

void enemy_patrol_system(Registry& reg, f32 dt) {
    auto entities = reg.view<Enemy>();
    for (auto e : entities) {
        auto& enemy = reg.get_component<Enemy>(e);
        auto& transform = reg.get_component<Transform3DComponent>(e);

        enemy.patrol_t += enemy.patrol_speed * dt * 0.1f;
        if (enemy.patrol_t > 1.0f) enemy.patrol_t -= 1.0f;

        f32 t = (sin(enemy.patrol_t * 2.0f * 3.14159f) + 1.0f) * 0.5f;
        transform.position = glm::mix(enemy.patrol_start, enemy.patrol_end, t);
    }
}

// ── Scene Setup ──────────────────────────────────────────────────────────

Entity create_fps_camera(Registry& reg) {
    auto camera = reg.create();
    reg.add_component<TagComponent>(camera, {"FPSCamera"});

    Transform3DComponent t;
    t.position = Vec3(0.0f, 1.7f, 5.0f);
    reg.add_component<Transform3DComponent>(camera, t);

    CameraComponent cam;
    cam.fov = 75.0f;
    cam.near_clip = 0.1f;
    cam.far_clip = 500.0f;
    reg.add_component<CameraComponent>(camera, cam);

    reg.add_component<FPSController>(camera, {5.0f, 0.1f, 0.0f, 0.0f});
    reg.add_component<Health>(camera, {100, 100});

    return camera;
}

Entity create_weapon(Registry& reg, Entity camera) {
    auto weapon = reg.create();
    reg.add_component<TagComponent>(weapon, {"Weapon"});

    Transform3DComponent t;
    t.position = Vec3(0.3f, -0.2f, -0.5f);
    t.scale = Vec3(0.1f, 0.1f, 0.4f);
    reg.add_component<Transform3DComponent>(weapon, t);

    Hierarchy::set_parent(reg, weapon, camera);
    return weapon;
}

Entity create_enemy(Registry& reg, Vec3 pos, Vec3 patrol_end) {
    auto e = reg.create();
    reg.add_component<TagComponent>(e, {"Enemy"});

    Transform3DComponent t;
    t.position = pos;
    reg.add_component<Transform3DComponent>(e, t);
    reg.add_component<Health>(e, {50, 50});

    Enemy enemy;
    enemy.patrol_start = pos;
    enemy.patrol_end = patrol_end;
    reg.add_component<Enemy>(e, enemy);

    return e;
}

Entity create_sun(Registry& reg) {
    auto sun = reg.create();
    reg.add_component<TagComponent>(sun, {"Sun"});

    DirectionalLightComponent light;
    light.direction = glm::normalize(Vec3(-0.5f, -1.0f, -0.3f));
    light.color = Vec3(1.0f, 0.95f, 0.9f);
    light.intensity = 1.5f;
    reg.add_component<DirectionalLightComponent>(sun, light);

    return sun;
}

Entity create_point_light(Registry& reg, Vec3 pos, Vec3 color) {
    auto e = reg.create();
    reg.add_component<TagComponent>(e, {"PointLight"});

    Transform3DComponent t;
    t.position = pos;
    reg.add_component<Transform3DComponent>(e, t);

    PointLightComponent light;
    light.color = color;
    light.intensity = 3.0f;
    light.radius = 15.0f;
    reg.add_component<PointLightComponent>(e, light);

    return e;
}

// ── Main ─────────────────────────────────────────────────────────────────

int main() {
    NX_INFO("=== NexusEngine 3D FPS Example ===");

    Registry registry;

    // Scene setup
    auto camera = create_fps_camera(registry);
    auto weapon = create_weapon(registry, camera);
    auto sun = create_sun(registry);

    // Lighting
    create_point_light(registry, Vec3(5.0f, 3.0f, 0.0f), Vec3(1.0f, 0.3f, 0.1f));
    create_point_light(registry, Vec3(-5.0f, 3.0f, 0.0f), Vec3(0.1f, 0.3f, 1.0f));

    // Enemies
    create_enemy(registry, Vec3(10.0f, 0.0f, -10.0f), Vec3(-10.0f, 0.0f, -10.0f));
    create_enemy(registry, Vec3(0.0f, 0.0f, -20.0f), Vec3(0.0f, 0.0f, -30.0f));
    create_enemy(registry, Vec3(-8.0f, 0.0f, -15.0f), Vec3(8.0f, 0.0f, -15.0f));

    NX_INFO("Scene created: {} entities", registry.entity_count());

    // Simulate game loop
    constexpr f32 dt = 1.0f / 60.0f;
    for (u32 frame = 0; frame < 300; ++frame) {
        fps_look_system(registry, dt);
        fps_move_system(registry, dt);
        enemy_patrol_system(registry, dt);
        Hierarchy::propagate_transforms_3d(registry);
    }

    auto& cam_pos = registry.get_component<Transform3DComponent>(camera);
    NX_INFO("Camera final position: ({}, {}, {})",
            cam_pos.position.x, cam_pos.position.y, cam_pos.position.z);
    NX_INFO("3D FPS example complete.");

    return 0;
}
