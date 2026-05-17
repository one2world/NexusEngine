// ============================================================================
// imgui_backend_vk.cpp — Vulkan backend's ImGui binding.
//
// VulkanRHI today owns a real VkInstance / VkDevice / VkQueue but does not
// own a window swapchain or a present surface — RHI::create_framebuffer()
// returns offscreen-only render targets.  ImGui_ImplVulkan requires a
// VkRenderPass that matches the surface format, a descriptor pool sized for
// font textures, and a frame-in-flight count tied to the swapchain.
//
// Rather than fabricate any of that, this backend reports unsupported via
// imgui_init() returning false.  The host (editor) is expected to fall back
// to OpenGL or Metal when running under Vulkan.  This is a feature gap, not
// a workaround — once VulkanRHI grows surface/swapchain support the binding
// will be filled in here without changing the public abstraction.
//
// Compiled only when NEXUS_ENABLE_VULKAN is defined.
// ============================================================================

#include <nexus/rhi/vk_rhi.h>
#include <nexus/core/log.h>

#if defined(NEXUS_ENABLE_VULKAN)

namespace nexus::rhi {

bool VulkanRHI::imgui_init(void* /*native_window*/) {
    NX_WARN("VulkanRHI::imgui_init: Vulkan ImGui binding requires swapchain "
            "support that is not yet wired into VulkanRHI.  The editor cannot "
            "host its UI on this backend yet — select OpenGL or Metal.");
    return false;
}

void VulkanRHI::imgui_shutdown()         {}
void VulkanRHI::imgui_new_frame()        {}
void VulkanRHI::imgui_render_draw_data() {}

} // namespace nexus::rhi

#endif // NEXUS_ENABLE_VULKAN
