#include <gtest/gtest.h>
#include "nexus/assets/asset_loader.h"
#include <filesystem>
#include <fstream>

using namespace nexus;
using namespace nexus::assets;

class AssetLoaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "nexus_loader_test";
        std::filesystem::create_directories(test_dir_);
        loader_ = std::make_unique<AssetLoader>(registry_);
        loader_->register_default_importers();
    }

    void TearDown() override {
        loader_.reset();
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }

    std::string create_file(const std::string& name, const std::string& content) {
        auto path = test_dir_ / name;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream f(path);
        f << content;
        f.close();
        return path.string();
    }

    std::string create_binary_file(const std::string& name, const std::vector<u8>& content) {
        auto path = test_dir_ / name;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream f(path, std::ios::binary);
        f.write(reinterpret_cast<const char*>(content.data()),
                static_cast<std::streamsize>(content.size()));
        f.close();
        return path.string();
    }

    // Create a minimal valid 2x2 24-bit BMP file
    static std::vector<u8> make_minimal_bmp() {
        // BMP header (14 bytes) + DIB header (40 bytes) + pixel data
        // 2x2 pixels, 24bpp, row stride = (2*3 + 3) & ~3 = 8
        std::vector<u8> bmp(14 + 40 + 8 * 2, 0);
        // BMP signature
        bmp[0] = 'B'; bmp[1] = 'M';
        // File size
        u32 fsize = static_cast<u32>(bmp.size());
        bmp[2] = static_cast<u8>(fsize); bmp[3] = static_cast<u8>(fsize >> 8);
        // Pixel data offset
        bmp[10] = 54;
        // DIB header size
        bmp[14] = 40;
        // Width = 2
        bmp[18] = 2;
        // Height = 2
        bmp[22] = 2;
        // Planes = 1
        bmp[26] = 1;
        // BPP = 24
        bmp[28] = 24;
        // Fill pixels with some color
        for (size_t i = 54; i < bmp.size(); ++i) bmp[i] = 128;
        return bmp;
    }

    // Create a minimal valid WAV file
    static std::vector<u8> make_minimal_wav() {
        // RIFF header + fmt chunk + data chunk
        // 44 bytes header + 4 bytes of PCM data
        std::vector<u8> wav(48, 0);
        // "RIFF"
        wav[0]='R'; wav[1]='I'; wav[2]='F'; wav[3]='F';
        // Chunk size = file_size - 8 = 40
        wav[4] = 40;
        // "WAVE"
        wav[8]='W'; wav[9]='A'; wav[10]='V'; wav[11]='E';
        // "fmt "
        wav[12]='f'; wav[13]='m'; wav[14]='t'; wav[15]=' ';
        // fmt chunk size = 16
        wav[16] = 16;
        // Audio format = 1 (PCM)
        wav[20] = 1;
        // Channels = 1
        wav[22] = 1;
        // Sample rate = 44100 (0xAC44)
        wav[24] = 0x44; wav[25] = 0xAC;
        // Byte rate = 44100 * 1 * 2 = 88200
        wav[28] = 0xA8; wav[29] = 0x58; wav[30] = 0x01;
        // Block align = 2
        wav[32] = 2;
        // Bits per sample = 16
        wav[34] = 16;
        // "data"
        wav[36]='d'; wav[37]='a'; wav[38]='t'; wav[39]='a';
        // Data chunk size = 4
        wav[40] = 4;
        // 4 bytes of PCM data
        wav[44] = 0; wav[45] = 0; wav[46] = 127; wav[47] = 0;
        return wav;
    }

    std::filesystem::path test_dir_;
    AssetRegistry registry_;
    std::unique_ptr<AssetLoader> loader_;
};

// =============================================================================
// Importers
// =============================================================================

TEST_F(AssetLoaderTest, DefaultImporterCount) {
    EXPECT_EQ(loader_->importer_count(), 6u); // texture, mesh, audio, shader, script, material
}

TEST_F(AssetLoaderTest, FindImporterByExtension) {
    EXPECT_NE(loader_->find_importer(".png"), nullptr);
    EXPECT_NE(loader_->find_importer(".obj"), nullptr);
    EXPECT_NE(loader_->find_importer(".wav"), nullptr);
    EXPECT_NE(loader_->find_importer(".glsl"), nullptr);
    EXPECT_NE(loader_->find_importer(".lua"), nullptr);
    EXPECT_NE(loader_->find_importer(".mat"), nullptr);
    EXPECT_EQ(loader_->find_importer(".xyz"), nullptr);
}

