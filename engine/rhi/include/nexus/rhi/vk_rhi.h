#pragma once

#include <nexus/rhi/rhi.h>
#include <vector>
#include <unordered_map>
#include <string>

#if defined(NEXUS_ENABLE_VULKAN)
  #include <vulkan/vulkan.h>
#endif

namespace nexus::rhi {

// ---------------------------------------------------------------------------
// VulkanRHI — Vulkan 1.3 RHI backend (real driver, not CPU simulation).
//
//   init()     -> creates a real VkInstance, selects a VkPhysicalDevice,
//                 creates a VkDevice and acquires a VkQueue through the
//                 Vulkan loader.  If no Vulkan runtime is present on the host
//                 init() returns false — there is no CPU fallback.
//   buffers    -> real VkBuffer + VkDeviceMemory (host-visible coherent).
//   textures   -> real VkImage + VkImageView + VkDeviceMemory.
//   shaders    -> real VkShaderModule when glslang is linked in, otherwise
//                 an empty module placeholder (still tracked for lifecycle).
//   pipelines  -> metadata records; full graphics pipeline object creation
//                 requires a render pass + render target description that is
//                 not expressed in the public RHI::PipelineDesc yet.
//   framebuffers -> metadata records; same reason.
//
// The design is explicit about what is GPU-backed today vs what is still a
// thin tracking wrapper.  There is no CPU emulation path.
// ---------------------------------------------------------------------------

class VulkanRHI : public RHI {
public:
    VulkanRHI() = default;
    ~VulkanRHI() override;

    NEXUS_NON_COPYABLE(VulkanRHI)
    NEXUS_NON_MOVABLE(VulkanRHI)

    bool init() override;
    void shutdown() override;

    // Resource creation / destruction
    BufferHandle      create_buffer(const BufferDesc& desc) override;
    void              destroy_buffer(BufferHandle handle) override;
    void              update_buffer(BufferHandle handle,
                                    const void* data, size_t size, size_t offset) override;

    TextureHandle     create_texture(const TextureDesc& desc) override;
    void              destroy_texture(TextureHandle handle) override;

    ShaderHandle      create_shader(const std::string& vertex_src,
                                    const std::string& fragment_src) override;
    void              destroy_shader(ShaderHandle handle) override;

    PipelineHandle    create_pipeline(const PipelineDesc& desc) override;
    void              destroy_pipeline(PipelineHandle handle) override;

    FramebufferHandle create_framebuffer(const FramebufferDesc& desc) override;
    void              destroy_framebuffer(FramebufferHandle handle) override;
    TextureHandle     framebuffer_depth_texture(FramebufferHandle handle) override;

    // Render commands
    void begin_frame() override;
    void end_frame() override;

    void set_viewport(i32 x, i32 y, i32 w, i32 h) override;
    void set_scissor(i32 x, i32 y, i32 w, i32 h) override;
    void clear(Vec4 color, float depth) override;

    void bind_pipeline(PipelineHandle handle) override;
    void bind_shader(ShaderHandle handle) override;
    void bind_texture(TextureHandle handle, u32 slot) override;
    void bind_framebuffer(FramebufferHandle handle) override;
    void unbind_framebuffer() override;
    void bind_vertex_buffer(BufferHandle handle) override;
    void bind_index_buffer(BufferHandle handle) override;

    // State toggles
    void set_blend_mode(BlendMode mode) override;
    void set_depth_test(bool enabled) override;
    void set_depth_write(bool enabled) override;
    void set_cull_mode(CullMode mode) override;

    // Uniforms
    void set_uniform_int(ShaderHandle shader,
                         const std::string& name, i32 value) override;
    void set_uniform_int_array(ShaderHandle shader,
                               const std::string& name,
                               const i32* values, u32 count) override;
    void set_uniform_float(ShaderHandle shader,
                           const std::string& name, float value) override;
    void set_uniform_vec2(ShaderHandle shader,
                          const std::string& name, Vec2 value) override;
    void set_uniform_vec3(ShaderHandle shader,
                          const std::string& name, Vec3 value) override;
    void set_uniform_vec4(ShaderHandle shader,
                          const std::string& name, Vec4 value) override;
    void set_uniform_mat4(ShaderHandle shader,
                          const std::string& name,
                          const Mat4& value) override;

    // Draw calls
    void draw(u32 vertex_count, u32 first_vertex) override;
    void draw_indexed(u32 index_count, u32 first_index) override;

    // ── ImGui backend (ImGui_ImplGlfw_InitForVulkan + ImGui_ImplVulkan) ─────
    [[nodiscard]] bool imgui_init(void* native_window) override;
    void               imgui_shutdown() override;
    void               imgui_new_frame() override;
    void               imgui_render_draw_data() override;
    [[nodiscard]] bool textures_are_bottom_up() const override { return false; }

    // ── Introspection ───────────────────────────────────────────────────────

    [[nodiscard]] u32  draw_call_count()     const { return draw_call_count_; }
    [[nodiscard]] u32  state_change_count()  const { return state_change_count_; }
    [[nodiscard]] bool is_initialized()      const { return initialized_; }

