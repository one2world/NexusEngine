#include <gtest/gtest.h>
#include "nexus/assets/asset_registry.h"
#include <filesystem>
#include <fstream>

using namespace nexus;
using namespace nexus::assets;

// Helper to create a temp file
static std::string create_temp_file(const std::string& name, const std::string& content) {
    auto dir = std::filesystem::temp_directory_path() / "nexus_test_assets";
    std::filesystem::create_directories(dir);
    auto path = dir / name;
    std::ofstream f(path);
    f << content;
    f.close();
    return path.string();
}

static void cleanup_temp_dir() {
    auto dir = std::filesystem::temp_directory_path() / "nexus_test_assets";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// =============================================================================
// Registration
// =============================================================================

TEST(AssetRegistry, RegisterAsset) {
    AssetRegistry reg;
    auto id = reg.register_asset("textures/player.png", "/fake/path.png", AssetType::Texture);
    EXPECT_TRUE(id.valid());
    EXPECT_EQ(reg.count(), 1u);
}

TEST(AssetRegistry, RegisterMultiple) {
    AssetRegistry reg;
    reg.register_asset("a.png", "", AssetType::Texture);
    reg.register_asset("b.obj", "", AssetType::Mesh);
    reg.register_asset("c.wav", "", AssetType::Audio);
    EXPECT_EQ(reg.count(), 3u);
}

TEST(AssetRegistry, RegisterDuplicateUpdates) {
    AssetRegistry reg;
    auto id1 = reg.register_asset("test.png", "/old/path.png", AssetType::Texture);
    auto id2 = reg.register_asset("test.png", "/new/path.png", AssetType::Texture);
    EXPECT_EQ(id1, id2);
    EXPECT_EQ(reg.count(), 1u);

    auto* meta = reg.find(id1);
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->source_path, "/new/path.png");
}

TEST(AssetRegistry, Unregister) {
    AssetRegistry reg;
    auto id = reg.register_asset("test.png", "", AssetType::Texture);
    EXPECT_EQ(reg.count(), 1u);
    EXPECT_TRUE(reg.unregister_asset(id));
    EXPECT_EQ(reg.count(), 0u);
    EXPECT_FALSE(reg.unregister_asset(id)); // already removed
}

TEST(AssetRegistry, AutoDetectType) {
    AssetRegistry reg;
    auto file = create_temp_file("detect.png", "fake");
    auto id = reg.register_asset("detect.png", file);
    auto* meta = reg.find(id);
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->type, AssetType::Texture);
    cleanup_temp_dir();
}

// =============================================================================
// Find
// =============================================================================

TEST(AssetRegistry, FindById) {
    AssetRegistry reg;
    auto id = reg.register_asset("test.png", "", AssetType::Texture);
    auto* meta = reg.find(id);
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->path, "test.png");
}

TEST(AssetRegistry, FindByPath) {
    AssetRegistry reg;
    reg.register_asset("textures/player.png", "", AssetType::Texture);
    auto* meta = reg.find_by_path("textures/player.png");
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->type, AssetType::Texture);
}

TEST(AssetRegistry, FindMissing) {
    AssetRegistry reg;
    EXPECT_EQ(reg.find(AssetId(999)), nullptr);
    EXPECT_EQ(reg.find_by_path("nonexistent"), nullptr);
}

// =============================================================================
// Data storage
// =============================================================================

TEST(AssetRegistry, StoreAndGetData) {
    AssetRegistry reg;
    auto id = reg.register_asset("test.png", "", AssetType::Texture);

    auto tex = std::make_shared<TextureData>();
    tex->width = 128;
    tex->height = 64;
    reg.store_data(id, tex);

    auto data = reg.get_data(id);
    ASSERT_NE(data, nullptr);
    auto* loaded = dynamic_cast<TextureData*>(data.get());
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->width, 128u);

    auto* meta = reg.find(id);
    EXPECT_EQ(meta->status, AssetStatus::Loaded);
}

TEST(AssetRegistry, GetTypedHandle) {
    AssetRegistry reg;
    auto id = reg.register_asset("test.png", "", AssetType::Texture);

    auto tex = std::make_shared<TextureData>();
    tex->width = 256;
    reg.store_data(id, tex);

    auto handle = reg.get_handle<TextureData>(id);
    EXPECT_TRUE(handle.valid());
    EXPECT_EQ(handle->width, 256u);
}