TEST_F(AssetLoaderTest, FindImporterByType) {
    EXPECT_NE(loader_->find_importer_for_type(AssetType::Texture), nullptr);
    EXPECT_NE(loader_->find_importer_for_type(AssetType::Mesh), nullptr);
    EXPECT_NE(loader_->find_importer_for_type(AssetType::Audio), nullptr);
    EXPECT_NE(loader_->find_importer_for_type(AssetType::Shader), nullptr);
    EXPECT_NE(loader_->find_importer_for_type(AssetType::Script), nullptr);
    EXPECT_NE(loader_->find_importer_for_type(AssetType::Material), nullptr);
    EXPECT_EQ(loader_->find_importer_for_type(AssetType::Font), nullptr);
}

TEST_F(AssetLoaderTest, ImporterSupportsExtension) {
    TextureImporter imp;
    EXPECT_TRUE(imp.supports(".png"));
    EXPECT_TRUE(imp.supports(".PNG"));
    EXPECT_TRUE(imp.supports(".jpg"));
    EXPECT_FALSE(imp.supports(".obj"));
}

// =============================================================================
// Sync loading
// =============================================================================

TEST_F(AssetLoaderTest, LoadTextureSync) {
    auto bmp_data = make_minimal_bmp();
    auto file = create_binary_file("test.bmp", bmp_data);
    auto id = registry_.register_asset("test.bmp", file, AssetType::Texture);

    EXPECT_TRUE(loader_->load_sync(id));

    auto handle = registry_.get_handle<TextureData>("test.bmp");
    EXPECT_TRUE(handle.valid());
    EXPECT_FALSE(handle->pixels.empty());
}

TEST_F(AssetLoaderTest, LoadMeshSync) {
    auto file = create_file("model.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3");
    auto id = registry_.register_asset("model.obj", file, AssetType::Mesh);

    EXPECT_TRUE(loader_->load_sync(id));

    auto handle = registry_.get_handle<MeshData>("model.obj");
    EXPECT_TRUE(handle.valid());
    EXPECT_FALSE(handle->vertices.empty());
}

TEST_F(AssetLoaderTest, LoadAudioSync) {
    auto wav_data = make_minimal_wav();
    auto file = create_binary_file("sound.wav", wav_data);
    auto id = registry_.register_asset("sound.wav", file, AssetType::Audio);

    EXPECT_TRUE(loader_->load_sync(id));

    auto handle = registry_.get_handle<AudioData>("sound.wav");
    EXPECT_TRUE(handle.valid());
}

TEST_F(AssetLoaderTest, LoadShaderSync) {
    auto file = create_file("test.vert", "void main() { gl_Position = vec4(0); }");
    auto id = registry_.register_asset("test.vert", file, AssetType::Shader);

    EXPECT_TRUE(loader_->load_sync(id));

    auto handle = registry_.get_handle<ShaderData>("test.vert");
    EXPECT_TRUE(handle.valid());
    EXPECT_FALSE(handle->vertex_source.empty());
    EXPECT_TRUE(handle->fragment_source.empty()); // Only .vert
}

TEST_F(AssetLoaderTest, LoadScriptSync) {
    auto file = create_file("player.lua", "function on_update(dt) end");
    auto id = registry_.register_asset("player.lua", file, AssetType::Script);

    EXPECT_TRUE(loader_->load_sync(id));

    auto handle = registry_.get_handle<ScriptData>("player.lua");
    EXPECT_TRUE(handle.valid());
    EXPECT_EQ(handle->source, "function on_update(dt) end");
}

TEST_F(AssetLoaderTest, LoadMaterialSync) {
    auto file = create_file("metal.mat", "shader = pbr\nmetallic = 0.9\nroughness = 0.1\n");
    auto id = registry_.register_asset("metal.mat", file, AssetType::Material);

    EXPECT_TRUE(loader_->load_sync(id));

    auto handle = registry_.get_handle<MaterialData>("metal.mat");
    EXPECT_TRUE(handle.valid());
    EXPECT_EQ(handle->shader_path, "pbr");
    EXPECT_NEAR(handle->metallic, 0.9f, 0.01f);
    EXPECT_NEAR(handle->roughness, 0.1f, 0.01f);
}

