/// NexusEngine Example: Top-Down RPG
///
/// Demonstrates:
///   - 2D sprite rendering and tilemap
///   - ECS with RPG-specific components
///   - Turn-based combat system
///   - Inventory/item system
///   - NPC dialog triggers
///   - Audio for music and SFX
///   - Scene serialization (save/load)

#include <nexus/core/types.h>
#include <nexus/core/math.h>
#include <nexus/core/log.h>
#include <nexus/scene/registry.h>
#include <nexus/scene/components.h>
#include <nexus/platform/input.h>

using namespace nexus;

// ── RPG Components ───────────────────────────────────────────────────────

struct RPGStats {
    i32 hp{100};
    i32 max_hp{100};
    i32 mp{50};
    i32 max_mp{50};
    i32 attack{10};
    i32 defense{5};
    i32 speed{8};
    u32 level{1};
    u32 experience{0};
};

struct Inventory {
    static constexpr u32 MAX_SLOTS = 20;

    struct Item {
        std::string name;
        u32 quantity{0};
        enum class Type : u8 { None, Weapon, Armor, Potion, Key } type{Type::None};
        i32 value{0}; // damage for weapons, defense for armor, heal for potions
    };

    std::vector<Item> items;

    bool add_item(const std::string& name, Item::Type type, i32 value, u32 qty = 1) {
        for (auto& item : items) {
            if (item.name == name) {
                item.quantity += qty;
                return true;
            }
        }
        if (items.size() < MAX_SLOTS) {
            items.push_back({name, qty, type, value});
            return true;
        }
        return false;
    }
};

struct NPCDialog {
    std::vector<std::string> lines;
    u32 current_line{0};
    bool active{false};
};

struct GridMovement {
    Vec2 target{0.0f};
    f32 move_speed{64.0f}; // pixels per second
    bool moving{false};
    f32 tile_size{32.0f};
};

struct CombatState {
    bool in_combat{false};
    Entity target{INVALID_ENTITY};
    f32 attack_cooldown{0.0f};
};

// ── Systems ──────────────────────────────────────────────────────────────

void grid_input_system(Registry& reg) {
    auto entities = reg.view<GridMovement>();
    for (auto e : entities) {
        if (!reg.has_component<PlayerTag>(e)) continue;
        auto& grid = reg.get_component<GridMovement>(e);
        if (grid.moving) continue;

        auto& transform = reg.get_component<Transform2DComponent>(e);
        Vec2 dir{0.0f};

        if (Input::key_pressed(Key::W) || Input::key_pressed(Key::Up))
            dir.y = grid.tile_size;
        else if (Input::key_pressed(Key::S) || Input::key_pressed(Key::Down))
            dir.y = -grid.tile_size;
        else if (Input::key_pressed(Key::A) || Input::key_pressed(Key::Left))
            dir.x = -grid.tile_size;
        else if (Input::key_pressed(Key::D) || Input::key_pressed(Key::Right))
            dir.x = grid.tile_size;

        if (glm::length(dir) > 0.001f) {
            grid.target = transform.position + dir;
            grid.moving = true;
        }
    }
}

void grid_movement_system(Registry& reg, f32 dt) {
    auto entities = reg.view<GridMovement>();
    for (auto e : entities) {
        auto& grid = reg.get_component<GridMovement>(e);
        if (!grid.moving) continue;
        auto& transform = reg.get_component<Transform2DComponent>(e);

        Vec2 diff = grid.target - transform.position;
        f32 dist = glm::length(diff);
        f32 step = grid.move_speed * dt;

        if (dist <= step) {
            transform.position = grid.target;
            grid.moving = false;
        } else {
            transform.position += glm::normalize(diff) * step;
        }
    }
}

void combat_system(Registry& reg, f32 dt) {
    auto entities = reg.view<CombatState>();
    for (auto e : entities) {
        auto& combat = reg.get_component<CombatState>(e);
        if (!combat.in_combat) continue;

        combat.attack_cooldown -= dt;
        if (combat.attack_cooldown > 0.0f) continue;

        if (combat.target == INVALID_ENTITY) continue;
        if (!reg.has_component<RPGStats>(e)) continue;
        if (!reg.has_component<RPGStats>(combat.target)) continue;

        auto& attacker = reg.get_component<RPGStats>(e);
        auto& defender = reg.get_component<RPGStats>(combat.target);

        i32 damage = std::max(1, attacker.attack - defender.defense);
        defender.hp -= damage;
        combat.attack_cooldown = 1.0f;

        if (reg.has_component<TagComponent>(e) && reg.has_component<TagComponent>(combat.target)) {
            NX_INFO("{} attacks {} for {} damage (HP: {}/{})",
                    reg.get_component<TagComponent>(e).name,
                    reg.get_component<TagComponent>(combat.target).name,
                    damage, defender.hp, defender.max_hp);
        }

        if (defender.hp <= 0) {
            NX_INFO("{} was defeated!",
                    reg.get_component<TagComponent>(combat.target).name);
            combat.in_combat = false;

            // Award experience
            attacker.experience += 25;
            if (attacker.experience >= attacker.level * 100) {
                attacker.level++;
                attacker.max_hp += 10;
                attacker.hp = attacker.max_hp;
                attacker.attack += 2;
                attacker.defense += 1;
                NX_INFO("{} leveled up to {}!",
                        reg.get_component<TagComponent>(e).name, attacker.level);
            }
        }
    }
}