TEST(AssetRegistry, GetHandleByPath) {
    AssetRegistry reg;
    reg.register_asset("test.png", "", AssetType::Texture);
    auto tex = std::make_shared<TextureData>();
    tex->width = 512;
    reg.store_data(AssetId("test.png"), tex);

    auto handle = reg.get_handle<TextureData>("test.png");
    EXPECT_TRUE(handle.valid());
    EXPECT_EQ(handle->width, 512u);
}

TEST(AssetRegistry, GetHandleWrongType) {
    AssetRegistry reg;
    auto id = reg.register_asset("test.png", "", AssetType::Texture);
    auto tex = std::make_shared<TextureData>();
    reg.store_data(id, tex);

    auto handle = reg.get_handle<MeshData>(id);
    EXPECT_FALSE(handle.valid());
}

// =============================================================================
// Queries
// =============================================================================

TEST(AssetRegistry, AllAssets) {
    AssetRegistry reg;
    reg.register_asset("a.png", "", AssetType::Texture);
    reg.register_asset("b.obj", "", AssetType::Mesh);
    EXPECT_EQ(reg.all_assets().size(), 2u);
}

TEST(AssetRegistry, AssetsOfType) {
    AssetRegistry reg;
    reg.register_asset("a.png", "", AssetType::Texture);
    reg.register_asset("b.png", "", AssetType::Texture);
    reg.register_asset("c.obj", "", AssetType::Mesh);

    EXPECT_EQ(reg.assets_of_type(AssetType::Texture).size(), 2u);
    EXPECT_EQ(reg.assets_of_type(AssetType::Mesh).size(), 1u);
    EXPECT_EQ(reg.assets_of_type(AssetType::Audio).size(), 0u);
}

TEST(AssetRegistry, TotalMemoryUsage) {
    AssetRegistry reg;
    auto id = reg.register_asset("test.png", "", AssetType::Texture);
    auto tex = std::make_shared<TextureData>();
    tex->pixels.resize(4096);
    reg.store_data(id, tex);
    EXPECT_EQ(reg.total_memory_usage(), 4096u);
}

// =============================================================================
// Garbage Collection
// =============================================================================

TEST(AssetRegistry, GarbageCollect) {
    AssetRegistry reg;
    auto id = reg.register_asset("test.png", "", AssetType::Texture);
    auto tex = std::make_shared<TextureData>();
    reg.store_data(id, tex);

    // No references → should collect
    u32 collected = reg.garbage_collect();
    EXPECT_EQ(collected, 1u);

    auto* meta = reg.find(id);
    EXPECT_EQ(meta->status, AssetStatus::Unloaded);
    EXPECT_EQ(reg.get_data(id), nullptr);
}

TEST(AssetRegistry, GarbageCollectWithRef) {
    AssetRegistry reg;
    auto id = reg.register_asset("test.png", "", AssetType::Texture);
    auto tex = std::make_shared<TextureData>();
    reg.store_data(id, tex);

    // Create a handle to keep a reference
    auto handle = reg.get_handle<TextureData>(id);
    EXPECT_TRUE(handle.valid());

    u32 collected = reg.garbage_collect();
    EXPECT_EQ(collected, 0u); // Should not collect — has reference
}

// =============================================================================
// Events
// =============================================================================

TEST(AssetRegistry, EventCallback) {
    AssetRegistry reg;
    std::vector<AssetRegistry::Event> events;

    reg.set_event_callback([&](AssetRegistry::Event ev, AssetId) {
        events.push_back(ev);
    });

    auto id = reg.register_asset("test.png", "", AssetType::Texture);
    auto tex = std::make_shared<TextureData>();
    reg.store_data(id, tex);

    EXPECT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0], AssetRegistry::Event::Registered);
    EXPECT_EQ(events[1], AssetRegistry::Event::Loaded);
}

TEST(AssetRegistry, Clear) {
    AssetRegistry reg;
    reg.register_asset("a.png", "", AssetType::Texture);
    reg.register_asset("b.obj", "", AssetType::Mesh);
    EXPECT_EQ(reg.count(), 2u);
    reg.clear();
    EXPECT_EQ(reg.count(), 0u);
}
