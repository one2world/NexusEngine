#include <gtest/gtest.h>

#include "nexus/editor/asset_thumbnail_cache.h"
#include "nexus/rhi/rhi.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <thread>
#include <vector>

namespace nexus::editor::tests {

using nexus::rhi::INVALID_HANDLE;
using nexus::rhi::TextureHandle;

// ── Recording RHI ────────────────────────────────────────────────────────────
//
// Records every create/destroy so the cache's lifecycle can be verified
// without spinning up a GL context.  Every method is a no-op except the
// texture create/destroy ones we actually exercise.
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
        last_width  = desc.width;
        last_height = desc.height;
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
    u32 last_width{0};
    u32 last_height{0};
};

static std::vector<u8> dummy_rgba8(u32 w, u32 h, u8 fill = 0x80) {
    return std::vector<u8>(static_cast<size_t>(w) * h * 4u, fill);
}

// ── Empty / lookup ──────────────────────────────────────────────────────────

TEST(AssetThumbnailCache, EmptyLookupReturnsInvalid) {
    AssetThumbnailCache cache(nullptr, 16);
    EXPECT_EQ(cache.lookup("missing.png"), INVALID_HANDLE);
    EXPECT_EQ(cache.state("missing.png"),
              AssetThumbnailCache::State::Empty);
    EXPECT_EQ(cache.size(), 0u);
}

// ── Inject decoded → drain uploads to RHI ───────────────────────────────────

TEST(AssetThumbnailCache, DrainUploadsDecodedPixelsToRHI) {
    RecordingRHI rhi;
    AssetThumbnailCache cache(&rhi, 16);

    cache.inject_decoded_pixels("a.png", 32, 16, dummy_rgba8(32, 16));
    EXPECT_EQ(cache.state("a.png"), AssetThumbnailCache::State::Decoded);

    EXPECT_EQ(cache.drain(), 1u);
    EXPECT_EQ(cache.state("a.png"), AssetThumbnailCache::State::Uploaded);
    EXPECT_EQ(rhi.created.load(), 1u);
    EXPECT_EQ(rhi.last_width, 32u);
    EXPECT_EQ(rhi.last_height, 16u);
    EXPECT_NE(cache.lookup("a.png"), INVALID_HANDLE);
    EXPECT_EQ(cache.drain(), 0u);
}

// ── Decode failure → DecodeFailed, no retry ─────────────────────────────────

TEST(AssetThumbnailCache, MarkFailedReturnsZeroAndDoesNotRetry) {
    RecordingRHI rhi;
    AssetThumbnailCache cache(&rhi, 16);
    cache.mark_failed("broken.png");
    EXPECT_EQ(cache.state("broken.png"),
              AssetThumbnailCache::State::DecodeFailed);
    EXPECT_EQ(cache.request("broken.png"), INVALID_HANDLE);
    EXPECT_EQ(cache.in_flight_count(), 0u);
}

// ── LRU eviction releases the texture ───────────────────────────────────────

TEST(AssetThumbnailCache, LRUEvictionReleasesTexture) {
    RecordingRHI rhi;
    AssetThumbnailCache cache(&rhi, 2);
    cache.inject_decoded_pixels("a.png", 4, 4, dummy_rgba8(4, 4));
    cache.inject_decoded_pixels("b.png", 4, 4, dummy_rgba8(4, 4));
    EXPECT_EQ(cache.drain(), 2u);
    EXPECT_EQ(rhi.destroyed.load(), 0u);

    cache.inject_decoded_pixels("c.png", 4, 4, dummy_rgba8(4, 4));
    EXPECT_EQ(rhi.destroyed.load(), 1u);
    EXPECT_EQ(cache.size(), 2u);
    EXPECT_EQ(cache.state("a.png"), AssetThumbnailCache::State::Empty);

    EXPECT_EQ(cache.drain(), 1u);
    EXPECT_EQ(rhi.created.load(), 3u);
}

// ── Promote on access ───────────────────────────────────────────────────────

TEST(AssetThumbnailCache, RequestPromotesAccessedEntryToMRU) {
    RecordingRHI rhi;
    AssetThumbnailCache cache(&rhi, 2);
    cache.inject_decoded_pixels("a.png", 4, 4, dummy_rgba8(4, 4));
    cache.inject_decoded_pixels("b.png", 4, 4, dummy_rgba8(4, 4));
    cache.drain();

    cache.request("a.png");  // promote a -> MRU
    cache.inject_decoded_pixels("c.png", 4, 4, dummy_rgba8(4, 4));

    EXPECT_EQ(cache.state("a.png"), AssetThumbnailCache::State::Uploaded);
    EXPECT_EQ(cache.state("b.png"), AssetThumbnailCache::State::Empty);
    EXPECT_EQ(cache.state("c.png"), AssetThumbnailCache::State::Decoded);
}