TEST_F(AssetLoaderTest, LoadByPath) {
    auto bmp_data = make_minimal_bmp();
    auto file = create_binary_file("test.bmp", bmp_data);
    registry_.register_asset("test.bmp", file, AssetType::Texture);

    EXPECT_TRUE(loader_->load_sync("test.bmp"));
}

TEST_F(AssetLoaderTest, LoadNonexistent) {
    auto id = registry_.register_asset("missing.png", "/nonexistent/path.png", AssetType::Texture);
    EXPECT_FALSE(loader_->load_sync(id));

    auto* meta = registry_.find(id);
    EXPECT_EQ(meta->status, AssetStatus::Failed);
}

TEST_F(AssetLoaderTest, LoadUnknownId) {
    EXPECT_FALSE(loader_->load_sync(AssetId(99999)));
}

TEST_F(AssetLoaderTest, AlreadyLoadedSkips) {
    auto bmp_data = make_minimal_bmp();
    auto file = create_binary_file("test.bmp", bmp_data);
    auto id = registry_.register_asset("test.bmp", file, AssetType::Texture);

    EXPECT_TRUE(loader_->load_sync(id));
    EXPECT_TRUE(loader_->load_sync(id)); // Should succeed immediately
}

// =============================================================================
// Async loading
// =============================================================================

TEST_F(AssetLoaderTest, AsyncLoadSingle) {
    auto bmp_data = make_minimal_bmp();
    auto file = create_binary_file("async.bmp", bmp_data);
    auto id = registry_.register_asset("async.bmp", file, AssetType::Texture);

    loader_->load_async(id, 0);
    EXPECT_EQ(loader_->progress().total, 1u);
    EXPECT_EQ(loader_->progress().completed, 0u);

    EXPECT_TRUE(loader_->process_one());
    EXPECT_EQ(loader_->progress().completed, 1u);
}

TEST_F(AssetLoaderTest, AsyncLoadPriority) {
    auto bmp = make_minimal_bmp();
    auto low_file = create_binary_file("low.bmp", bmp);
    auto high_file = create_binary_file("high.bmp", bmp);

    auto low_id = registry_.register_asset("low.bmp", low_file, AssetType::Texture);
    auto high_id = registry_.register_asset("high.bmp", high_file, AssetType::Texture);

    loader_->load_async(low_id, 1);
    loader_->load_async(high_id, 10);

    // Higher priority should be processed first
    loader_->process_one();
    EXPECT_EQ(loader_->progress().current_asset, high_id);
}

TEST_F(AssetLoaderTest, ProcessAll) {
    auto bmp = make_minimal_bmp();
    for (int i = 0; i < 5; i++) {
        auto name = "file" + std::to_string(i) + ".bmp";
        auto file = create_binary_file(name, bmp);
        auto id = registry_.register_asset(name, file, AssetType::Texture);
        loader_->load_async(id);
    }

    u32 processed = loader_->process_all();
    EXPECT_EQ(processed, 5u);
    EXPECT_EQ(loader_->progress().completed, 5u);
}

TEST_F(AssetLoaderTest, ProgressCallback) {
    auto bmp = make_minimal_bmp();
    auto file = create_binary_file("cb.bmp", bmp);
    auto id = registry_.register_asset("cb.bmp", file, AssetType::Texture);

    u32 callback_count = 0;
    loader_->set_progress_callback([&](const ProgressInfo& info) {
        callback_count++;
        EXPECT_GT(info.fraction(), 0.0f);
    });

    loader_->load_async(id);
    loader_->process_all();
    EXPECT_EQ(callback_count, 1u);
}

TEST_F(AssetLoaderTest, ProcessOneEmptyQueue) {
    EXPECT_FALSE(loader_->process_one());
}

// =============================================================================
// Custom importer
// =============================================================================

class FakeImporter : public AssetImporter {
public:
    AssetType handled_type() const override { return AssetType::Font; }
    std::vector<std::string> supported_extensions() const override { return {".fake"}; }
    std::shared_ptr<AssetData> import(const std::string&,
                                       const AssetMeta&) override {
        auto data = std::make_shared<ScriptData>();
        data->source = "custom_imported";
        return data;
    }
};

TEST_F(AssetLoaderTest, CustomImporter) {
    loader_->register_importer(std::make_unique<FakeImporter>());
    EXPECT_EQ(loader_->importer_count(), 7u);
    EXPECT_NE(loader_->find_importer(".fake"), nullptr);
}