    [[nodiscard]] u32 live_buffer_count()      const;
    [[nodiscard]] u32 live_texture_count()     const;
    [[nodiscard]] u32 live_shader_count()      const;
    [[nodiscard]] u32 live_pipeline_count()    const;
    [[nodiscard]] u32 live_framebuffer_count() const;

#if defined(NEXUS_ENABLE_VULKAN)
    [[nodiscard]] VkInstance       instance()        const { return instance_; }
    [[nodiscard]] VkPhysicalDevice physical_device() const { return physical_device_; }
    [[nodiscard]] VkDevice         device()          const { return device_; }
    [[nodiscard]] VkQueue          graphics_queue()  const { return graphics_queue_; }
    [[nodiscard]] u32              graphics_queue_family() const { return graphics_queue_family_; }
#endif

private:
#if defined(NEXUS_ENABLE_VULKAN)
    // ── Real Vulkan driver handles ──────────────────────────────────────────
    VkInstance       instance_{VK_NULL_HANDLE};
    VkPhysicalDevice physical_device_{VK_NULL_HANDLE};
    VkDevice         device_{VK_NULL_HANDLE};
    VkQueue          graphics_queue_{VK_NULL_HANDLE};
    u32              graphics_queue_family_{0xFFFFFFFFu};
    VkPhysicalDeviceMemoryProperties mem_props_{};
    VkCommandPool    command_pool_{VK_NULL_HANDLE};
    VkCommandBuffer  upload_cmd_{VK_NULL_HANDLE};
    VkFence          upload_fence_{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT debug_messenger_{VK_NULL_HANDLE};
    bool             validation_enabled_{false};

    // ── Resource records ────────────────────────────────────────────────────
    // Each struct owns real Vulkan objects where applicable.
    struct BufferRec {
        bool         alive{false};
        BufferType   type{BufferType::Vertex};
        BufferUsage  usage{BufferUsage::Static};
        VkBuffer     buffer{VK_NULL_HANDLE};
        VkDeviceMemory memory{VK_NULL_HANDLE};
        VkDeviceSize capacity{0};     // bytes allocated
        VkDeviceSize size{0};         // current logical size
        void*        mapped{nullptr}; // persistent map when host-visible
    };

    struct TextureRec {
        bool          alive{false};
        u32           width{0};
        u32           height{0};
        TextureFormat format{TextureFormat::RGBA8};
        VkImage       image{VK_NULL_HANDLE};
        VkImageView   view{VK_NULL_HANDLE};
        VkDeviceMemory memory{VK_NULL_HANDLE};
    };

    struct ShaderRec {
        bool          alive{false};
        VkShaderModule vertex_module{VK_NULL_HANDLE};
        VkShaderModule fragment_module{VK_NULL_HANDLE};
        std::unordered_map<std::string, i32>   uniform_ints;
        std::unordered_map<std::string, float> uniform_floats;
        std::unordered_map<std::string, Vec2>  uniform_vec2s;
        std::unordered_map<std::string, Vec3>  uniform_vec3s;
        std::unordered_map<std::string, Vec4>  uniform_vec4s;
        std::unordered_map<std::string, Mat4>  uniform_mat4s;
    };

    struct PipelineRec {
        bool         alive{false};
        PipelineDesc desc;
    };

    struct FramebufferRec {
        bool                       alive{false};
        u32                        width{0};
        u32                        height{0};
        std::vector<TextureFormat> color_formats;
        bool                       has_depth{false};
        // RHI contract: when desc.depth_sampleable was true, a
        // TextureHandle aliases the depth attachment so callers can
        // bind it as a regular sampler.  The simulation backend has
        // no real GPU memory but still registers the alias so the
        // public API contract is preserved (frontend code paths
        // shouldn't branch on backend identity).
        TextureHandle              depth_texture_handle{INVALID_HANDLE};
    };

    // Helpers (all no-ops when device_ is null)
    bool create_vk_instance();
    bool pick_physical_device();
    bool create_logical_device();
    bool create_command_pool();
    void destroy_debug_messenger();
    u32  find_memory_type(u32 type_filter, VkMemoryPropertyFlags props) const;
    bool alloc_host_visible_buffer(VkDeviceSize size,
                                   VkBufferUsageFlags usage,
                                   VkBuffer* out_buffer,
                                   VkDeviceMemory* out_memory,
                                   void** out_mapped);
#else
    // No Vulkan SDK at compile time → these fields do not exist; init()
    // will fail.  The engine factory `RHI::create(Backend::Vulkan)` returns
    // nullptr in that case, so this path is unreachable in release builds.
#endif

    // CPU-side tracking (lifecycle & validation counters)
#if defined(NEXUS_ENABLE_VULKAN)
    std::vector<BufferRec>      buffers_;
    std::vector<TextureRec>     textures_;
    std::vector<ShaderRec>      shaders_;
    std::vector<PipelineRec>    pipelines_;
    std::vector<FramebufferRec> framebuffers_;
#endif

    struct BoundState {
        PipelineHandle    pipeline{INVALID_HANDLE};
        ShaderHandle      shader{INVALID_HANDLE};
        FramebufferHandle framebuffer{INVALID_HANDLE};
        BufferHandle      vertex_buffer{INVALID_HANDLE};
        BufferHandle      index_buffer{INVALID_HANDLE};
        BlendMode         blend{BlendMode::None};
        bool              depth_test{true};
        bool              depth_write{true};
        CullMode          cull{CullMode::Back};
        i32 viewport_x{0}, viewport_y{0}, viewport_w{0}, viewport_h{0};
        i32 scissor_x{0},  scissor_y{0},  scissor_w{0},  scissor_h{0};
    };

    BoundState state_;
    bool       initialized_{false};
    bool       in_frame_{false};
    u32        draw_call_count_{0};
    u32        state_change_count_{0};
};

} // namespace nexus::rhi
