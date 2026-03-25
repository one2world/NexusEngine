#include <gtest/gtest.h>
#include <nexus/core/resource_manager.h>
#include <string>

namespace nexus::tests {

struct TestTexture {
    std::string name;
    int width, height;
};

struct TestMesh {
    std::string name;
    int vertex_count;
};

TEST(ResourceManager, StoreAndRetrieve) {
    ResourceManager rm;
    auto tex = std::make_shared<TestTexture>(TestTexture{"diffuse", 512, 512});
    rm.store<TestTexture>("diffuse", tex);

    auto result = rm.get<TestTexture>("diffuse");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->name, "diffuse");
    EXPECT_EQ(result->width, 512);
}

TEST(ResourceManager, GetNonexistentReturnsNull) {
    ResourceManager rm;
    EXPECT_EQ(rm.get<TestTexture>("missing"), nullptr);
}

TEST(ResourceManager, HasResource) {
    ResourceManager rm;
    rm.store<TestTexture>("tex", std::make_shared<TestTexture>());
    EXPECT_TRUE(rm.has<TestTexture>("tex"));
    EXPECT_FALSE(rm.has<TestTexture>("other"));
}

TEST(ResourceManager, RemoveResource) {
    ResourceManager rm;
    rm.store<TestTexture>("tex", std::make_shared<TestTexture>());
    rm.remove<TestTexture>("tex");
    EXPECT_FALSE(rm.has<TestTexture>("tex"));
}

TEST(ResourceManager, DifferentTypesIndependent) {
    ResourceManager rm;
    rm.store<TestTexture>("asset", std::make_shared<TestTexture>(TestTexture{"tex", 256, 256}));
    rm.store<TestMesh>("asset", std::make_shared<TestMesh>(TestMesh{"mesh", 100}));

    auto tex = rm.get<TestTexture>("asset");
    auto mesh = rm.get<TestMesh>("asset");
    ASSERT_NE(tex, nullptr);
    ASSERT_NE(mesh, nullptr);
    EXPECT_EQ(tex->name, "tex");
    EXPECT_EQ(mesh->name, "mesh");
}

TEST(ResourceManager, GetOrLoad) {
    ResourceManager rm;
    int load_count = 0;

    auto result = rm.get_or_load<TestTexture>("lazy", [&]() {
        load_count++;
        return std::make_shared<TestTexture>(TestTexture{"lazy", 64, 64});
    });
    EXPECT_EQ(load_count, 1);
    EXPECT_NE(result, nullptr);

    // Second call should use cached version
    auto result2 = rm.get_or_load<TestTexture>("lazy", [&]() {
        load_count++;
        return std::make_shared<TestTexture>();
    });
    EXPECT_EQ(load_count, 1); // not called again
    EXPECT_EQ(result2->name, "lazy");
}

TEST(ResourceManager, ClearAll) {
    ResourceManager rm;
    rm.store<TestTexture>("a", std::make_shared<TestTexture>());
    rm.store<TestMesh>("b", std::make_shared<TestMesh>());
    rm.clear_all();
    EXPECT_FALSE(rm.has<TestTexture>("a"));
    EXPECT_FALSE(rm.has<TestMesh>("b"));
}

TEST(ResourceManager, Count) {
    ResourceManager rm;
    rm.store<TestTexture>("a", std::make_shared<TestTexture>());
    rm.store<TestTexture>("b", std::make_shared<TestTexture>());
    EXPECT_EQ(rm.count<TestTexture>(), 2u);
}

} // namespace nexus::tests