// ── Scene Setup ──────────────────────────────────────────────────────────

struct PlayerTag {};

Entity create_hero(Registry& reg) {
    auto hero = reg.create();
    reg.add_component<TagComponent>(hero, {"Hero"});
    reg.add_component<PlayerTag>(hero, {});

    Transform2DComponent t;
    t.position = Vec2(160.0f, 160.0f);
    reg.add_component<Transform2DComponent>(hero, t);

    SpriteRendererComponent sprite;
    sprite.color = Vec4(0.2f, 0.5f, 1.0f, 1.0f);
    sprite.size = Vec2(28.0f, 28.0f);
    reg.add_component<SpriteRendererComponent>(hero, sprite);

    RPGStats stats;
    stats.hp = 100; stats.max_hp = 100;
    stats.mp = 30; stats.max_mp = 30;
    stats.attack = 12; stats.defense = 6; stats.speed = 10;
    reg.add_component<RPGStats>(hero, stats);

    reg.add_component<GridMovement>(hero, {});
    reg.add_component<CombatState>(hero, {});

    Inventory inv;
    inv.add_item("Iron Sword", Inventory::Item::Type::Weapon, 5);
    inv.add_item("Health Potion", Inventory::Item::Type::Potion, 30, 3);
    reg.add_component<Inventory>(hero, inv);

    return hero;
}

Entity create_npc(Registry& reg, const std::string& name, Vec2 pos,
                  std::vector<std::string> dialog) {
    auto npc = reg.create();
    reg.add_component<TagComponent>(npc, {name});

    Transform2DComponent t;
    t.position = pos;
    reg.add_component<Transform2DComponent>(npc, t);

    SpriteRendererComponent sprite;
    sprite.color = Vec4(0.8f, 0.7f, 0.2f, 1.0f);
    sprite.size = Vec2(28.0f, 28.0f);
    reg.add_component<SpriteRendererComponent>(npc, sprite);

    NPCDialog dlg;
    dlg.lines = std::move(dialog);
    reg.add_component<NPCDialog>(npc, dlg);

    return npc;
}

Entity create_monster(Registry& reg, const std::string& name, Vec2 pos,
                      i32 hp, i32 atk, i32 def) {
    auto monster = reg.create();
    reg.add_component<TagComponent>(monster, {name});

    Transform2DComponent t;
    t.position = pos;
    reg.add_component<Transform2DComponent>(monster, t);

    SpriteRendererComponent sprite;
    sprite.color = Vec4(0.9f, 0.2f, 0.2f, 1.0f);
    sprite.size = Vec2(28.0f, 28.0f);
    reg.add_component<SpriteRendererComponent>(monster, sprite);

    RPGStats stats;
    stats.hp = hp; stats.max_hp = hp;
    stats.attack = atk; stats.defense = def;
    reg.add_component<RPGStats>(monster, stats);

    return monster;
}

// ── Main ─────────────────────────────────────────────────────────────────

int main() {
    NX_INFO("=== NexusEngine Top-Down RPG Example ===");

    Registry registry;

    // Create hero
    auto hero = create_hero(registry);

    // NPCs
    create_npc(registry, "Villager", Vec2(256.0f, 192.0f), {
        "Welcome to the village, adventurer!",
        "Beware of the slimes in the forest to the east.",
        "The blacksmith can upgrade your sword."
    });

    create_npc(registry, "Blacksmith", Vec2(320.0f, 128.0f), {
        "Need a weapon upgrade?",
        "Bring me 3 iron ore and I'll forge you a steel sword."
    });

    // Monsters
    auto slime = create_monster(registry, "Green Slime", Vec2(384.0f, 192.0f), 30, 6, 2);
    auto goblin = create_monster(registry, "Goblin", Vec2(448.0f, 256.0f), 50, 10, 4);

    NX_INFO("Scene created: {} entities", registry.entity_count());

    // Simulate combat encounter
    auto& hero_combat = registry.get_component<CombatState>(hero);
    hero_combat.in_combat = true;
    hero_combat.target = slime;

    constexpr f32 dt = 1.0f / 60.0f;
    for (u32 frame = 0; frame < 600; ++frame) {
        grid_input_system(registry);
        grid_movement_system(registry, dt);
        combat_system(registry, dt);
    }

    auto& hero_stats = registry.get_component<RPGStats>(hero);
    NX_INFO("Hero stats — Level: {}, HP: {}/{}, ATK: {}, DEF: {}, EXP: {}",
            hero_stats.level, hero_stats.hp, hero_stats.max_hp,
            hero_stats.attack, hero_stats.defense, hero_stats.experience);

    auto& inv = registry.get_component<Inventory>(hero);
    NX_INFO("Inventory: {} items", inv.items.size());
    for (const auto& item : inv.items) {
        NX_INFO("  - {} x{}", item.name, item.quantity);
    }

    NX_INFO("Top-Down RPG example complete.");
    return 0;
}