// ── Invalidate ──────────────────────────────────────────────────────────────

TEST(AssetThumbnailCache, InvalidateReleasesTextureAndDropsEntry) {
    RecordingRHI rhi;
    AssetThumbnailCache cache(&rhi, 16);
    cache.inject_decoded_pixels("a.png", 8, 8, dummy_rgba8(8, 8));
    cache.drain();
    EXPECT_EQ(rhi.created.load(), 1u);

    cache.invalidate("a.png");
    EXPECT_EQ(rhi.destroyed.load(), 1u);
    EXPECT_EQ(cache.size(), 0u);
    EXPECT_EQ(cache.state("a.png"), AssetThumbnailCache::State::Empty);
}

// ── Clear ───────────────────────────────────────────────────────────────────

TEST(AssetThumbnailCache, ClearReleasesAllTextures) {
    RecordingRHI rhi;
    AssetThumbnailCache cache(&rhi, 16);
    cache.inject_decoded_pixels("a.png", 4, 4, dummy_rgba8(4, 4));
    cache.inject_decoded_pixels("b.png", 4, 4, dummy_rgba8(4, 4));
    cache.drain();
    EXPECT_EQ(rhi.created.load(), 2u);

    cache.clear();
    EXPECT_EQ(rhi.destroyed.load(), 2u);
    EXPECT_EQ(cache.size(), 0u);
}

TEST(AssetThumbnailCache, SetCapacityShrinksToFit) {
    RecordingRHI rhi;
    AssetThumbnailCache cache(&rhi, 16);
    cache.inject_decoded_pixels("a.png", 4, 4, dummy_rgba8(4, 4));
    cache.inject_decoded_pixels("b.png", 4, 4, dummy_rgba8(4, 4));
    cache.inject_decoded_pixels("c.png", 4, 4, dummy_rgba8(4, 4));
    cache.drain();
    EXPECT_EQ(cache.size(), 3u);

    cache.set_capacity(1);
    EXPECT_EQ(cache.size(), 1u);
    EXPECT_EQ(rhi.destroyed.load(), 2u);
}

// ── Real decode end-to-end (writes a tiny BMP, decodes via stb_image) ──────

namespace {

bool write_test_bmp(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    static const u8 bmp[] = {
        'B','M', 0x46,0x00,0x00,0x00, 0x00,0x00, 0x00,0x00, 0x36,0x00,0x00,0x00,
        0x28,0x00,0x00,0x00, 0x02,0x00,0x00,0x00, 0x02,0x00,0x00,0x00,
        0x01,0x00, 0x18,0x00, 0x00,0x00,0x00,0x00, 0x10,0x00,0x00,0x00,
        0x13,0x0B,0x00,0x00, 0x13,0x0B,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0xFF,0x00,0x00,  0x00,0xFF,0x00, 0x00,0x00,
        0x00,0x00,0xFF,  0xFF,0xFF,0xFF, 0x00,0x00,
    };
    const size_t n = std::fwrite(bmp, 1, sizeof(bmp), f);
    std::fclose(f);
    return n == sizeof(bmp);
}

}  // namespace

TEST(AssetThumbnailCache, RequestDecodesRealFileViaStbImage) {
    namespace fs = std::filesystem;
    const fs::path tmp = fs::temp_directory_path() /
        "nexus_thumbnail_cache_test.bmp";
    ASSERT_TRUE(write_test_bmp(tmp.string()));

    RecordingRHI rhi;
    AssetThumbnailCache cache(&rhi, 16);
    EXPECT_EQ(cache.request(tmp.string()), INVALID_HANDLE);
    EXPECT_EQ(cache.state(tmp.string()),
              AssetThumbnailCache::State::Pending);

    using namespace std::chrono_literals;
    bool uploaded = false;
    for (int i = 0; i < 200 && !uploaded; ++i) {
        if (cache.drain() > 0) uploaded = true;
        if (!uploaded) std::this_thread::sleep_for(10ms);
    }
    EXPECT_TRUE(uploaded);
    EXPECT_EQ(cache.state(tmp.string()),
              AssetThumbnailCache::State::Uploaded);
    EXPECT_NE(cache.lookup(tmp.string()), INVALID_HANDLE);
    EXPECT_EQ(rhi.last_width,  2u);
    EXPECT_EQ(rhi.last_height, 2u);

    fs::remove(tmp);
}

TEST(AssetThumbnailCache, ConcurrentRequestsDedupToSingleDecode) {
    AssetThumbnailCache cache(nullptr, 16);
    cache.request("dup.png");
    cache.request("dup.png");
    cache.request("dup.png");
    EXPECT_EQ(cache.in_flight_count(), 1u);
}

}  // namespace nexus::editor::tests
