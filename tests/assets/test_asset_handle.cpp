#include <gtest/gtest.h>
#include "nexus/assets/asset_handle.h"

using namespace nexus;
using namespace nexus::assets;

// =============================================================================
// AssetType
// =============================================================================

TEST(AssetType, Names) {
    EXPECT_STREQ(asset_type_name(AssetType::Texture), "Texture");
    EXPECT_STREQ(asset_type_name(AssetType::Mesh), "Mesh");
    EXPECT_STREQ(asset_type_name(AssetType::Material), "Material");
    EXPECT_STREQ(asset_type_name(AssetType::Shader), "Shader");
    EXPECT_STREQ(asset_type_name(AssetType::Audio), "Audio");
    EXPECT_STREQ(asset_type_name(AssetType::Font), "Font");
    EXPECT_STREQ(asset_type_name(AssetType::Scene), "Scene");
    EXPECT_STREQ(asset_type_name(AssetType::Prefab), "Prefab");
    EXPECT_STREQ(asset_type_name(AssetType::Script), "Script");
    EXPECT_STREQ(asset_type_name(AssetType::Animation), "Animation");
    EXPECT_STREQ(asset_type_name(AssetType::Unknown), "Unknown");
}

TEST(AssetType, FromExtensionTextures) {
    EXPECT_EQ(asset_type_from_extension(".png"), AssetType::Texture);
    EXPECT_EQ(asset_type_from_extension(".jpg"), AssetType::Texture);
    EXPECT_EQ(asset_type_from_extension(".jpeg"), AssetType::Texture);
    EXPECT_EQ(asset_type_from_extension(".tga"), AssetType::Texture);
    EXPECT_EQ(asset_type_from_extension(".bmp"), AssetType::Texture);
    EXPECT_EQ(asset_type_from_extension(".hdr"), AssetType::Texture);
    EXPECT_EQ(asset_type_from_extension(".PNG"), AssetType::Texture);
}

TEST(AssetType, FromExtensionMeshes) {
    EXPECT_EQ(asset_type_from_extension(".gltf"), AssetType::Mesh);
    EXPECT_EQ(asset_type_from_extension(".glb"), AssetType::Mesh);
    EXPECT_EQ(asset_type_from_extension(".obj"), AssetType::Mesh);
    EXPECT_EQ(asset_type_from_extension(".fbx"), AssetType::Mesh);
}

TEST(AssetType, FromExtensionAudio) {
    EXPECT_EQ(asset_type_from_extension(".wav"), AssetType::Audio);
    EXPECT_EQ(asset_type_from_extension(".ogg"), AssetType::Audio);
    EXPECT_EQ(asset_type_from_extension(".mp3"), AssetType::Audio);
    EXPECT_EQ(asset_type_from_extension(".flac"), AssetType::Audio);
}

TEST(AssetType, FromExtensionShaders) {
    EXPECT_EQ(asset_type_from_extension(".glsl"), AssetType::Shader);
    EXPECT_EQ(asset_type_from_extension(".vert"), AssetType::Shader);
    EXPECT_EQ(asset_type_from_extension(".frag"), AssetType::Shader);
    EXPECT_EQ(asset_type_from_extension(".comp"), AssetType::Shader);
}

TEST(AssetType, FromExtensionScripts) {
    EXPECT_EQ(asset_type_from_extension(".lua"), AssetType::Script);
    EXPECT_EQ(asset_type_from_extension(".nxs"), AssetType::Script);
}

TEST(AssetType, FromExtensionOther) {
    EXPECT_EQ(asset_type_from_extension(".mat"), AssetType::Material);
    EXPECT_EQ(asset_type_from_extension(".anim"), AssetType::Animation);
    EXPECT_EQ(asset_type_from_extension(".scene"), AssetType::Scene);
    EXPECT_EQ(asset_type_from_extension(".prefab"), AssetType::Prefab);
    EXPECT_EQ(asset_type_from_extension(".ttf"), AssetType::Font);
    EXPECT_EQ(asset_type_from_extension(".xyz"), AssetType::Unknown);
}

// =============================================================================
// AssetId
// =============================================================================

TEST(AssetId, Default) {
    AssetId id;
    EXPECT_FALSE(id.valid());
    EXPECT_EQ(id.value, 0u);
}

TEST(AssetId, FromPath) {
    AssetId id("textures/player.png");
    EXPECT_TRUE(id.valid());
    EXPECT_NE(id.value, 0u);
}

TEST(AssetId, SamePathSameId) {
    AssetId a("textures/player.png");
    AssetId b("textures/player.png");
    EXPECT_EQ(a, b);
}

TEST(AssetId, DifferentPathDifferentId) {
    AssetId a("textures/player.png");
    AssetId b("textures/enemy.png");
    EXPECT_NE(a, b);
}

TEST(AssetId, Explicit) {
    AssetId id(42ULL);
    EXPECT_TRUE(id.valid());
    EXPECT_EQ(id.value, 42u);
}

// =============================================================================
// AssetData types
// =============================================================================

TEST(TextureData, Type) {
    TextureData tex;
    EXPECT_EQ(tex.type(), AssetType::Texture);
}

TEST(TextureData, MemoryUsage) {
    TextureData tex;
    tex.pixels.resize(1024);
    EXPECT_EQ(tex.memory_usage(), 1024u);
}

TEST(MeshData, Type) {
    MeshData mesh;
    EXPECT_EQ(mesh.type(), AssetType::Mesh);
}

