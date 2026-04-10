// Integration tests for cross-subsystem pipelines in NexusEngine.

#include <gtest/gtest.h>

#include "nexus/core/types.h"
#include "nexus/core/math.h"

// ECS / Scene
#include "nexus/scene/registry.h"
#include "nexus/scene/components.h"
#include "nexus/scene/scene.h"
#include "nexus/scene/scene_serializer.h"

// Scripting
#include "nexus/scripting/script_engine.h"
#include "nexus/scripting/script_value.h"

// Assets
#include "nexus/assets/asset_registry.h"
#include "nexus/assets/asset_loader.h"

// Animation
#include "nexus/animation/animation_clip.h"
#include "nexus/animation/skeleton.h"

// Physics
#include "nexus/physics/physics_system.h"

// Network
#include "nexus/net/rpc.h"
#include "nexus/net/serialization.h"

using namespace nexus;

// =============================================================================
// Test 1: ECS + Serialization Round-trip
// =============================================================================

TEST(IntegrationPipeline, ECS_Serialization_RoundTrip) {
    // -- Build a scene with multiple entities and components --------------------
    Scene src_scene;
    Registry& src_reg = src_scene.registry();

    Entity e1 = src_scene.create_entity("Player");
    src_reg.add_component<Transform2DComponent>(e1, Transform2DComponent{
        {10.0f, 20.0f}, 1.5f, {2.0f, 2.0f}
    });

    Entity e2 = src_scene.create_entity("Enemy");
    src_reg.add_component<Transform2DComponent>(e2, Transform2DComponent{
        {-5.0f, 3.0f}, 0.0f, {1.0f, 1.0f}
    });

    Entity e3 = src_scene.create_entity("Camera");
    src_reg.add_component<CameraComponent>(e3, CameraComponent{
        true, true, 45.0f, 10.0f, 0.1f, 1000.0f
    });

    ASSERT_EQ(src_reg.size(), 3u);

    // -- Serialize to JSON string ---------------------------------------------
    SceneSerializer src_ser(src_scene);
    std::string json = src_ser.to_json();
    ASSERT_FALSE(json.empty());

    // -- Deserialize into a fresh scene ---------------------------------------
    Scene dst_scene;
    SceneSerializer dst_ser(dst_scene);
    bool ok = dst_ser.from_json(json);
    ASSERT_TRUE(ok);

    Registry& dst_reg = dst_scene.registry();
    EXPECT_EQ(dst_reg.size(), src_reg.size());

    // Verify that TagComponents round-tripped correctly.
    auto tagged = dst_reg.view<TagComponent>();
    ASSERT_EQ(tagged.size(), 3u);

    bool found_player = false;
    bool found_enemy  = false;
    bool found_camera = false;
    for (Entity e : tagged) {
        const auto& tag = dst_reg.get_component<TagComponent>(e);
        if (tag.name == "Player") found_player = true;
        if (tag.name == "Enemy")  found_enemy  = true;
        if (tag.name == "Camera") found_camera = true;
    }
    EXPECT_TRUE(found_player);
    EXPECT_TRUE(found_enemy);
    EXPECT_TRUE(found_camera);

    // Verify that Transform2DComponent data survived the round-trip.
    auto transforms = dst_reg.view<Transform2DComponent>();
    EXPECT_GE(transforms.size(), 2u);
}

// =============================================================================
// Test 2: ECS + Scripting Pipeline
// =============================================================================

TEST(IntegrationPipeline, ECS_Scripting_CreateEntitiesViaScript) {
    using namespace nexus::scripting;

    // -- Set up a registry and a ScriptEngine ---------------------------------
    Registry registry;
    ScriptEngine engine;

    // Register a native binding that creates an entity in the registry.
    engine.register_function("Entity", "create",
        [&registry](const std::vector<ScriptValue>& args) -> ScriptValue {
            Entity e = registry.create();
            if (!args.empty() && args[0].is_string()) {
                registry.add_component<TagComponent>(e, TagComponent{args[0].as_string()});
            }
            return ScriptValue::entity(static_cast<u32>(e));
        },
        0, 1, "Create a new entity");

    // Register a binding that adds a Transform2DComponent.
    engine.register_function("Entity", "set_position",
        [&registry](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (args.size() < 3) return ScriptValue::nil();
            Entity e = static_cast<Entity>(args[0].as_entity());
            float x  = args[1].as_float();
            float y  = args[2].as_float();
            if (!registry.has_component<Transform2DComponent>(e)) {
                registry.add_component<Transform2DComponent>(e, Transform2DComponent{});
            }
            registry.get_component<Transform2DComponent>(e).position = {x, y};
            return ScriptValue::nil();
        },
        3, 3, "Set entity 2D position");

    ASSERT_EQ(registry.size(), 0u);

    // -- Simulate script calls ------------------------------------------------
    ScriptValue hero = engine.call_function("Entity.create",
        {ScriptValue("Hero")});
    ASSERT_TRUE(hero.is_entity());

    ScriptValue npc = engine.call_function("Entity.create",
        {ScriptValue("NPC")});
    ASSERT_TRUE(npc.is_entity());

    engine.call_function("Entity.set_position",
        {hero, ScriptValue(42.0f), ScriptValue(7.0f)});

    // -- Verify ECS state reflects the scripted operations --------------------
    EXPECT_EQ(registry.size(), 2u);

    Entity hero_e = static_cast<Entity>(hero.as_entity());
    ASSERT_TRUE(registry.has_component<TagComponent>(hero_e));
    EXPECT_EQ(registry.get_component<TagComponent>(hero_e).name, "Hero");

    ASSERT_TRUE(registry.has_component<Transform2DComponent>(hero_e));
    auto& pos = registry.get_component<Transform2DComponent>(hero_e).position;
    EXPECT_FLOAT_EQ(pos.x, 42.0f);
    EXPECT_FLOAT_EQ(pos.y, 7.0f);
}

