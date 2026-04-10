// ============================================================================
// imgui_backends.cpp — Compile ImGui platform backends for the editor
//
// We compile the GLFW and OpenGL3 backends here, separate from the ImGui core
// library, because the OpenGL3 backend needs GL types and its own internal
// function loader (imgui_impl_opengl3_loader.h).
// ============================================================================

// Suppress all warnings from third-party code
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

// Prevent GLFW from pulling in system <GL/gl.h> — ImGui's OpenGL3 backend
// ships its own minimal GL loader (imgui_impl_opengl3_loader.h) and the
// system header defines GL_VERSION_1_0/1_1 which causes ImGui's loader to
// skip its own GL 1.0/1.1 typedefs, breaking the ImGL3WProcs struct.
#define GLFW_INCLUDE_NONE

// ImGui GLFW backend
#include "imgui_impl_glfw.cpp"

// ImGui OpenGL3 backend — uses its own built-in GL loader
// (imgui_impl_opengl3_loader.h is auto-included by imgui_impl_opengl3.cpp)
#include "imgui_impl_opengl3.cpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
