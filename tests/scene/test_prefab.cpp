#include <gtest/gtest.h>
#include <nexus/scene/prefab.h>
#include <nexus/scene/components.h>

namespace nexus::tests {

TEST(Prefab, CreateFromEntity) {
    Scene scene;
    Entity e = scene.create_entity_3d("TestPrefab");
    scene.registry().add_component<MeshRendererComponent>(e, MeshRendererComponent{42, 7});

    Prefab p = Prefab::from_entity(scene, e, "MyPrefab");
    EXPECT_TRUE(p.valid());
    EXPECT_EQ(p.name(), "MyPrefab");
    EXPECT_FALSE(p.data().empty());
}

TEST(Prefab, InstantiateCreatesEntity) {
    Scene src_scene;
    Entity e = src_scene.create_entity_3d("Soldier");
    src_scene.registry().add_component<PointLightComponent>(e, PointLightComponent{});

    Prefab p = Prefab::from_entity(src_scene, e, "SoldierPrefab");

    Scene dst_scene;
    Entity inst = p.instantiate(dst_scene);
    EXPECT_NE(inst, INVALID_ENTITY);
}

TEST(Prefab, InvalidPrefabFails) {
    Prefab p;
    EXPECT_FALSE(p.valid());

    Scene scene;
    Entity e = p.instantiate(scene);
    EXPECT_EQ(e, INVALID_ENTITY);
}

TEST(PrefabLibrary, AddGetRemove) {
    PrefabLibrary lib;
    Prefab p("test", R"({"entities":[]})");

    lib.add(p);
    EXPECT_TRUE(lib.has("test"));
    EXPECT_NE(lib.get("test"), nullptr);

    lib.remove("test");
    EXPECT_FALSE(lib.has("test"));
}

TEST(PrefabLibrary, GetNonexistent) {
    PrefabLibrary lib;
    EXPECT_EQ(lib.get("missing"), nullptr);
}

} // namespace nexus::tests
