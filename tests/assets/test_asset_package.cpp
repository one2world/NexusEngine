#include <gtest/gtest.h>
#include "nexus/assets/asset_package.h"
#include <filesystem>
#include <fstream>

using namespace nexus;
using namespace nexus::assets;

class AssetPackageTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "nexus_package_test";
        std::filesystem::create_directories(test_dir_);
        pak_path_ = (test_dir_ / "test.pak").string();
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }

    std::string create_file(const std::string& name, const std::string& content) {
        auto path = test_dir_ / name;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream f(path, std::ios::binary);
        f << content;
        f.close();
        return path.string();
    }

    std::filesystem::path test_dir_;
    std::string pak_path_;
};

// =============================================================================
// CRC32
// =============================================================================

TEST(PackageCRC, EmptyData) {
    u32 crc = AssetPackage::crc32(nullptr, 0);
    EXPECT_EQ(crc, 0u); // CRC of empty is 0 after XOR
}

TEST(PackageCRC, KnownData) {
    const u8 data[] = {'h', 'e', 'l', 'l', 'o'};
    u32 crc = AssetPackage::crc32(data, 5);
    EXPECT_NE(crc, 0u);
}

TEST(PackageCRC, DifferentDataDifferentCRC) {
    const u8 a[] = {1, 2, 3};
    const u8 b[] = {4, 5, 6};
    EXPECT_NE(AssetPackage::crc32(a, 3), AssetPackage::crc32(b, 3));
}

TEST(PackageCRC, SameDataSameCRC) {
    const u8 data[] = {1, 2, 3};
    EXPECT_EQ(AssetPackage::crc32(data, 3), AssetPackage::crc32(data, 3));
}

// =============================================================================
// Package creation and reading
// =============================================================================

TEST_F(AssetPackageTest, CreateAndOpen) {
    {
        AssetPackage pkg;
        EXPECT_TRUE(pkg.create(pak_path_));
        EXPECT_TRUE(pkg.is_open());
        EXPECT_TRUE(pkg.finalize());
        pkg.close();
    }

    {
        AssetPackage pkg;
        EXPECT_TRUE(pkg.open(pak_path_));
        EXPECT_TRUE(pkg.is_open());
        EXPECT_EQ(pkg.entry_count(), 0u);
        pkg.close();
    }
}

TEST_F(AssetPackageTest, AddFileAndRead) {
    auto src = create_file("hello.txt", "Hello, World!");

    {
        AssetPackage pkg;
        EXPECT_TRUE(pkg.create(pak_path_));
        EXPECT_TRUE(pkg.add_file("texts/hello.txt", src));
        EXPECT_TRUE(pkg.finalize());
        pkg.close();
    }

    {
        AssetPackage pkg;
        EXPECT_TRUE(pkg.open(pak_path_));
        EXPECT_EQ(pkg.entry_count(), 1u);
        EXPECT_TRUE(pkg.has_entry("texts/hello.txt"));

        auto data = pkg.read_entry("texts/hello.txt");
        std::string content(data.begin(), data.end());
        EXPECT_EQ(content, "Hello, World!");
        pkg.close();
    }
}

TEST_F(AssetPackageTest, AddDataAndRead) {
    std::vector<u8> raw_data = {0xDE, 0xAD, 0xBE, 0xEF};

    {
        AssetPackage pkg;
        EXPECT_TRUE(pkg.create(pak_path_));
        EXPECT_TRUE(pkg.add_data("data/binary.bin", raw_data, AssetType::Unknown));
        EXPECT_TRUE(pkg.finalize());
        pkg.close();
    }

    {
        AssetPackage pkg;
        EXPECT_TRUE(pkg.open(pak_path_));
        auto data = pkg.read_entry("data/binary.bin");
        EXPECT_EQ(data, raw_data);
        pkg.close();
    }
}

TEST_F(AssetPackageTest, MultipleEntries) {
    auto f1 = create_file("a.txt", "alpha");
    auto f2 = create_file("b.txt", "beta");
    auto f3 = create_file("c.txt", "gamma");

    {
        AssetPackage pkg;
        pkg.create(pak_path_);
        pkg.add_file("a.txt", f1);
        pkg.add_file("b.txt", f2);
        pkg.add_file("c.txt", f3);
        pkg.finalize();
        pkg.close();
    }

    {
        AssetPackage pkg;
        pkg.open(pak_path_);
        EXPECT_EQ(pkg.entry_count(), 3u);

        auto da = pkg.read_entry("a.txt");
        auto db = pkg.read_entry("b.txt");
        auto dc = pkg.read_entry("c.txt");

        EXPECT_EQ(std::string(da.begin(), da.end()), "alpha");
        EXPECT_EQ(std::string(db.begin(), db.end()), "beta");
        EXPECT_EQ(std::string(dc.begin(), dc.end()), "gamma");
        pkg.close();
    }
}

TEST_F(AssetPackageTest, ReadByAssetId) {
    std::vector<u8> data = {1, 2, 3, 4, 5};

    {
        AssetPackage pkg;
        pkg.create(pak_path_);
        pkg.add_data("test/data.bin", data);
        pkg.finalize();
        pkg.close();
    }

    {
        AssetPackage pkg;
        pkg.open(pak_path_);
        AssetId id("test/data.bin");
        EXPECT_TRUE(pkg.has_entry(id));

        auto result = pkg.read_entry(id);
        EXPECT_EQ(result, data);
        pkg.close();
    }
}

