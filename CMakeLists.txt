cmake_minimum_required(VERSION 3.21)

project(NexusEngine
    VERSION 1.0.0
    DESCRIPTION "A modern 2D + 3D hybrid game engine"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(NEXUS_BUILD_EDITOR    "Build the NexusEngine editor"     ON)
option(NEXUS_BUILD_SANDBOX   "Build the sandbox demo project"   ON)
option(NEXUS_BUILD_TESTS     "Build unit tests"                 ON)
option(NEXUS_ENABLE_VULKAN   "Enable Vulkan rendering backend"  ON)
option(NEXUS_ENABLE_OPENGL   "Enable OpenGL rendering backend"  ON)
if(APPLE)
    option(NEXUS_ENABLE_METAL "Enable Metal rendering backend" ON)
else()
    option(NEXUS_ENABLE_METAL "Enable Metal rendering backend" OFF)
endif()

# ----------------------------------------------------------------------------
# Real SDK detection — release standard forbids CPU-simulated backends.
# Vulkan / Metal backends are compiled ONLY when a real SDK/framework is
# available on this machine.  Otherwise the corresponding backend is
# compiled out entirely; RHI::create(Backend::X) returns nullptr and callers
# fall back to OpenGL.
# ----------------------------------------------------------------------------

if(NEXUS_ENABLE_VULKAN)
    find_package(Vulkan 1.3 QUIET COMPONENTS glslang)
    if(NOT Vulkan_FOUND)
        find_package(Vulkan QUIET)
    endif()
    if(Vulkan_FOUND)
        message(STATUS "Vulkan SDK found: ${Vulkan_LIBRARY} (version ${Vulkan_VERSION})")
        set(NEXUS_HAVE_VULKAN_SDK ON)
    else()
        message(STATUS "Vulkan SDK not found — Vulkan backend will be excluded")
        set(NEXUS_ENABLE_VULKAN OFF CACHE BOOL "Enable Vulkan rendering backend" FORCE)
        set(NEXUS_HAVE_VULKAN_SDK OFF)
    endif()

    # glslang for runtime GLSL→SPIR-V compilation.
    find_library(NEXUS_GLSLANG_LIB      NAMES glslang)
    find_library(NEXUS_GLSLANG_SPV_LIB  NAMES SPIRV)
    find_library(NEXUS_GLSLANG_MGR_LIB  NAMES glslang-default-resource-limits)
    find_path(NEXUS_GLSLANG_INCLUDE_DIR glslang/Public/ShaderLang.h)
    if(NEXUS_GLSLANG_LIB AND NEXUS_GLSLANG_SPV_LIB AND NEXUS_GLSLANG_INCLUDE_DIR)
        message(STATUS "glslang found: ${NEXUS_GLSLANG_LIB}")
        set(NEXUS_HAVE_GLSLANG ON)
    else()
        message(STATUS "glslang not found — SPIR-V runtime compile disabled")
        set(NEXUS_HAVE_GLSLANG OFF)
    endif()
endif()

if(NEXUS_ENABLE_METAL AND APPLE)
    find_library(NEXUS_METAL_FRAMEWORK        Metal)
    find_library(NEXUS_QUARTZCORE_FRAMEWORK   QuartzCore)
    find_library(NEXUS_FOUNDATION_FRAMEWORK   Foundation)
    if(NEXUS_METAL_FRAMEWORK AND NEXUS_QUARTZCORE_FRAMEWORK AND NEXUS_FOUNDATION_FRAMEWORK)
        message(STATUS "Metal frameworks found")
        set(NEXUS_HAVE_METAL ON)
    else()
        message(STATUS "Metal frameworks not found — Metal backend excluded")
        set(NEXUS_ENABLE_METAL OFF CACHE BOOL "Enable Metal rendering backend" FORCE)
        set(NEXUS_HAVE_METAL OFF)
    endif()
elseif(NEXUS_ENABLE_METAL)
    message(STATUS "Metal backend requires APPLE — disabling")
    set(NEXUS_ENABLE_METAL OFF CACHE BOOL "Enable Metal rendering backend" FORCE)
    set(NEXUS_HAVE_METAL OFF)
endif()


# Build type defaults
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "Profile")
endif()

# CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(cmake/CompilerFlags.cmake)

# Platform-specific overlays
option(NEXUS_ENABLE_WEBGL  "Enable WebGL 2.0 rendering backend" OFF)
option(NEXUS_ENABLE_GLES   "Enable OpenGL ES 3.0 backend"       OFF)

if(EMSCRIPTEN)
    include(cmake/Emscripten.cmake)
endif()
if(ANDROID)
    include(cmake/Android.cmake)
endif()

include(cmake/Dependencies.cmake)

# Engine modules (order matters: dependencies first)
add_subdirectory(engine/core)
add_subdirectory(engine/platform)
add_subdirectory(engine/rhi)
add_subdirectory(engine/scene)
add_subdirectory(engine/animation)
add_subdirectory(engine/renderer)
add_subdirectory(engine/physics)
add_subdirectory(engine/audio)
add_subdirectory(engine/ui)
add_subdirectory(engine/scripting)
add_subdirectory(engine/assets)
add_subdirectory(engine/network)
add_subdirectory(engine/perf)

# Runtime
add_subdirectory(runtime)

# Editor library (always built for tests)
add_subdirectory(editor)

# Editor executable (optional)
if(NEXUS_BUILD_EDITOR)
    add_subdirectory(editor/app)
endif()

if(NEXUS_BUILD_SANDBOX)
    add_subdirectory(sandbox)
endif()

if(NEXUS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
