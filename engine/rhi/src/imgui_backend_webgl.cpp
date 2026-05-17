// ============================================================================
// imgui_backend_webgl.cpp — WebGL2 / Emscripten backend's ImGui binding.
//
// On Emscripten WebGL2 acts like GLES3, so ImGui_ImplOpenGL3 with a
// `#version 300 es` GLSL header and ImGui_ImplGlfw_InitForOpenGL against the
// Emscripten GLFW shim works out of the box.
//
// On the desktop dev-stub build (NEXUS_RHI_WEBGL_DEVSTUB) this backend exists
// only for unit tests — there is no window, no GL context, and the editor
// never instantiates it.  The methods are no-ops in that mode so the symbol
// table is complete (the abstract base requires the overrides) without
// pulling in GLFW or ImGui's GL backend on a path that would never run.
// ============================================================================

#include <nexus/rhi/webgl_rhi.h>
#include <nexus/core/log.h>

#if defined(__EMSCRIPTEN__)

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#define GLFW_INCLUDE_NONE
#define IMGUI_IMPL_OPENGL_ES3

// imgui_impl_glfw.cpp lives in imgui_backend_glfw_platform.cpp (single TU).
#include "imgui_impl_opengl3.cpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <imgui.h>

namespace nexus::rhi {

bool WebGLRHI::imgui_init(void* native_window) {
    auto* window = static_cast<GLFWwindow*>(native_window);
    if (!window) return false;
    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) return false;
    imgui_platform_installed_ = true;
    if (!ImGui_ImplOpenGL3_Init("#version 300 es")) {
        ImGui_ImplGlfw_Shutdown();
        imgui_platform_installed_ = false;
        return false;
    }
    imgui_renderer_installed_ = true;
    return true;
}

void WebGLRHI::imgui_shutdown() {
    if (imgui_renderer_installed_) {
        ImGui_ImplOpenGL3_Shutdown();
        imgui_renderer_installed_ = false;
    }
    if (imgui_platform_installed_) {
        ImGui_ImplGlfw_Shutdown();
        imgui_platform_installed_ = false;
    }
}

void WebGLRHI::imgui_new_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
}

void WebGLRHI::imgui_render_draw_data() {
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

} // namespace nexus::rhi

#else // !__EMSCRIPTEN__ — desktop dev-stub: methods exist but do nothing.

namespace nexus::rhi {
bool WebGLRHI::imgui_init(void* /*native_window*/) { return true; }
void WebGLRHI::imgui_shutdown()                    {}
void WebGLRHI::imgui_new_frame()                   {}
void WebGLRHI::imgui_render_draw_data()            {}
} // namespace nexus::rhi

#endif // __EMSCRIPTEN__
