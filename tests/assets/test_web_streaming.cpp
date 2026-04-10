#include <gtest/gtest.h>
#include "nexus/assets/web_asset_streaming.h"
#include "nexus/assets/asset_package.h"

using namespace nexus::assets;

// ── StreamingManifest Tests ────────────────────────────────────────────────

TEST(WebStreaming, ManifestAddAndFind) {
    StreamingManifest manifest;
    manifest.set_base_url("https://cdn.example.com/assets");

    StreamingManifestEntry entry;
    entry.id = AssetId(100);
    entry.type = AssetType::Texture;
    entry.virtual_path = "textures/player.png";
    entry.chunk_file = "chunk_0.bin";
    entry.offset_in_chunk = 0;
    entry.size = 4096;
    entry.priority = 10;

    manifest.add_entry(entry);

    EXPECT_EQ(manifest.entry_count(), 1u);
    EXPECT_EQ(manifest.base_url(), "https://cdn.example.com/assets");

    const auto* found_by_id = manifest.find_entry(AssetId(100));
    ASSERT_NE(found_by_id, nullptr);
    EXPECT_EQ(found_by_id->virtual_path, "textures/player.png");
    EXPECT_EQ(found_by_id->size, 4096u);

    const auto* found_by_path = manifest.find_entry("textures/player.png");
    ASSERT_NE(found_by_path, nullptr);
    EXPECT_EQ(found_by_path->id, AssetId(100));

    EXPECT_EQ(manifest.find_entry(AssetId(999)), nullptr);
    EXPECT_EQ(manifest.find_entry("nonexistent.png"), nullptr);
}

TEST(WebStreaming, ManifestTotalSize) {
    StreamingManifest manifest;

    StreamingManifestEntry e1;
    e1.id = AssetId(1);
    e1.size = 1000;
    e1.chunk_file = "chunk_0.bin";
    manifest.add_entry(e1);

    StreamingManifestEntry e2;
    e2.id = AssetId(2);
    e2.size = 2000;
    e2.chunk_file = "chunk_0.bin";
    manifest.add_entry(e2);

    StreamingManifestEntry e3;
    e3.id = AssetId(3);
    e3.size = 3000;
    e3.chunk_file = "chunk_1.bin";
    manifest.add_entry(e3);

    EXPECT_EQ(manifest.total_size(), 6000u);

    auto chunks = manifest.chunk_files();
    EXPECT_EQ(chunks.size(), 2u);
}

TEST(WebStreaming, ManifestJsonRoundTrip) {
    StreamingManifest original;
    original.set_base_url("https://cdn.example.com");

    StreamingManifestEntry entry;
    entry.id = AssetId(42);
    entry.type = AssetType::Mesh;
    entry.virtual_path = "meshes/cube.obj";
    entry.chunk_file = "chunk_0.bin";
    entry.offset_in_chunk = 512;
    entry.size = 8192;
    entry.priority = 5;
    entry.dependencies.push_back(AssetId(10));
    entry.dependencies.push_back(AssetId(20));
    original.add_entry(entry);

    std::string json = original.save_to_json();
    ASSERT_FALSE(json.empty());

    StreamingManifest loaded;
    ASSERT_TRUE(loaded.load_from_json(json));

    EXPECT_EQ(loaded.base_url(), "https://cdn.example.com");
    EXPECT_EQ(loaded.entry_count(), 1u);

    const auto* e = loaded.find_entry(AssetId(42));
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->virtual_path, "meshes/cube.obj");
    EXPECT_EQ(e->chunk_file, "chunk_0.bin");
    EXPECT_EQ(e->offset_in_chunk, 512u);
    EXPECT_EQ(e->size, 8192u);
    EXPECT_EQ(e->priority, 5u);
    ASSERT_EQ(e->dependencies.size(), 2u);
    EXPECT_EQ(e->dependencies[0], AssetId(10));
    EXPECT_EQ(e->dependencies[1], AssetId(20));
}

// ── WebAssetStreamer Tests ─────────────────────────────────────────────────

