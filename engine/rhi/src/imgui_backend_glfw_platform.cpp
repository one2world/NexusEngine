// ============================================================================
// imgui_backend_glfw_platform.cpp — single TU that compiles ImGui's GLFW
// platform backend.
//
// Why this file exists:
//   On Apple, the Metal renderer wrapper (imgui_backend_metal.mm) is .mm and
//   compiled with -fobjc-arc.  ImGui's imgui_impl_glfw.cpp pulls in Cocoa via
//   `glfwGetCocoaWindow()` and casts the returned `id` to `void*` with a
//   C-style cast — illegal under ARC.  Rather than special-case-build the
//   GLFW backend without ARC inside an .mm, we compile it exactly once here
//   in a plain .cpp TU.  The OpenGL/WebGL/Vulkan/Metal renderer wrappers all
//   call into the symbols this TU defines.
//
//   Compiling it more than once would produce duplicate symbols at link
//   time, so this file is the single owner.  All other RHI source files
//   include only `imgui_impl_glfw.h`.
// ============================================================================

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

// Prevent GLFW from pulling in any system <GL/gl.h>.  ImGui's GL renderer
// wrapper (imgui_backend_gl.cpp) installs its own GL loader; the platform TU
// itself doesn't need GL types.
#define GLFW_INCLUDE_NONE

#include "imgui_impl_glfw.cpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
