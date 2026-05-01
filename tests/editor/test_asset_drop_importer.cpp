#include <gtest/gtest.h>

#include "nexus/editor/asset_drop_importer.h"
#include "nexus/editor/editor_panels.h"
#include "nexus/renderer/forward_renderer_3d.h"
#include "nexus/rhi/rhi.h"

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace nexus::editor::tests {

using nexus::rhi::INVALID_HANDLE;
using nexus::rhi::TextureHandle;

// ── Recording RHI ────────────────────────────────────────────────────────────
//
// Same minimal stub used by the thumbnail-cache tests.  Every method is a
// no-op except the texture create/destroy ones we exercise.
class RecordingRHI final : public nexus::rhi::RHI {
public:
    bool init() override { return true; }
    void shutdown() override {}
    void begin_frame() override {}
    void end_frame() override {}
    void clear(nexus::Vec4, float) override {}
    void set_viewport(i32, i32, i32, i32) override {}
    void set_scissor(i32, i32, i32, i32) override {}

    nexus::rhi::BufferHandle create_buffer(const nexus::rhi::BufferDesc&) override { return 0; }
    void destroy_buffer(nexus::rhi::BufferHandle) override {}
    void update_buffer(nexus::rhi::BufferHandle, const void*, size_t, size_t) override {}

    nexus::rhi::TextureHandle create_texture(const nexus::rhi::TextureDesc& desc) override {
        ++created;
        last_w = desc.width;
        last_h = desc.height;
        return ++next_id;
    }
    void destroy_texture(nexus::rhi::TextureHandle) override { ++destroyed; }

    nexus::rhi::ShaderHandle create_shader(const std::string&, const std::string&) override { return 0; }
    void destroy_shader(nexus::rhi::ShaderHandle) override {}
    nexus::rhi::PipelineHandle create_pipeline(const nexus::rhi::PipelineDesc&) override { return 0; }
    void destroy_pipeline(nexus::rhi::PipelineHandle) override {}
    nexus::rhi::FramebufferHandle create_framebuffer(const nexus::rhi::FramebufferDesc&) override { return 0; }
    void destroy_framebuffer(nexus::rhi::FramebufferHandle) override {}

    void bind_pipeline(nexus::rhi::PipelineHandle) override {}
    void bind_shader(nexus::rhi::ShaderHandle) override {}
    void bind_texture(nexus::rhi::TextureHandle, u32) override {}
    void bind_framebuffer(nexus::rhi::FramebufferHandle) override {}
    void unbind_framebuffer() override {}
    void bind_vertex_buffer(nexus::rhi::BufferHandle) override {}
    void bind_index_buffer(nexus::rhi::BufferHandle) override {}

    void set_blend_mode(nexus::rhi::BlendMode) override {}
    void set_depth_test(bool) override {}
    void set_depth_write(bool) override {}
    void set_cull_mode(nexus::rhi::CullMode) override {}

    void set_uniform_int(nexus::rhi::ShaderHandle, const std::string&, i32) override {}
    void set_uniform_int_array(nexus::rhi::ShaderHandle, const std::string&, const i32*, u32) override {}
    void set_uniform_float(nexus::rhi::ShaderHandle, const std::string&, float) override {}
    void set_uniform_vec2(nexus::rhi::ShaderHandle, const std::string&, nexus::Vec2) override {}
    void set_uniform_vec3(nexus::rhi::ShaderHandle, const std::string&, nexus::Vec3) override {}
    void set_uniform_vec4(nexus::rhi::ShaderHandle, const std::string&, nexus::Vec4) override {}
    void set_uniform_mat4(nexus::rhi::ShaderHandle, const std::string&, const nexus::Mat4&) override {}

    void draw(u32, u32) override {}
    void draw_indexed(u32, u32) override {}

    bool imgui_init(void*) override { return true; }
    void imgui_shutdown() override {}
    void imgui_new_frame() override {}
    void imgui_render_draw_data() override {}
    bool textures_are_bottom_up() const override { return false; }

    std::atomic<u32> created{0};
    std::atomic<u32> destroyed{0};
    nexus::rhi::TextureHandle next_id{0};
    u32 last_w{0};
    u32 last_h{0};
};

// ── Helpers ─────────────────────────────────────────────────────────────────

namespace {

bool write_test_bmp(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    static const u8 bmp[] = {
        'B','M', 0x46,0x00,0x00,0x00, 0x00,0x00, 0x00,0x00, 0x36,0x00,0x00,0x00,
        0x28,0x00,0x00,0x00, 0x02,0x00,0x00,0x00, 0x02,0x00,0x00,0x00,
        0x01,0x00, 0x18,0x00, 0x00,0x00,0x00,0x00, 0x10,0x00,0x00,0x00,
        0x13,0x0B,0x00,0x00, 0x13,0x0B,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0xFF,0x00,0x00, 0x00,0xFF,0x00, 0x00,0x00,
        0x00,0x00,0xFF, 0xFF,0xFF,0xFF, 0x00,0x00,
    };
    const size_t n = std::fwrite(bmp, 1, sizeof(bmp), f);
    std::fclose(f);
    return n == sizeof(bmp);
}

// Tiny OBJ — single triangle.  MeshImporter parses positions / normals /
// uvs and builds a MeshData with 3 vertices + 3 indices.
bool write_test_obj(const std::string& path) {
    std::ofstream out(path);
    if (!out) return false;
    out << "v 0.0 0.0 0.0\n"
        << "v 1.0 0.0 0.0\n"
        << "v 0.0 1.0 0.0\n"
        << "vn 0.0 0.0 1.0\n"
        << "vt 0.0 0.0\n"
        << "vt 1.0 0.0\n"
        << "vt 0.0 1.0\n"
        << "f 1/1/1 2/2/1 3/3/1\n";
    return true;
}

std::filesystem::path tmp_path(const char* name) {
    return std::filesystem::temp_directory_path() / name;
}

}  // namespace

// ── Texture import ──────────────────────────────────────────────────────────

TEST(AssetDropImporterTexture, ImportRealBMPProducesTextureHandle) {
    namespace fs = std::filesystem;
    const fs::path bmp = tmp_path("nexus_drop_importer_test.bmp");
    ASSERT_TRUE(write_test_bmp(bmp.string()));

    RecordingRHI rhi;
    AssetDropImporter imp;
    imp.set_rhi(&rhi);

    const auto h = imp.import_texture(bmp.string());
    EXPECT_NE(h, INVALID_HANDLE);
    EXPECT_EQ(rhi.created.load(), 1u);
    EXPECT_EQ(rhi.last_w, 2u);
    EXPECT_EQ(rhi.last_h, 2u);
    EXPECT_EQ(imp.texture_count(), 1u);

    fs::remove(bmp);
}

TEST(AssetDropImporterTexture, RepeatedImportIsCachedNoSecondUpload) {
    namespace fs = std::filesystem;
    const fs::path bmp = tmp_path("nexus_drop_importer_cache.bmp");
    ASSERT_TRUE(write_test_bmp(bmp.string()));

    RecordingRHI rhi;
    AssetDropImporter imp;
    imp.set_rhi(&rhi);

    const auto h1 = imp.import_texture(bmp.string());
    const auto h2 = imp.import_texture(bmp.string());
    EXPECT_EQ(h1, h2);
    EXPECT_EQ(rhi.created.load(), 1u);

    fs::remove(bmp);
}

TEST(AssetDropImporterTexture, MissingFileReturnsInvalidWithNoCrash) {
    RecordingRHI rhi;
    AssetDropImporter imp;
    imp.set_rhi(&rhi);
    EXPECT_EQ(imp.import_texture("/nonexistent/path/asdfgh.png"),
              INVALID_HANDLE);
    EXPECT_EQ(rhi.created.load(), 0u);
}

TEST(AssetDropImporterTexture, NoRHIReturnsInvalid) {
    AssetDropImporter imp;  // no RHI bound
    EXPECT_EQ(imp.import_texture("/anywhere.png"), INVALID_HANDLE);
}

TEST(AssetDropImporterTexture, DestructorReleasesEveryUploadedTexture) {
    namespace fs = std::filesystem;
    const fs::path bmp = tmp_path("nexus_drop_importer_release.bmp");
    ASSERT_TRUE(write_test_bmp(bmp.string()));

    RecordingRHI rhi;
    {
        AssetDropImporter imp;
        imp.set_rhi(&rhi);
        imp.import_texture(bmp.string());
        EXPECT_EQ(rhi.destroyed.load(), 0u);
    }
    EXPECT_EQ(rhi.destroyed.load(), 1u);

    fs::remove(bmp);
}

// ── Mesh import ─────────────────────────────────────────────────────────────

TEST(AssetDropImporterMesh, ImportOBJProducesMeshIdAndRegistersWithViewport) {
    namespace fs = std::filesystem;
    const fs::path obj = tmp_path("nexus_drop_importer_test.obj");
    ASSERT_TRUE(write_test_obj(obj.string()));

    AssetDropImporter imp;
    ViewportPanel scene("Scene");
    ViewportPanel game("Game");
    imp.add_viewport(&scene);
    imp.add_viewport(&game);

    const u32 id = imp.import_mesh(obj.string());
    EXPECT_GT(id, 1023u);  // imported ids start at 1024
    EXPECT_EQ(imp.mesh_count(), 1u);

    // Both viewports should carry the same mesh pointer under the same id.
    Mesh* m_scene = scene.find_registered_mesh(id);
    Mesh* m_game  = game.find_registered_mesh(id);
    ASSERT_NE(m_scene, nullptr);
    ASSERT_EQ(m_scene, m_game);
    EXPECT_EQ(m_scene->vertices.size(), 3u);
    EXPECT_EQ(m_scene->indices.size(),  3u);

    fs::remove(obj);
}

TEST(AssetDropImporterMesh, RepeatedMeshImportIsCachedSameId) {
    namespace fs = std::filesystem;
    const fs::path obj = tmp_path("nexus_drop_importer_mesh_cache.obj");
    ASSERT_TRUE(write_test_obj(obj.string()));

    AssetDropImporter imp;
    ViewportPanel scene("Scene");
    imp.add_viewport(&scene);
    const u32 a = imp.import_mesh(obj.string());
    const u32 b = imp.import_mesh(obj.string());
    EXPECT_EQ(a, b);
    EXPECT_EQ(imp.mesh_count(), 1u);
    EXPECT_EQ(scene.registered_mesh_count(), 1u);

    fs::remove(obj);
}

TEST(AssetDropImporterMesh, MissingFileReturnsZero) {
    AssetDropImporter imp;
    EXPECT_EQ(imp.import_mesh("/nonexistent/missing.obj"), 0u);
    EXPECT_EQ(imp.mesh_count(), 0u);
}

TEST(AssetDropImporterMesh, MultipleMeshesGetUniqueIds) {
    namespace fs = std::filesystem;
    const fs::path a = tmp_path("nexus_drop_importer_a.obj");
    const fs::path b = tmp_path("nexus_drop_importer_b.obj");
    ASSERT_TRUE(write_test_obj(a.string()));
    ASSERT_TRUE(write_test_obj(b.string()));

    AssetDropImporter imp;
    ViewportPanel scene("Scene");
    imp.add_viewport(&scene);
    const u32 ia = imp.import_mesh(a.string());
    const u32 ib = imp.import_mesh(b.string());
    EXPECT_NE(ia, ib);
    EXPECT_EQ(scene.registered_mesh_count(), 2u);

    fs::remove(a);
    fs::remove(b);
}

TEST(AssetDropImporterMesh, AddViewportIsIdempotent) {
    AssetDropImporter imp;
    ViewportPanel vp("Scene");
    imp.add_viewport(&vp);
    imp.add_viewport(&vp);  // adding again must NOT cause double-register
    imp.add_viewport(nullptr);  // nullptr is a no-op

    namespace fs = std::filesystem;
    const fs::path obj = tmp_path("nexus_drop_importer_idem.obj");
    ASSERT_TRUE(write_test_obj(obj.string()));
    const u32 id = imp.import_mesh(obj.string());
    EXPECT_GT(id, 0u);
    // Even though add_viewport was called twice, the panel should hold one
    // entry (register_mesh is upsert-by-id, so duplicate add is harmless,
    // but the importer dedupes internally so we don't pay the cost twice).
    EXPECT_EQ(vp.registered_mesh_count(), 1u);

    fs::remove(obj);
}

}  // namespace nexus::editor::tests