// =============================================================================
// Test 3: Asset Registry + Loader Pipeline
// =============================================================================

TEST(IntegrationPipeline, AssetRegistry_Loader_Pipeline) {
    using namespace nexus::assets;

    AssetRegistry registry;
    AssetLoader   loader(registry);

    // Register default importers (texture, mesh, audio, shader, script, material).
    loader.register_default_importers();
    EXPECT_GE(loader.importer_count(), 6u);

    // Verify that importers handle expected extensions.
    EXPECT_NE(loader.find_importer(".png"), nullptr);
    EXPECT_NE(loader.find_importer(".obj"), nullptr);
    EXPECT_NE(loader.find_importer(".wav"), nullptr);

    // Register some assets in the registry.
    AssetId tex_id = registry.register_asset("textures/player.png",
                                              "/data/textures/player.png",
                                              AssetType::Texture);
    AssetId mesh_id = registry.register_asset("meshes/cube.obj",
                                               "/data/meshes/cube.obj",
                                               AssetType::Mesh);
    EXPECT_TRUE(tex_id.valid());
    EXPECT_TRUE(mesh_id.valid());
    EXPECT_EQ(registry.count(), 2u);

    // Verify metadata look-up by path.
    const AssetMeta* tex_meta = registry.find_by_path("textures/player.png");
    ASSERT_NE(tex_meta, nullptr);
    EXPECT_EQ(tex_meta->type, AssetType::Texture);

    // Queue async loads and check progress before processing.
    loader.load_async(tex_id, /*priority=*/10);
    loader.load_async(mesh_id, /*priority=*/5);

    ProgressInfo info = loader.progress();
    EXPECT_EQ(info.total, 2u);
    EXPECT_EQ(info.completed, 0u);

    // Process the queue (files do not exist on disk, so loads will fail,
    // but the pipeline must not crash and progress counters must advance).
    loader.process_all();

    ProgressInfo after = loader.progress();
    EXPECT_EQ(after.total, 2u);
    EXPECT_EQ(after.completed + after.failed, 2u);
}

// =============================================================================
// Test 4: Animation + ECS Integration
// =============================================================================

TEST(IntegrationPipeline, Animation_ECS_SampleClip) {
    using namespace nexus::anim;

    // Build a simple clip with one bone channel (translation along X).
    AnimationClip clip("walk", 1.0f);

    BoneChannel channel;
    channel.bone_index = 0;
    channel.positions.push_back(PositionKey{0.0f, Vec3{0.0f, 0.0f, 0.0f}});
    channel.positions.push_back(PositionKey{1.0f, Vec3{10.0f, 0.0f, 0.0f}});
    // Rotation stays identity.
    channel.rotations.push_back(RotationKey{0.0f, Quat{1.0f, 0.0f, 0.0f, 0.0f}});
    channel.rotations.push_back(RotationKey{1.0f, Quat{1.0f, 0.0f, 0.0f, 0.0f}});
    // Scale stays 1.
    channel.scales.push_back(ScaleKey{0.0f, Vec3{1.0f}});
    channel.scales.push_back(ScaleKey{1.0f, Vec3{1.0f}});

    clip.add_channel(std::move(channel));

    // Add an animation event.
    clip.add_event(0.5f, "footstep");
    EXPECT_EQ(clip.events().size(), 1u);

    // Sample at t=0.
    std::vector<BonePose> poses(1);
    clip.sample(0.0f, poses);
    EXPECT_NEAR(poses[0].position.x, 0.0f, 0.01f);

    // Sample at t=0.5 (expect linear interpolation -> x = 5).
    clip.sample(0.5f, poses);
    EXPECT_NEAR(poses[0].position.x, 5.0f, 0.01f);

    // Sample at t=1.0.
    clip.sample(1.0f, poses);
    EXPECT_NEAR(poses[0].position.x, 10.0f, 0.01f);

    // Collect events between t=0.0 and t=0.6 (should fire "footstep").
    std::vector<const AnimationEvent*> fired;
    clip.collect_events(0.0f, 0.6f, false, fired);
    ASSERT_EQ(fired.size(), 1u);
    EXPECT_EQ(fired[0]->name, "footstep");
}