TEST(WebStreaming, StreamerInitialize) {
    StreamingManifest manifest;
    manifest.set_base_url("/local");

    StreamingManifestEntry entry;
    entry.id = AssetId(1);
    entry.virtual_path = "test.txt";
    entry.chunk_file = "chunk_0.bin";
    entry.size = 100;
    manifest.add_entry(entry);

    WebAssetStreamer streamer;
    ASSERT_TRUE(streamer.initialize(manifest));
    EXPECT_FALSE(streamer.is_active());
    EXPECT_FALSE(streamer.is_asset_ready(AssetId(1)));
}

TEST(WebStreaming, StreamerRequestUnknownAsset) {
    StreamingManifest manifest;
    WebAssetStreamer streamer;
    streamer.initialize(manifest);

    // Requesting a non-existent asset should not crash.
    streamer.request_asset(AssetId(999));
    EXPECT_FALSE(streamer.is_asset_ready(AssetId(999)));
}

TEST(WebStreaming, StreamerProgress) {
    StreamingManifest manifest;

    StreamingManifestEntry e1;
    e1.id = AssetId(1);
    e1.virtual_path = "a.txt";
    e1.chunk_file = "chunk_0.bin";
    e1.size = 100;
    manifest.add_entry(e1);

    StreamingManifestEntry e2;
    e2.id = AssetId(2);
    e2.virtual_path = "b.txt";
    e2.chunk_file = "chunk_1.bin";
    e2.size = 200;
    manifest.add_entry(e2);

    WebAssetStreamer streamer;
    streamer.initialize(manifest);

    auto prog = streamer.progress();
    EXPECT_EQ(prog.assets_total, 2u);
    EXPECT_EQ(prog.assets_ready, 0u);
}

TEST(WebStreaming, CacheManagement) {
    StreamingManifest manifest;
    WebAssetStreamer streamer;
    streamer.initialize(manifest);

    EXPECT_EQ(streamer.cache_size(), 0u);
    streamer.clear_cache();
    EXPECT_EQ(streamer.cache_size(), 0u);

    streamer.set_cache_limit(1024 * 1024);
    EXPECT_EQ(streamer.max_concurrent(), 4u);

    streamer.set_max_concurrent(2);
    EXPECT_EQ(streamer.max_concurrent(), 2u);
}

TEST(WebStreaming, GenerateManifestFromPackage) {
    AssetPackage pkg;
    // We can't create a real package without file I/O, but we can test
    // that generate_manifest handles an empty package.
    auto manifest = WebAssetStreamer::generate_manifest(pkg, "https://cdn.example.com", 1024 * 1024);

    EXPECT_EQ(manifest.base_url(), "https://cdn.example.com");
    EXPECT_EQ(manifest.entry_count(), 0u);
}

TEST(WebStreaming, StreamerReadyCallback) {
    StreamingManifest manifest;

    StreamingManifestEntry entry;
    entry.id = AssetId(1);
    entry.virtual_path = "test.txt";
    entry.chunk_file = "chunk_0.bin";
    entry.size = 100;
    manifest.add_entry(entry);

    WebAssetStreamer streamer;
    streamer.initialize(manifest);

    bool callback_fired = false;
    streamer.set_ready_callback([&](AssetId id, const std::string& path) {
        callback_fired = true;
    });

    // Without a local directory, the download will fail, but the callback
    // mechanism itself should be testable.
    streamer.request_asset(AssetId(1));
    streamer.update();

    // File doesn't exist, so asset won't be ready. Callback shouldn't fire.
    EXPECT_FALSE(callback_fired);
    EXPECT_FALSE(streamer.is_asset_ready(AssetId(1)));
}

TEST(WebStreaming, ManifestJsonInvalid) {
    StreamingManifest manifest;
    EXPECT_FALSE(manifest.load_from_json("not valid json{{{"));
    EXPECT_FALSE(manifest.load_from_json("{}"));
}

TEST(WebStreaming, AssetDataByPath) {
    StreamingManifest manifest;

    StreamingManifestEntry entry;
    entry.id = AssetId(1);
    entry.virtual_path = "test.txt";
    entry.chunk_file = "chunk_0.bin";
    entry.size = 100;
    manifest.add_entry(entry);

    WebAssetStreamer streamer;
    streamer.initialize(manifest);

    // Not ready yet - should return empty.
    auto data = streamer.get_asset_data("test.txt");
    EXPECT_TRUE(data.empty());

    EXPECT_FALSE(streamer.is_asset_ready("test.txt"));
    EXPECT_FALSE(streamer.is_asset_ready("nonexistent.txt"));
}