TEST(MeshData, MemoryUsage) {
    MeshData mesh;
    MeshData::Vertex v{};
    mesh.vertices = {v, v, v};
    mesh.indices = {0, 1, 2};
    EXPECT_EQ(mesh.memory_usage(),
              3 * sizeof(MeshData::Vertex) + 3 * sizeof(u32));
}

TEST(AudioData, Type) {
    AudioData audio;
    EXPECT_EQ(audio.type(), AssetType::Audio);
}

TEST(ShaderData, Type) {
    ShaderData shader;
    EXPECT_EQ(shader.type(), AssetType::Shader);
}

TEST(ShaderData, MemoryUsage) {
    ShaderData shader;
    shader.vertex_source = "void main() {}";
    shader.fragment_source = "void main() {}";
    EXPECT_EQ(shader.memory_usage(),
              shader.vertex_source.size() + shader.fragment_source.size());
}

TEST(ScriptData, Type) {
    ScriptData script;
    EXPECT_EQ(script.type(), AssetType::Script);
}

TEST(MaterialData, Type) {
    MaterialData mat;
    EXPECT_EQ(mat.type(), AssetType::Material);
}

TEST(AnimationData, Type) {
    AnimationData anim;
    EXPECT_EQ(anim.type(), AssetType::Animation);
}

TEST(AnimationData, MemoryUsage) {
    AnimationData anim;
    AnimationData::Channel ch;
    ch.keyframes.resize(10);
    anim.channels.push_back(ch);
    EXPECT_GT(anim.memory_usage(), 0u);
}

// =============================================================================
// AssetHandle
// =============================================================================

TEST(AssetHandle, DefaultInvalid) {
    AssetHandle<TextureData> handle;
    EXPECT_FALSE(handle.valid());
    EXPECT_FALSE(static_cast<bool>(handle));
}

TEST(AssetHandle, ValidHandle) {
    AssetMeta meta;
    meta.id = AssetId("test.png");
    auto data = std::make_shared<TextureData>();
    data->width = 64;

    AssetHandle<TextureData> handle(&meta, data);
    EXPECT_TRUE(handle.valid());
    EXPECT_EQ(handle->width, 64u);
    EXPECT_EQ(handle.ref_count(), 1u);
}

TEST(AssetHandle, CopyIncreasesRefCount) {
    AssetMeta meta;
    meta.id = AssetId("test.png");
    auto data = std::make_shared<TextureData>();

    AssetHandle<TextureData> h1(&meta, data);
    EXPECT_EQ(h1.ref_count(), 1u);

    AssetHandle<TextureData> h2 = h1;
    EXPECT_EQ(h1.ref_count(), 2u);
    EXPECT_EQ(h2.ref_count(), 2u);
}

TEST(AssetHandle, MoveDoesNotIncreaseRefCount) {
    AssetMeta meta;
    meta.id = AssetId("test.png");
    auto data = std::make_shared<TextureData>();

    AssetHandle<TextureData> h1(&meta, data);
    EXPECT_EQ(h1.ref_count(), 1u);

    AssetHandle<TextureData> h2 = std::move(h1);
    EXPECT_TRUE(h2.valid());
    EXPECT_FALSE(h1.valid());
    EXPECT_EQ(h2.ref_count(), 1u);
}

TEST(AssetHandle, DestructorDecreasesRefCount) {
    AssetMeta meta;
    meta.id = AssetId("test.png");
    auto data = std::make_shared<TextureData>();

    {
        AssetHandle<TextureData> h1(&meta, data);
        EXPECT_EQ(meta.ref_count.load(), 1u);

        {
            AssetHandle<TextureData> h2 = h1;
            EXPECT_EQ(meta.ref_count.load(), 2u);
        }

        EXPECT_EQ(meta.ref_count.load(), 1u);
    }

    EXPECT_EQ(meta.ref_count.load(), 0u);
}

TEST(AssetHandle, CopyAssignment) {
    AssetMeta meta1, meta2;
    meta1.id = AssetId("a.png");
    meta2.id = AssetId("b.png");
    auto data1 = std::make_shared<TextureData>();
    auto data2 = std::make_shared<TextureData>();
    data1->width = 1;
    data2->width = 2;

    AssetHandle<TextureData> h1(&meta1, data1);
    AssetHandle<TextureData> h2(&meta2, data2);

    h2 = h1;
    EXPECT_EQ(h2->width, 1u);
    EXPECT_EQ(meta1.ref_count.load(), 2u);
    EXPECT_EQ(meta2.ref_count.load(), 0u);
}

TEST(AssetHandle, MoveAssignment) {
    AssetMeta meta;
    meta.id = AssetId("test.png");
    auto data = std::make_shared<TextureData>();

    AssetHandle<TextureData> h1(&meta, data);
    AssetHandle<TextureData> h2;

    h2 = std::move(h1);
    EXPECT_TRUE(h2.valid());
    EXPECT_FALSE(h1.valid());
    EXPECT_EQ(meta.ref_count.load(), 1u);
}

TEST(AssetHandle, AccessorsId) {
    AssetMeta meta;
    meta.id = AssetId("test.png");
    meta.path = "textures/test.png";
    meta.status = AssetStatus::Loaded;
    auto data = std::make_shared<TextureData>();

    AssetHandle<TextureData> handle(&meta, data);
    EXPECT_EQ(handle.id(), meta.id);
    EXPECT_EQ(handle.path(), "textures/test.png");
    EXPECT_EQ(handle.status(), AssetStatus::Loaded);
}