// =============================================================================
// Test 5: Physics + ECS Integration
// =============================================================================

TEST(IntegrationPipeline, Physics_ECS_StepSimulation) {
    using namespace nexus::physics;

    Registry registry;
    PhysicsSystem physics;

    // Create a dynamic entity with a 2D rigid body and collider.
    Entity ball = registry.create();
    registry.add_component<Transform2DComponent>(ball, Transform2DComponent{
        {0.0f, 100.0f}, 0.0f, {1.0f, 1.0f}
    });
    registry.add_component<RigidBody2DComponent>(ball, RigidBody2DComponent{
        RigidBody2DComponent::Dynamic, 1.0f, 0.3f, 0.0f
    });
    registry.add_component<Collider2DComponent>(ball, Collider2DComponent{});

    // Create a static ground entity.
    Entity ground = registry.create();
    registry.add_component<Transform2DComponent>(ground, Transform2DComponent{
        {0.0f, 0.0f}, 0.0f, {100.0f, 1.0f}
    });
    registry.add_component<RigidBody2DComponent>(ground, RigidBody2DComponent{
        RigidBody2DComponent::Static
    });
    registry.add_component<Collider2DComponent>(ground, Collider2DComponent{
        Collider2DComponent::Box, {0.0f, 0.0f}, {50.0f, 0.5f}
    });

    // Record initial position.
    float initial_y = registry.get_component<Transform2DComponent>(ball).position.y;

    // Step several frames with default gravity.
    for (int i = 0; i < 60; ++i) {
        physics.update(registry, 1.0f / 60.0f);
    }

    // The ball should have fallen due to gravity.
    float final_y = registry.get_component<Transform2DComponent>(ball).position.y;
    EXPECT_LT(final_y, initial_y);
}

// =============================================================================
// Test 6: Network Serialization Round-trip
// =============================================================================

TEST(IntegrationPipeline, Network_Serialization_RoundTrip) {
    using namespace nexus::net;

    // -- BitWriter / BitReader round-trip -------------------------------------
    BitWriter writer;
    writer.write_bool(true);
    writer.write_u8(42);
    writer.write_u16(1234);
    writer.write_u32(0xDEADBEEF);
    writer.write_i32(-999);
    writer.write_f32(3.14f);
    writer.write_string("Hello NexusEngine");
    writer.flush();

    BitReader reader(writer.data());
    EXPECT_EQ(reader.read_bool(), true);
    EXPECT_EQ(reader.read_u8(), 42u);
    EXPECT_EQ(reader.read_u16(), 1234u);
    EXPECT_EQ(reader.read_u32(), 0xDEADBEEFu);
    EXPECT_EQ(reader.read_i32(), -999);
    EXPECT_FLOAT_EQ(reader.read_f32(), 3.14f);
    EXPECT_EQ(reader.read_string(), "Hello NexusEngine");
    EXPECT_FALSE(reader.has_error());

    // -- RPC Registry round-trip ---------------------------------------------
    RPCRegistry rpc_reg;

    bool handler_called = false;
    u32  received_sender = 0;
    std::vector<u8> received_data;

    rpc_reg.register_rpc("damage", RPCTarget::Server,
        [&](u32 sender_id, const RPCArgs& args) {
            handler_called = true;
            received_sender = sender_id;
            received_data = args.data;
        });

    EXPECT_EQ(rpc_reg.count(), 1u);
    EXPECT_NE(rpc_reg.find("damage"), nullptr);

    // Serialize some RPC argument data.
    BitWriter arg_writer;
    arg_writer.write_u32(50);  // damage amount
    arg_writer.write_u32(7);   // target entity id
    arg_writer.flush();

    RPCArgs rpc_args(arg_writer.data());

    // Build an incoming RPC call.
    RPCCall call;
    call.rpc_id = RPCRegistry::hash_name("damage");
    call.sender_id = 3;
    call.target = RPCTarget::Server;
    call.args = rpc_args;

    rpc_reg.push_incoming(std::move(call));
    u32 processed = rpc_reg.process_incoming();

    EXPECT_EQ(processed, 1u);
    EXPECT_TRUE(handler_called);
    EXPECT_EQ(received_sender, 3u);

    // Read back the argument data to verify integrity.
    BitReader arg_reader(received_data);
    EXPECT_EQ(arg_reader.read_u32(), 50u);
    EXPECT_EQ(arg_reader.read_u32(), 7u);
    EXPECT_FALSE(arg_reader.has_error());
}

