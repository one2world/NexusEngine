cmake_minimum_required(VERSION 3.21)

project(NexusEngine
    VERSION 0.1.0
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

# Build type defaults
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "Profile")
endif()

# CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(cmake/CompilerFlags.cmake)
include(cmake/Dependencies.cmake)

# Engine modules (order matters: dependencies first)
add_subdirectory(engine/core)
add_subdirectory(engine/platform)
add_subdirectory(engine/rhi)
add_subdirectory(engine/renderer)
add_subdirectory(engine/scene)
add_subdirectory(engine/physics)
add_subdirectory(engine/audio)
add_subdirectory(engine/animation)
add_subdirectory(engine/ui)
add_subdirectory(engine/scripting)
add_subdirectory(engine/assets)
add_subdirectory(engine/network)

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
