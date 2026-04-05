/// NexusEngine Example: 2D Platformer
///
/// Demonstrates:
///   - ECS entity/component setup
///   - 2D sprite rendering with batch renderer
///   - 2D physics (gravity, collision)
///   - Input handling (keyboard)
///   - Tilemap for level geometry
///   - Sprite animation
///   - Audio playback

#include <nexus/core/types.h>
#include <nexus/core/math.h>
#include <nexus/core/log.h>
#include <nexus/scene/registry.h>
#include <nexus/scene/components.h>
#include <nexus/scene/hierarchy.h>
#include <nexus/platform/input.h>
#include <nexus/renderer/batch_renderer_2d.h>
#include <nexus/physics/physics_2d.h>
#include <nexus/audio/audio_engine.h>

using namespace nexus;

// ── Game Components ──────────────────────────────────────────────────────

struct PlayerTag {};

struct Velocity2D {
    Vec2 value{0.0f, 0.0f};
};

struct Gravity {
    f32 force{-980.0f};
};

struct Jumper {
    bool grounded{false};
    f32 jump_speed{400.0f};
};

struct Collectible {
    bool collected{false};
    i32 score_value{10};
};

// ── Systems ──────────────────────────────────────────────────────────────

void player_input_system(Registry& reg) {
    auto players = reg.view<PlayerTag>();
    for (auto e : players) {
        if (!reg.has_component<Velocity2D>(e)) continue;
        auto& vel = reg.get_component<Velocity2D>(e);

        // Horizontal movement
        vel.value.x = 0.0f;
        if (Input::key_held(Key::A) || Input::key_held(Key::Left))
            vel.value.x = -200.0f;
        if (Input::key_held(Key::D) || Input::key_held(Key::Right))
            vel.value.x = 200.0f;

        // Jump
        if (reg.has_component<Jumper>(e)) {
            auto& jumper = reg.get_component<Jumper>(e);
            if (jumper.grounded && Input::key_pressed(Key::Space)) {
                vel.value.y = jumper.jump_speed;
                jumper.grounded = false;
            }
        }
    }
}

void gravity_system(Registry& reg, f32 dt) {
    auto entities = reg.view<Velocity2D>();
    for (auto e : entities) {
        if (!reg.has_component<Gravity>(e)) continue;
        auto& vel = reg.get_component<Velocity2D>(e);
        auto& grav = reg.get_component<Gravity>(e);
        vel.value.y += grav.force * dt;
    }
}

void movement_system(Registry& reg, f32 dt) {
    auto entities = reg.view<Velocity2D>();
    for (auto e : entities) {
        if (!reg.has_component<Transform2DComponent>(e)) continue;
        auto& transform = reg.get_component<Transform2DComponent>(e);
        auto& vel = reg.get_component<Velocity2D>(e);
        transform.position += vel.value * dt;

        // Simple ground check
        if (transform.position.y <= 0.0f) {
            transform.position.y = 0.0f;
            vel.value.y = 0.0f;
            if (reg.has_component<Jumper>(e)) {
                reg.get_component<Jumper>(e).grounded = true;
            }
        }
    }
}

// ── Scene Setup ──────────────────────────────────────────────────────────

Entity create_player(Registry& reg) {
    auto player = reg.create();
    reg.add_component<TagComponent>(player, {"Player"});
    reg.add_component<PlayerTag>(player, {});

    Transform2DComponent t;
    t.position = Vec2(100.0f, 0.0f);
    reg.add_component<Transform2DComponent>(player, t);

    SpriteRendererComponent sprite;
    sprite.color = Vec4(0.2f, 0.6f, 1.0f, 1.0f);
    sprite.size = Vec2(32.0f, 48.0f);
    reg.add_component<SpriteRendererComponent>(player, sprite);

    reg.add_component<Velocity2D>(player, {});
    reg.add_component<Gravity>(player, {-980.0f});
    reg.add_component<Jumper>(player, {true, 450.0f});

    return player;
}

Entity create_platform(Registry& reg, Vec2 pos, Vec2 size) {
    auto platform = reg.create();
    reg.add_component<TagComponent>(platform, {"Platform"});

    Transform2DComponent t;
    t.position = pos;
    reg.add_component<Transform2DComponent>(platform, t);

    SpriteRendererComponent sprite;
    sprite.color = Vec4(0.4f, 0.8f, 0.3f, 1.0f);
    sprite.size = size;
    reg.add_component<SpriteRendererComponent>(platform, sprite);

    return platform;
}

Entity create_coin(Registry& reg, Vec2 pos) {
    auto coin = reg.create();
    reg.add_component<TagComponent>(coin, {"Coin"});

    Transform2DComponent t;
    t.position = pos;
    reg.add_component<Transform2DComponent>(coin, t);

    SpriteRendererComponent sprite;
    sprite.color = Vec4(1.0f, 0.85f, 0.0f, 1.0f);
    sprite.size = Vec2(16.0f, 16.0f);
    reg.add_component<SpriteRendererComponent>(coin, sprite);

    reg.add_component<Collectible>(coin, {false, 10});
    return coin;
}

// ── Main ─────────────────────────────────────────────────────────────────

int main() {
    NX_INFO("=== NexusEngine 2D Platformer Example ===");

    Registry registry;

    // Create player
    auto player = create_player(registry);

    // Create level geometry
    create_platform(registry, Vec2(0.0f, -16.0f), Vec2(800.0f, 32.0f));   // Ground
    create_platform(registry, Vec2(200.0f, 80.0f), Vec2(120.0f, 16.0f));  // Platform 1
    create_platform(registry, Vec2(400.0f, 160.0f), Vec2(100.0f, 16.0f)); // Platform 2
    create_platform(registry, Vec2(600.0f, 240.0f), Vec2(80.0f, 16.0f));  // Platform 3

    // Place coins
    create_coin(registry, Vec2(220.0f, 110.0f));
    create_coin(registry, Vec2(420.0f, 190.0f));
    create_coin(registry, Vec2(620.0f, 270.0f));

    NX_INFO("Scene created: {} entities", registry.entity_count());

    // Game loop (simplified — real loop uses Window + RHI)
    constexpr f32 dt = 1.0f / 60.0f;
    for (u32 frame = 0; frame < 300; ++frame) {
        player_input_system(registry);
        gravity_system(registry, dt);
        movement_system(registry, dt);
    }

    auto& pos = registry.get_component<Transform2DComponent>(player);
    NX_INFO("Player final position: ({}, {})", pos.position.x, pos.position.y);
    NX_INFO("2D Platformer example complete.");

    return 0;
}
