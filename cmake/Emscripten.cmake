# ============================================================================
# Emscripten CMake toolchain overlay for NexusEngine
#
# This file is included (not used as a toolchain file) when building for
# Emscripten. The actual toolchain is Emscripten's built-in
# $EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake.
#
# Usage:
#   emcmake cmake --preset web -S . -B build-web
#
# This overlay sets NexusEngine-specific options for web builds.
# ============================================================================

message(STATUS "NexusEngine: Configuring for Emscripten/WebGL 2.0")

# Disable features not available on the web
set(NEXUS_ENABLE_VULKAN OFF CACHE BOOL "" FORCE)
set(NEXUS_ENABLE_OPENGL OFF CACHE BOOL "" FORCE)
set(NEXUS_ENABLE_WEBGL  ON  CACHE BOOL "" FORCE)
set(NEXUS_BUILD_EDITOR  OFF CACHE BOOL "" FORCE)

# Emscripten linker flags for WebGL 2.0
set(NEXUS_WEB_LINKER_FLAGS
    "-s USE_WEBGL2=1"
    "-s FULL_ES3=1"
    "-s ALLOW_MEMORY_GROWTH=1"
    "-s INITIAL_MEMORY=268435456"   # 256 MB
    "-s MAXIMUM_MEMORY=536870912"   # 512 MB
    "-s WASM=1"
    "-s ASYNCIFY=1"
    "-s EXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
    "-s MIN_WEBGL_VERSION=2"
    "-s MAX_WEBGL_VERSION=2"
)

string(REPLACE ";" " " NEXUS_WEB_LINKER_FLAGS_STR "${NEXUS_WEB_LINKER_FLAGS}")

# Apply to all Emscripten targets via a helper function
function(nexus_emscripten_target TARGET)
    if(EMSCRIPTEN)
        target_link_options(${TARGET} PRIVATE ${NEXUS_WEB_LINKER_FLAGS})
        target_compile_definitions(${TARGET} PRIVATE NEXUS_PLATFORM_WEB=1)
    endif()
endfunction()