// =============================================================================
// Test 7: Full Init-Simulate-Shutdown Cycle
// =============================================================================

TEST(IntegrationPipeline, FullCycle_Init_Simulate_Shutdown) {
    using namespace nexus::scripting;
    using namespace nexus::assets;

    // -- Phase 1: Initialization ----------------------------------------------
    Scene scene;
    ScriptEngine script_engine;
    AssetRegistry asset_registry;
    AssetLoader   asset_loader(asset_registry);
    asset_loader.register_default_importers();

    // Register a scripting binding that creates entities in the scene.
    script_engine.register_function("Scene", "spawn",
        [&scene](const std::vector<ScriptValue>& args) -> ScriptValue {
            std::string name = args.empty() ? "Entity" : args[0].as_string();
            Entity e = scene.create_entity(name);
            return ScriptValue::entity(static_cast<u32>(e));
        },
        0, 1, "Spawn an entity in the scene");

    // -- Phase 2: Populate the scene (simulates loading) ----------------------
    ScriptValue e1 = script_engine.call_function("Scene.spawn",
        {ScriptValue("PlayerShip")});
    ScriptValue e2 = script_engine.call_function("Scene.spawn",
        {ScriptValue("Asteroid")});
    ASSERT_TRUE(e1.is_entity());
    ASSERT_TRUE(e2.is_entity());

    Entity player = static_cast<Entity>(e1.as_entity());
    scene.registry().add_component<Transform2DComponent>(player, Transform2DComponent{
        {0.0f, 0.0f}, 0.0f, {1.0f, 1.0f}
    });

    // Register an asset (no real file needed).
    asset_registry.register_asset("sprites/ship.png",
                                   "assets/sprites/ship.png",
                                   AssetType::Texture);
    EXPECT_EQ(asset_registry.count(), 1u);

    EXPECT_EQ(scene.registry().size(), 2u);

    // -- Phase 3: Simulate a few frames ---------------------------------------
    for (int frame = 0; frame < 10; ++frame) {
        float dt = 1.0f / 60.0f;

        // Move player every frame (mimics a system).
        auto& t2d = scene.registry().get_component<Transform2DComponent>(player);
        t2d.position.x += 1.0f * dt;

        // Tick coroutines.
        script_engine.update_coroutines(dt);

        // Tick the scene (system scheduler, transforms, etc.).
        scene.update(dt);
    }

    // Verify the player moved.
    auto& final_pos = scene.registry().get_component<Transform2DComponent>(player).position;
    EXPECT_GT(final_pos.x, 0.0f);

    // -- Phase 4: Snapshot / Restore (editor play-mode pattern) ---------------
    bool snap_ok = scene.take_snapshot();
    ASSERT_TRUE(snap_ok);
    ASSERT_TRUE(scene.has_snapshot());

    // Mutate the scene (simulate play-mode changes).
    scene.create_entity("Bullet");
    EXPECT_EQ(scene.registry().size(), 3u);

    // Restore from snapshot.
    bool restore_ok = scene.restore_snapshot();
    ASSERT_TRUE(restore_ok);
    EXPECT_EQ(scene.registry().size(), 2u);

    // -- Phase 5: Tear down ---------------------------------------------------
    scene.clear();
    EXPECT_EQ(scene.registry().size(), 0u);

    asset_registry.clear();
    EXPECT_EQ(asset_registry.count(), 0u);

    // Scripting engine destructs with no leaks (ASAN/LSAN will catch).
}

// =============================================================================
// Test 8: DeltaCompressor Network Integrity
// =============================================================================

TEST(IntegrationPipeline, Network_DeltaCompression_RoundTrip) {
    using namespace nexus::net;

    // Baseline state.
    std::vector<u8> baseline = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    // Current state with a few bytes changed.
    std::vector<u8> current  = {0, 1, 99, 3, 4, 5, 6, 77, 8, 9};

    std::vector<u8> delta = DeltaCompressor::compress(baseline, current);
    EXPECT_FALSE(delta.empty());

    std::vector<u8> reconstructed = DeltaCompressor::decompress(baseline, delta);
    ASSERT_EQ(reconstructed.size(), current.size());
    EXPECT_EQ(reconstructed, current);

    // For small payloads, delta overhead may exceed savings.
    // Just verify the ratio API returns a positive value.
    f32 ratio = DeltaCompressor::ratio(current, delta);
    EXPECT_GT(ratio, 0.0f);
}
