// ============================================================================
// imgui_backend_metal.mm — Metal backend's ImGui binding.
//
// Uses ImGui_ImplGlfw_InitForOther for the platform side (GLFW does not have
// a Metal-aware init helper — "Other" tells it to skip GL/Vulkan-specific
// callbacks) and ImGui_ImplMetal for the renderer side.
//
// Compiled only when the Metal backend is enabled (NEXUS_ENABLE_METAL).
// ============================================================================

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
#if defined(__clang__)
// Apple's MTL* headers themselves are nullability-annotated; ImGui's Metal
// backend predates that convention and trips -Wnullability-completeness on
// every Objective-C pointer signature.  Silence within the wrapper TU only.
#pragma clang diagnostic ignored "-Wnullability-completeness"
#pragma clang diagnostic ignored "-Wnullable-to-nonnull-conversion"
#endif

#define GLFW_INCLUDE_NONE

// imgui_impl_glfw.cpp lives in imgui_backend_glfw_platform.cpp (single TU).
// Including it here would (a) collide with that TU at link, and (b) fail
// under ARC because GLFW's Cocoa helpers cast Objective-C `id` to `void*`
// without a __bridge.
#include "imgui_impl_metal.mm"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <nexus/rhi/metal_rhi.h>

#include "imgui_impl_glfw.h"
#include "imgui_impl_metal.h"
#include <GLFW/glfw3.h>
#include <imgui.h>
#import <Metal/Metal.h>

namespace nexus::rhi {

bool MetalRHI::imgui_init(void* native_window) {
    auto* window = static_cast<GLFWwindow*>(native_window);
    if (!window || !device_) return false;

    if (!ImGui_ImplGlfw_InitForOther(window, /*install_callbacks=*/true)) {
        return false;
    }
    imgui_platform_installed_ = true;

    id<MTLDevice> device = (__bridge id<MTLDevice>)device_;
    if (!ImGui_ImplMetal_Init(device)) {
        ImGui_ImplGlfw_Shutdown();
        imgui_platform_installed_ = false;
        return false;
    }
    imgui_renderer_installed_ = true;
    return true;
}

void MetalRHI::imgui_shutdown() {
    if (imgui_renderer_installed_) {
        ImGui_ImplMetal_Shutdown();
        imgui_renderer_installed_ = false;
    }
    if (imgui_platform_installed_) {
        ImGui_ImplGlfw_Shutdown();
        imgui_platform_installed_ = false;
    }
}

void MetalRHI::imgui_new_frame() {
    // ImGui_ImplMetal_NewFrame requires the active MTLRenderPassDescriptor so
    // it can size internal buffers to match the current target.  The host's
    // bound framebuffer drives that — when no FBO is bound (drawing to the
    // window), the editor's per-frame setup must call this through the RHI
    // *after* a render pass descriptor is available.  We cache the encoder's
    // descriptor when bind_framebuffer fires; here we reuse the most recent
    // one.  If none exists yet, we fall back to a default descriptor so ImGui
    // can still allocate vertex buffers on the first frame.
    MTLRenderPassDescriptor* desc = nil;
    if (current_encoder_) {
        // The encoder owns its descriptor internally; we rebuild a minimal
        // matching one here so ImGui_ImplMetal_NewFrame has the format it
        // needs.  Single attachment, BGRA8 — matches MetalRHI's offscreen.
        desc = [MTLRenderPassDescriptor renderPassDescriptor];
        if (offscreen_color_) {
            desc.colorAttachments[0].texture =
                (__bridge id<MTLTexture>)offscreen_color_;
            desc.colorAttachments[0].loadAction = MTLLoadActionLoad;
            desc.colorAttachments[0].storeAction = MTLStoreActionStore;
        }
    }
    if (!desc) {
        desc = [MTLRenderPassDescriptor renderPassDescriptor];
    }
    ImGui_ImplMetal_NewFrame(desc);
    ImGui_ImplGlfw_NewFrame();
}

void MetalRHI::imgui_render_draw_data() {
    if (!current_cmd_ || !current_encoder_) return;
    id<MTLCommandBuffer>        cmd = (__bridge id<MTLCommandBuffer>)current_cmd_;
    id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)current_encoder_;
    ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), cmd, enc);
}

} // namespace nexus::rhi
