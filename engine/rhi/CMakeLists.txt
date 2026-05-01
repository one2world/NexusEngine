set(NEXUS_RHI_SOURCES
    src/gl_functions.cpp
    src/gl_rhi.cpp
    # ImGui platform backend (GLFW) — compiled exactly once.  All renderer
    # backends below link against the symbols this TU defines; including
    # imgui_impl_glfw.cpp in more than one TU would cause duplicate symbols
    # at link, and including it inside the Metal .mm (ARC) wrapper fails on
    # GLFW's Cocoa bridging.
    src/imgui_backend_glfw_platform.cpp
    # ImGui renderer binding for the OpenGL backend.
    src/imgui_backend_gl.cpp
)

# ── Vulkan 1.3 backend ────────────────────────────────────────────────────────
# Compiled only when a real Vulkan SDK is found on the host machine.
# The backend links against the real Vulkan loader; there is no CPU-simulated
# fallback in release builds.
if(NEXUS_ENABLE_VULKAN AND NEXUS_HAVE_VULKAN_SDK)
    list(APPEND NEXUS_RHI_SOURCES
        src/vk_rhi.cpp
        src/imgui_backend_vk.cpp
    )
endif()

# ── Metal backend ────────────────────────────────────────────────────────────
# Apple-only, compiled via Objective-C++ (.mm).
if(NEXUS_ENABLE_METAL AND NEXUS_HAVE_METAL)
    list(APPEND NEXUS_RHI_SOURCES
        src/metal_rhi.mm
        src/imgui_backend_metal.mm
    )
endif()

# ── WebGL 2.0 backend ────────────────────────────────────────────────────────
# Real GLES3/WebGL2 driver path under Emscripten.  The desktop build compiles
# the same TU as a test-only stub that is excluded from shipping targets by the
# NEXUS_RHI_WEBGL_DEVSTUB flag.  A release artifact must not set that flag.
if(EMSCRIPTEN OR NEXUS_ENABLE_WEBGL)
    list(APPEND NEXUS_RHI_SOURCES
        src/webgl_rhi.cpp
        src/imgui_backend_webgl.cpp
    )
endif()

add_library(nexus-rhi STATIC ${NEXUS_RHI_SOURCES})

target_include_directories(nexus-rhi
    PUBLIC  include
    PRIVATE src
)

# ── ImGui backend wiring ──────────────────────────────────────────────────────
# The RHI owns the platform + renderer ImGui bindings so the editor never
# touches imgui_impl_*.h directly.  We add ImGui's `backends/` directory so
# the wrapper TUs (src/imgui_backend_*.cpp/.mm) can `#include` the matching
# imgui_impl_*.h headers.  GLFW is a private link dep — only the wrapper TUs
# touch GLFW types; the RHI public surface stays GLFW-free.
target_include_directories(nexus-rhi PRIVATE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(nexus-rhi
    PUBLIC  Nexus::Core
    PRIVATE imgui glfw
)

# ImGui's own backend translation units do not pass our strict warning gate
# (they use C-style casts, sign conversions, etc.).  They are already
# `#include`d behind a local diagnostic-push in each src/imgui_backend_*.cpp,
# so no per-target relaxation is needed.

if(NEXUS_ENABLE_VULKAN AND NEXUS_HAVE_VULKAN_SDK)
    target_compile_definitions(nexus-rhi PUBLIC NEXUS_ENABLE_VULKAN=1)
    target_link_libraries(nexus-rhi PUBLIC Vulkan::Vulkan)

    if(NEXUS_HAVE_GLSLANG)
        target_compile_definitions(nexus-rhi PRIVATE NEXUS_HAVE_GLSLANG=1)
        target_include_directories(nexus-rhi PRIVATE ${NEXUS_GLSLANG_INCLUDE_DIR})
        target_link_libraries(nexus-rhi PRIVATE
            ${NEXUS_GLSLANG_LIB}
            ${NEXUS_GLSLANG_SPV_LIB}
        )
        if(NEXUS_GLSLANG_MGR_LIB)
            target_link_libraries(nexus-rhi PRIVATE ${NEXUS_GLSLANG_MGR_LIB})
        endif()
    endif()
endif()

if(NEXUS_ENABLE_OPENGL)
    target_compile_definitions(nexus-rhi PUBLIC NEXUS_ENABLE_OPENGL=1)
endif()

if(NEXUS_ENABLE_WEBGL)
    target_compile_definitions(nexus-rhi PUBLIC NEXUS_ENABLE_WEBGL=1)
    # When enabled on desktop it is a dev-only stub; release artifacts omit this target.
    if(NOT EMSCRIPTEN)
        target_compile_definitions(nexus-rhi PRIVATE NEXUS_RHI_WEBGL_DEVSTUB=1)
    endif()
endif()

if(NEXUS_ENABLE_METAL AND NEXUS_HAVE_METAL)
    target_compile_definitions(nexus-rhi PUBLIC NEXUS_ENABLE_METAL=1)
    target_link_libraries(nexus-rhi PUBLIC
        ${NEXUS_METAL_FRAMEWORK}
        ${NEXUS_QUARTZCORE_FRAMEWORK}
        ${NEXUS_FOUNDATION_FRAMEWORK}
    )
    set_source_files_properties(src/metal_rhi.mm PROPERTIES
        COMPILE_FLAGS "-fobjc-arc"
    )
    set_source_files_properties(src/imgui_backend_metal.mm PROPERTIES
        COMPILE_FLAGS "-fobjc-arc"
    )
endif()

nexus_set_compiler_flags(nexus-rhi)
nexus_set_build_defines(nexus-rhi)

add_library(Nexus::RHI ALIAS nexus-rhi)
