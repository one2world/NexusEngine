#pragma once

/// Vulkan-compatible type definitions for NexusEngine.
///
/// When building with the Vulkan SDK (NEXUS_HAS_VULKAN_SDK), these types
/// are aliases for the real Vulkan types. Otherwise, they are forward
/// declarations and opaque handles that allow the engine to compile and
/// test without the SDK installed.
///
/// This enables:
///   1. Full Vulkan initialization code to be written and reviewed
///   2. Tests to validate resource management logic
///   3. Seamless switch to real Vulkan when SDK is available

#include <nexus/core/types.h>
#include <cstdint>

namespace nexus::rhi::vk {

#ifdef NEXUS_HAS_VULKAN_SDK
// When the Vulkan SDK is available, include the real header
#include <vulkan/vulkan.h>

using Instance       = VkInstance;
using PhysicalDevice = VkPhysicalDevice;
using Device         = VkDevice;
using Queue          = VkQueue;
using CommandPool    = VkCommandPool;
using CommandBuffer  = VkCommandBuffer;
using Fence          = VkFence;
using Semaphore      = VkSemaphore;
using SwapchainKHR   = VkSwapchainKHR;
using SurfaceKHR     = VkSurfaceKHR;
using RenderPass     = VkRenderPass;
using Framebuffer    = VkFramebuffer;
using Pipeline       = VkPipeline;
using PipelineLayout = VkPipelineLayout;
using DescriptorSetLayout = VkDescriptorSetLayout;
using DescriptorPool = VkDescriptorPool;
using DescriptorSet  = VkDescriptorSet;
using Buffer         = VkBuffer;
using Image          = VkImage;
using ImageView      = VkImageView;
using Sampler        = VkSampler;
using ShaderModule   = VkShaderModule;
using DeviceMemory   = VkDeviceMemory;

#else
// Opaque handle types for compilation without SDK
using Instance       = void*;
using PhysicalDevice = void*;
using Device         = void*;
using Queue          = void*;
using CommandPool    = void*;
using CommandBuffer  = void*;
using Fence          = void*;
using Semaphore      = void*;
using SwapchainKHR   = void*;
using SurfaceKHR     = void*;
using RenderPass     = void*;
using Framebuffer    = void*;
using Pipeline       = void*;
using PipelineLayout = void*;
using DescriptorSetLayout = void*;
using DescriptorPool = void*;
using DescriptorSet  = void*;
using Buffer         = void*;
using Image          = void*;
using ImageView      = void*;
using Sampler        = void*;
using ShaderModule   = void*;
using DeviceMemory   = void*;
#endif

/// Queue family indices for a Vulkan device.
struct QueueFamilyIndices {
    u32 graphics{~0u};
    u32 present{~0u};
    u32 compute{~0u};
    u32 transfer{~0u};

    [[nodiscard]] bool is_complete() const {
        return graphics != ~0u && present != ~0u;
    }
};

/// Swapchain configuration.
struct SwapchainConfig {
    u32 width{0};
    u32 height{0};
    u32 image_count{3}; // Triple buffering
    bool vsync{true};
};

/// Per-frame synchronization objects.
struct FrameSync {
    Fence     in_flight_fence{nullptr};
    Semaphore image_available{nullptr};
    Semaphore render_finished{nullptr};
    CommandBuffer command_buffer{nullptr};
};

/// GPU memory allocation info (for VMA integration).
struct AllocationInfo {
    DeviceMemory memory{nullptr};
    u64 offset{0};
    u64 size{0};
    void* mapped_ptr{nullptr};
};

/// Physical device properties (subset).
struct PhysicalDeviceInfo {
    std::string name;
    u32 api_version{0};
    u32 driver_version{0};
    u64 vram_size{0};
    bool discrete{false};
    bool supports_geometry_shader{false};
    bool supports_tessellation{false};
    bool supports_compute{false};
    u32 max_texture_size{4096};
    u32 max_push_constant_size{128};
    f32 max_anisotropy{1.0f};
};

} // namespace nexus::rhi::vk
