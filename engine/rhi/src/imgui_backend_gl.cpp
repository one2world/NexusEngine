// ============================================================================
// imgui_backend_gl.cpp — OpenGL backend's ImGui binding.
//
// This TU compiles ImGui's OpenGL3 renderer backend and implements
// `OpenGLRHI::imgui_*` against it.  The GLFW *platform* backend is NOT
// included here — it lives in `imgui_backend_glfw_platform.cpp` so it is
// compiled exactly once (the Metal wrapper is .mm under ARC and cannot
// safely include it; including it here too would produce duplicate symbols).
//
// Compiled only when the OpenGL backend is enabled (NEXUS_ENABLE_OPENGL).
// ============================================================================

// ── Suppress all third-party warnings ───────────────────────────────────────
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#pragma GCC diagnostic ignored "-Wuseless-cast"
#endif
#endif

// Prevent GLFW from pulling in <GL/gl.h>; ImGui ships its own loader.
#define GLFW_INCLUDE_NONE

#include "imgui_impl_opengl3.cpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

// ── OpenGLRHI::imgui_* implementations ──────────────────────────────────────
#include <nexus/rhi/gl_rhi.h>

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <imgui.h>

namespace nexus::rhi {

bool OpenGLRHI::imgui_init(void* native_window) {
    auto* window = static_cast<GLFWwindow*>(native_window);
    if (!window) return false;

    // Platform half first — if it fails, do not install the renderer half so
    // imgui_shutdown() has nothing partial to tear down.
    if (!ImGui_ImplGlfw_InitForOpenGL(window, /*install_callbacks=*/true)) {
        return false;
    }
    imgui_platform_installed_ = true;

    // GLSL version string is the lowest desktop core profile that supports
    // ImGui's vertex layout — matches the editor's prior hardcoded init.
    if (!ImGui_ImplOpenGL3_Init("#version 150")) {
        ImGui_ImplGlfw_Shutdown();
        imgui_platform_installed_ = false;
        return false;
    }
    imgui_renderer_installed_ = true;
    return true;
}

void OpenGLRHI::imgui_shutdown() {
    // Reverse install order — renderer holds GL resources that must die first.
    if (imgui_renderer_installed_) {
        ImGui_ImplOpenGL3_Shutdown();
        imgui_renderer_installed_ = false;
    }
    if (imgui_platform_installed_) {
        ImGui_ImplGlfw_Shutdown();
        imgui_platform_installed_ = false;
    }
}

void OpenGLRHI::imgui_new_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
}

void OpenGLRHI::imgui_render_draw_data() {
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

} // namespace nexus::rhi