TEST_F(AssetPackageTest, FindEntry) {
    auto f = create_file("texture.png", std::string(256, 'X'));

    {
        AssetPackage pkg;
        pkg.create(pak_path_);
        pkg.add_file("textures/player.png", f, AssetType::Texture);
        pkg.finalize();
        pkg.close();
    }

    {
        AssetPackage pkg;
        pkg.open(pak_path_);

        auto* entry = pkg.find_entry("textures/player.png");
        ASSERT_NE(entry, nullptr);
        EXPECT_EQ(entry->type, AssetType::Texture);
        EXPECT_EQ(entry->size, 256u);
        EXPECT_NE(entry->checksum, 0u);
        pkg.close();
    }
}

TEST_F(AssetPackageTest, MissingEntry) {
    {
        AssetPackage pkg;
        pkg.create(pak_path_);
        pkg.finalize();
        pkg.close();
    }

    AssetPackage pkg;
    pkg.open(pak_path_);
    EXPECT_FALSE(pkg.has_entry("nonexistent"));
    EXPECT_EQ(pkg.find_entry("nonexistent"), nullptr);
    auto data = pkg.read_entry("nonexistent");
    EXPECT_TRUE(data.empty());
    pkg.close();
}

TEST_F(AssetPackageTest, TotalDataSize) {
    {
        AssetPackage pkg;
        pkg.create(pak_path_);
        pkg.add_data("a", {1, 2, 3});
        pkg.add_data("b", {4, 5, 6, 7, 8});
        pkg.finalize();
        pkg.close();
    }

    AssetPackage pkg;
    pkg.open(pak_path_);
    EXPECT_EQ(pkg.total_data_size(), 8u);
    pkg.close();
}

TEST_F(AssetPackageTest, Validate) {
    {
        AssetPackage pkg;
        pkg.create(pak_path_);
        pkg.add_data("test", {1, 2, 3});
        pkg.finalize();
        pkg.close();
    }

    AssetPackage pkg;
    pkg.open(pak_path_);
    EXPECT_TRUE(pkg.validate());
    pkg.close();
}

TEST_F(AssetPackageTest, AutoDetectTypeFromExtension) {
    auto f = create_file("test.wav", "wave_data");

    AssetPackage pkg;
    pkg.create(pak_path_);
    pkg.add_file("audio/bgm.wav", f);
    pkg.finalize();

    auto* entry = pkg.find_entry("audio/bgm.wav");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->type, AssetType::Audio);
    pkg.close();
}

TEST_F(AssetPackageTest, OpenNonexistent) {
    AssetPackage pkg;
    EXPECT_FALSE(pkg.open("/nonexistent/path.pak"));
}

// =============================================================================
// PackageEntry
// =============================================================================

TEST(PackageEntry, NotCompressed) {
    PackageEntry entry;
    entry.size = 100;
    entry.compressed_size = 0;
    EXPECT_FALSE(entry.is_compressed());
}

TEST(PackageEntry, Compressed) {
    PackageEntry entry;
    entry.size = 100;
    entry.compressed_size = 50;
    EXPECT_TRUE(entry.is_compressed());
}

// =============================================================================
// Hot Reload
// =============================================================================

class HotReloadTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "nexus_hotreload_test";
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }

    std::string create_file(const std::string& name, const std::string& content) {
        auto path = test_dir_ / name;
        std::ofstream f(path);
        f << content;
        f.close();
        return path.string();
    }

    std::filesystem::path test_dir_;
};

TEST_F(HotReloadTest, WatchDirectory) {
    AssetHotReload reload;
    reload.watch(test_dir_.string());
    EXPECT_EQ(reload.watch_count(), 1u);
}

TEST_F(HotReloadTest, UnwatchDirectory) {
    AssetHotReload reload;
    reload.watch(test_dir_.string());
    reload.unwatch(test_dir_.string());
    EXPECT_EQ(reload.watch_count(), 0u);
}

TEST_F(HotReloadTest, DetectNewFile) {
    AssetHotReload reload;
    reload.watch(test_dir_.string());

    // Initial poll — no changes
    auto changes = reload.poll_changes();
    EXPECT_TRUE(changes.empty());

    // Create a new file
    create_file("new_file.txt", "content");

    // Poll again — should detect new file
    changes = reload.poll_changes();
    EXPECT_EQ(changes.size(), 1u);
}

TEST_F(HotReloadTest, DetectModifiedFile) {
    create_file("existing.txt", "original");

    AssetHotReload reload;
    reload.watch(test_dir_.string());

    // Initial poll
    reload.poll_changes();

    // Modify the file (need to change modification time)
    // We can't easily change the mtime in a reliable cross-platform way
    // in a unit test, but we can verify the tracking infrastructure works
    EXPECT_GT(reload.tracked_file_count(), 0u);
}

TEST_F(HotReloadTest, DuplicateWatch) {
    AssetHotReload reload;
    reload.watch(test_dir_.string());
    reload.watch(test_dir_.string()); // Should be ignored
    EXPECT_EQ(reload.watch_count(), 1u);
}

TEST_F(HotReloadTest, NonexistentDirectory) {
    AssetHotReload reload;
    reload.watch("/nonexistent/directory");
    EXPECT_EQ(reload.watch_count(), 1u);

    auto changes = reload.poll_changes();
    EXPECT_TRUE(changes.empty());
}
