#include <gtest/gtest.h>

#include "nexus/animation/animator_system.h"
#include "nexus/animation/animation_clip.h"
#include "nexus/scene/scene.h"
#include "nexus/scene/components.h"

#include <memory>

namespace nexus::anim::tests {

namespace {

// Builds a clip whose bone-0 channel slides position.x from 0 → 10 over 1s,
// rotates around Y from identity to 90°, and scales from 1 to 2.  Lets the
// system tests verify a real sample is being applied to Transform3D.
std::unique_ptr<AnimationClip> make_slide_clip(f32 duration = 1.0f) {
    auto clip = std::make_unique<AnimationClip>("slide", duration);
    BoneChannel ch;
    ch.bone_index = 0;
    ch.positions.push_back({0.0f,      Vec3(0.0f, 0.0f, 0.0f)});
    ch.positions.push_back({duration,  Vec3(10.0f, 0.0f, 0.0f)});
    ch.rotations.push_back({0.0f,      Quat(1.0f, 0.0f, 0.0f, 0.0f)});
    ch.rotations.push_back({duration,
        glm::angleAxis(glm::radians(90.0f), Vec3(0.0f, 1.0f, 0.0f))});
    ch.scales.push_back({0.0f,        Vec3(1.0f)});
    ch.scales.push_back({duration,    Vec3(2.0f, 1.0f, 1.0f)});
    clip->add_channel(std::move(ch));
    return clip;
}

}  // namespace

// ── No-op cases ─────────────────────────────────────────────────────────────

TEST(AnimatorSystem, NotPlayingDoesNotAdvanceTransform) {
    auto clip = make_slide_clip();
    AnimatorSystem sys([&](u32) { return clip.get(); });

    Scene scene;
    Entity e = scene.create_entity_3d("E");
    auto& reg = scene.registry();
    reg.add_component<AnimatorComponent>(e, AnimatorComponent{});
    auto& a = reg.get_component<AnimatorComponent>(e);
    a.clip_id = 1;
    a.playing = false;  // gate

    EXPECT_EQ(sys.tick(reg, 0.5f), 0u);
    EXPECT_NEAR(reg.get_component<Transform3DComponent>(e).position.x, 0.0f, 1e-5f);
}

TEST(AnimatorSystem, NoClipIdNoOps) {
    auto clip = make_slide_clip();
    AnimatorSystem sys([&](u32) { return clip.get(); });

    Scene scene;
    Entity e = scene.create_entity_3d("E");
    auto& reg = scene.registry();
    reg.add_component<AnimatorComponent>(e, AnimatorComponent{});
    auto& a = reg.get_component<AnimatorComponent>(e);
    a.clip_id = 0;  // unbound
    a.playing = true;

    EXPECT_EQ(sys.tick(reg, 0.5f), 0u);
    EXPECT_NEAR(reg.get_component<Transform3DComponent>(e).position.x, 0.0f, 1e-5f);
}

TEST(AnimatorSystem, ResolverReturningNullptrIsSafe) {
    AnimatorSystem sys([](u32) { return nullptr; });

    Scene scene;
    Entity e = scene.create_entity_3d("E");
    auto& reg = scene.registry();
    reg.add_component<AnimatorComponent>(e, AnimatorComponent{});
    auto& a = reg.get_component<AnimatorComponent>(e);
    a.clip_id = 99;
    a.playing = true;

    EXPECT_EQ(sys.tick(reg, 0.5f), 0u);
    EXPECT_NEAR(reg.get_component<Transform3DComponent>(e).position.x, 0.0f, 1e-5f);
}

TEST(AnimatorSystem, NoResolverIsSafeNoOp) {
    AnimatorSystem sys;  // no resolver bound

    Scene scene;
    Entity e = scene.create_entity_3d("E");
    auto& reg = scene.registry();
    reg.add_component<AnimatorComponent>(e, AnimatorComponent{});
    auto& a = reg.get_component<AnimatorComponent>(e);
    a.clip_id = 1;
    a.playing = true;

    EXPECT_FALSE(sys.has_resolver());
    EXPECT_EQ(sys.tick(reg, 0.5f), 0u);
}

// ── Active playback ─────────────────────────────────────────────────────────

TEST(AnimatorSystem, PlayingAdvancesPositionLinearly) {
    auto clip = make_slide_clip(1.0f);
    AnimatorSystem sys([&](u32) { return clip.get(); });

    Scene scene;
    Entity e = scene.create_entity_3d("E");
    auto& reg = scene.registry();
    reg.add_component<AnimatorComponent>(e, AnimatorComponent{});
    auto& a = reg.get_component<AnimatorComponent>(e);
    a.clip_id = 1;
    a.playing = true;
    a.looping = true;

    EXPECT_EQ(sys.tick(reg, 0.5f), 1u);
    EXPECT_NEAR(reg.get_component<Transform3DComponent>(e).position.x, 5.0f, 1e-3f);
    EXPECT_NEAR(a.time, 0.5f, 1e-5f);

    EXPECT_EQ(sys.tick(reg, 0.25f), 1u);
    EXPECT_NEAR(reg.get_component<Transform3DComponent>(e).position.x, 7.5f, 1e-3f);
    EXPECT_NEAR(a.time, 0.75f, 1e-5f);
}

TEST(AnimatorSystem, SpeedScalesAdvanceRate) {
    auto clip = make_slide_clip(1.0f);
    AnimatorSystem sys([&](u32) { return clip.get(); });

    Scene scene;
    Entity e = scene.create_entity_3d("E");
    auto& reg = scene.registry();
    reg.add_component<AnimatorComponent>(e, AnimatorComponent{});
    auto& a = reg.get_component<AnimatorComponent>(e);
    a.clip_id = 1;
    a.playing = true;
    a.looping = true;
    a.speed   = 2.0f;

    sys.tick(reg, 0.25f);  // 0.25s real → 0.5s clip
    EXPECT_NEAR(a.time, 0.5f, 1e-5f);
}

TEST(AnimatorSystem, LoopingWrapsModuloDuration) {
    auto clip = make_slide_clip(1.0f);
    AnimatorSystem sys([&](u32) { return clip.get(); });

    Scene scene;
    Entity e = scene.create_entity_3d("E");
    auto& reg = scene.registry();
    reg.add_component<AnimatorComponent>(e, AnimatorComponent{});
    auto& a = reg.get_component<AnimatorComponent>(e);
    a.clip_id = 1;
    a.playing = true;
    a.looping = true;
    a.time    = 0.9f;

    sys.tick(reg, 0.5f);  // 0.9 + 0.5 = 1.4 → wraps to 0.4
    EXPECT_NEAR(a.time, 0.4f, 1e-5f);
    EXPECT_TRUE(a.playing);
}

TEST(AnimatorSystem, NonLoopingClampsAndStops) {
    auto clip = make_slide_clip(1.0f);
    AnimatorSystem sys([&](u32) { return clip.get(); });

    Scene scene;
    Entity e = scene.create_entity_3d("E");
    auto& reg = scene.registry();
    reg.add_component<AnimatorComponent>(e, AnimatorComponent{});
    auto& a = reg.get_component<AnimatorComponent>(e);
    a.clip_id = 1;
    a.playing = true;
    a.looping = false;
    a.time    = 0.9f;

    sys.tick(reg, 0.5f);  // overshoots end
    EXPECT_NEAR(a.time, 1.0f, 1e-5f);
    EXPECT_FALSE(a.playing);  // clamped + stopped
}

TEST(AnimatorSystem, NegativeSpeedRewindsAndWraps) {
    auto clip = make_slide_clip(1.0f);
    AnimatorSystem sys([&](u32) { return clip.get(); });

    Scene scene;
    Entity e = scene.create_entity_3d("E");
    auto& reg = scene.registry();
    reg.add_component<AnimatorComponent>(e, AnimatorComponent{});
    auto& a = reg.get_component<AnimatorComponent>(e);
    a.clip_id = 1;
    a.playing = true;
    a.looping = true;
    a.speed   = -1.0f;
    a.time    = 0.2f;

    sys.tick(reg, 0.5f);  // 0.2 - 0.5 = -0.3 → wraps to 0.7
    EXPECT_NEAR(a.time, 0.7f, 1e-5f);
    EXPECT_TRUE(a.playing);
}

// ── Multi-entity coverage ───────────────────────────────────────────────────

TEST(AnimatorSystem, TickReturnsCountOfEntitiesAdvanced) {
    auto clip = make_slide_clip(1.0f);
    AnimatorSystem sys([&](u32) { return clip.get(); });

    Scene scene;
    auto& reg = scene.registry();

    Entity playing1 = scene.create_entity_3d("Playing1");
    reg.add_component<AnimatorComponent>(playing1, AnimatorComponent{});
    auto& p1 = reg.get_component<AnimatorComponent>(playing1);
    p1.clip_id = 1; p1.playing = true; p1.looping = true;

    Entity stopped = scene.create_entity_3d("Stopped");
    reg.add_component<AnimatorComponent>(stopped, AnimatorComponent{});
    auto& st = reg.get_component<AnimatorComponent>(stopped);
    st.clip_id = 1; st.playing = false;

    Entity playing2 = scene.create_entity_3d("Playing2");
    reg.add_component<AnimatorComponent>(playing2, AnimatorComponent{});
    auto& p2 = reg.get_component<AnimatorComponent>(playing2);
    p2.clip_id = 1; p2.playing = true; p2.looping = true;

    EXPECT_EQ(sys.tick(reg, 0.1f), 2u);
}

// ── start_autoplay ──────────────────────────────────────────────────────────

TEST(AnimatorSystem, StartAutoplayFlipsPlayOnStartFlag) {
    Scene scene;
    auto& reg = scene.registry();

    Entity a = scene.create_entity_3d("A");
    reg.add_component<AnimatorComponent>(a, AnimatorComponent{});
    {
        auto& aa = reg.get_component<AnimatorComponent>(a);
        aa.play_on_start = true;
        aa.playing = false;
        aa.time    = 0.4f;
    }

    Entity b = scene.create_entity_3d("B");
    reg.add_component<AnimatorComponent>(b, AnimatorComponent{});
    {
        auto& bb = reg.get_component<AnimatorComponent>(b);
        bb.play_on_start = false;  // opt out
        bb.playing = false;
    }

    Entity c = scene.create_entity_3d("C");
    reg.add_component<AnimatorComponent>(c, AnimatorComponent{});
    {
        auto& cc = reg.get_component<AnimatorComponent>(c);
        cc.play_on_start = true;
        cc.playing = true;  // already playing — leave alone
    }

    const u32 started = AnimatorSystem::start_autoplay(reg);
    EXPECT_EQ(started, 1u);  // only A flipped on

    // Re-fetch references after start_autoplay since interior mutation
    // doesn't grow the pool.  These accesses are safe (no add_component
    // intervened).
    EXPECT_TRUE(reg.get_component<AnimatorComponent>(a).playing);
    EXPECT_NEAR(reg.get_component<AnimatorComponent>(a).time, 0.0f, 1e-5f);
    EXPECT_FALSE(reg.get_component<AnimatorComponent>(b).playing);
    EXPECT_TRUE(reg.get_component<AnimatorComponent>(c).playing);
}

// ── Sampled rotation reaches expected target at endpoint ────────────────────

// =============================================================================
// SkeletonComponent multi-bone path (M20)
// =============================================================================
//
// AnimatorSystem::tick must, when SkeletonComponent is present, route each
// bone-i pose into the matching bone entity's Transform3D — not into the
// owning entity's Transform3D.  These tests verify the routing, the
// graceful fallbacks (out-of-range pose, dead bone, missing transform),
// and the single-transform path's continued correctness when there is no
// skeleton bound.

namespace {

// Build a clip with N bone channels, each animating bone-i's position to
// a unique target so the test can verify which channel hit which entity.
std::unique_ptr<AnimationClip> make_per_bone_clip(u32 bone_count) {
    auto clip = std::make_unique<AnimationClip>("rig", 1.0f);
    for (u32 i = 0; i < bone_count; ++i) {
        BoneChannel ch;
        ch.bone_index = static_cast<i32>(i);
        ch.positions.push_back({0.0f, Vec3(0.0f)});
        ch.positions.push_back({1.0f, Vec3(static_cast<f32>(i + 1) * 10.0f,
                                            0.0f, 0.0f)});
        clip->add_channel(std::move(ch));
    }
    return clip;
}

}  // namespace

TEST(AnimatorSystem, SkeletonRoutesEachBoneToItsOwnEntity) {
    auto clip = make_per_bone_clip(3);
    AnimatorSystem sys([&](u32) { return clip.get(); });

    Scene scene;
    auto& reg = scene.registry();

    Entity rig = scene.create_entity_3d("Rig");
    Entity bone0 = scene.create_entity_3d("Bone0");
    Entity bone1 = scene.create_entity_3d("Bone1");
    Entity bone2 = scene.create_entity_3d("Bone2");

    SkeletonComponent skel;
    skel.bone_entities = {static_cast<u32>(bone0),
                          static_cast<u32>(bone1),
                          static_cast<u32>(bone2)};
    reg.add_component<SkeletonComponent>(rig, std::move(skel));

    AnimatorComponent ac;
    ac.clip_id = 1;
    ac.playing = true;
    ac.looping = false;
    ac.time    = 0.0f;
    reg.add_component<AnimatorComponent>(rig, std::move(ac));

    // Tick to the end so each bone's position channel hits its target.
    sys.tick(reg, 1.0f);

    EXPECT_NEAR(reg.get_component<Transform3DComponent>(bone0).position.x, 10.0f, 1e-3f);
    EXPECT_NEAR(reg.get_component<Transform3DComponent>(bone1).position.x, 20.0f, 1e-3f);
    EXPECT_NEAR(reg.get_component<Transform3DComponent>(bone2).position.x, 30.0f, 1e-3f);

    // The rig entity itself must NOT inherit bone-0's pose when a
    // skeleton routes the channel away.  Its transform stays default.
    EXPECT_NEAR(reg.get_component<Transform3DComponent>(rig).position.x, 0.0f, 1e-3f);
}

TEST(AnimatorSystem, SkeletonHandlesFewerEntitiesThanChannels) {
    // 3-channel clip but only 2 bone entities — the third channel is
    // silently dropped.  No crash, no out-of-bounds write.
    auto clip = make_per_bone_clip(3);
    AnimatorSystem sys([&](u32) { return clip.get(); });

    Scene scene;
    auto& reg = scene.registry();
    Entity rig = scene.create_entity_3d("Rig");
    Entity b0 = scene.create_entity_3d("B0");
    Entity b1 = scene.create_entity_3d("B1");
    SkeletonComponent skel;
    skel.bone_entities = {static_cast<u32>(b0), static_cast<u32>(b1)};
    reg.add_component<SkeletonComponent>(rig, std::move(skel));

    AnimatorComponent ac;
    ac.clip_id = 1; ac.playing = true; ac.looping = false; ac.time = 0.0f;
    reg.add_component<AnimatorComponent>(rig, std::move(ac));

    sys.tick(reg, 1.0f);
    EXPECT_NEAR(reg.get_component<Transform3DComponent>(b0).position.x, 10.0f, 1e-3f);
    EXPECT_NEAR(reg.get_component<Transform3DComponent>(b1).position.x, 20.0f, 1e-3f);
}

TEST(AnimatorSystem, SkeletonHandlesMoreEntitiesThanChannels) {
    // 1-channel clip, 3 bone entities — only bone 0 receives a pose; the
    // others stay at their default Transform3D values.
    auto clip = make_per_bone_clip(1);
    AnimatorSystem sys([&](u32) { return clip.get(); });

    Scene scene;
    auto& reg = scene.registry();
    Entity rig = scene.create_entity_3d("Rig");
    Entity b0 = scene.create_entity_3d("B0");
    Entity b1 = scene.create_entity_3d("B1");
    Entity b2 = scene.create_entity_3d("B2");
    SkeletonComponent skel;
    skel.bone_entities = {static_cast<u32>(b0), static_cast<u32>(b1),
                          static_cast<u32>(b2)};
    reg.add_component<SkeletonComponent>(rig, std::move(skel));

    AnimatorComponent ac;
    ac.clip_id = 1; ac.playing = true; ac.looping = false; ac.time = 0.0f;
    reg.add_component<AnimatorComponent>(rig, std::move(ac));

    sys.tick(reg, 1.0f);
    EXPECT_NEAR(reg.get_component<Transform3DComponent>(b0).position.x, 10.0f, 1e-3f);
    EXPECT_NEAR(reg.get_component<Transform3DComponent>(b1).position.x, 0.0f,  1e-3f);
    EXPECT_NEAR(reg.get_component<Transform3DComponent>(b2).position.x, 0.0f,  1e-3f);
}

TEST(AnimatorSystem, SkeletonSkipsDeadBoneEntities) {
    auto clip = make_per_bone_clip(3);
    AnimatorSystem sys([&](u32) { return clip.get(); });

    Scene scene;
    auto& reg = scene.registry();
    Entity rig = scene.create_entity_3d("Rig");
    Entity b0 = scene.create_entity_3d("B0");
    Entity b1 = scene.create_entity_3d("B1");
    Entity b2 = scene.create_entity_3d("B2");
    SkeletonComponent skel;
    skel.bone_entities = {static_cast<u32>(b0), static_cast<u32>(b1),
                          static_cast<u32>(b2)};
    reg.add_component<SkeletonComponent>(rig, std::move(skel));

    AnimatorComponent ac;
    ac.clip_id = 1; ac.playing = true; ac.looping = false; ac.time = 0.0f;
    reg.add_component<AnimatorComponent>(rig, std::move(ac));

    // Kill bone1 between bind and tick — system must skip it without
    // crashing and continue applying poses to the surviving bones.
    reg.destroy(b1);
    sys.tick(reg, 1.0f);
    EXPECT_NEAR(reg.get_component<Transform3DComponent>(b0).position.x, 10.0f, 1e-3f);
    EXPECT_NEAR(reg.get_component<Transform3DComponent>(b2).position.x, 30.0f, 1e-3f);
}

TEST(AnimatorSystem, EmptySkeletonFallsBackToSingleTransformPath) {
    // SkeletonComponent present but bone_entities empty → behave as if
    // no skeleton: bone 0's pose lands on the entity's own Transform3D.
    auto clip = make_per_bone_clip(1);
    AnimatorSystem sys([&](u32) { return clip.get(); });

    Scene scene;
    auto& reg = scene.registry();
    Entity rig = scene.create_entity_3d("Rig");
    reg.add_component<SkeletonComponent>(rig, SkeletonComponent{});  // empty

    AnimatorComponent ac;
    ac.clip_id = 1; ac.playing = true; ac.looping = false; ac.time = 0.0f;
    reg.add_component<AnimatorComponent>(rig, std::move(ac));

    sys.tick(reg, 1.0f);
    EXPECT_NEAR(reg.get_component<Transform3DComponent>(rig).position.x, 10.0f, 1e-3f);
}

TEST(AnimatorSystem, RotationAtEndpointMatchesAuthoredYaw) {
    auto clip = make_slide_clip(1.0f);
    AnimatorSystem sys([&](u32) { return clip.get(); });

    Scene scene;
    Entity e = scene.create_entity_3d("E");
    auto& reg = scene.registry();
    reg.add_component<AnimatorComponent>(e, AnimatorComponent{});
    auto& a = reg.get_component<AnimatorComponent>(e);
    a.clip_id = 1; a.playing = true; a.looping = false;

    sys.tick(reg, 1.0f);  // arrive at end
    EXPECT_FALSE(a.playing);
    const auto& tc = reg.get_component<Transform3DComponent>(e);
    // 90° around Y → quaternion ≈ (w=√½, x=0, y=√½, z=0).
    EXPECT_NEAR(tc.rotation.w, std::sqrt(0.5f), 1e-3f);
    EXPECT_NEAR(tc.rotation.y, std::sqrt(0.5f), 1e-3f);
    EXPECT_NEAR(tc.scale.x,    2.0f,            1e-3f);
}

}  // namespace nexus::anim::tests
