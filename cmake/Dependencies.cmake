include(FetchContent)

# spdlog — Fast logging library
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.13.0
    GIT_SHALLOW    TRUE
)

# GLM — OpenGL Mathematics
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)

# GLFW — Window and input
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# nlohmann/json — JSON serialization
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)

# stb — Single-header libraries (stb_vorbis for OGG Vorbis decoding)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)

# Dear ImGui — Immediate-mode GUI for editor panels.
# Uses the `docking` branch so the editor can dock/dragd/float panels like
# Unity / Unreal.  Pinned to a known-good commit on docking for repro.
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.8-docking
    GIT_SHALLOW    TRUE
)

# miniaudio — Cross-platform audio device output (single-header)
FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG        0.11.21
    GIT_SHALLOW    TRUE
)

# ImGuizmo — Translate/rotate/scale 3D gizmo built on top of Dear ImGui.
# The 1.83 tag predates ImGui 1.89 / 1.91 (which removed
# `CaptureMouseFromApp` and now requires consumers to opt into the math
# operator overloads via `IMGUI_DEFINE_MATH_OPERATORS`).  Track the upstream
# `master` branch which has both fixes.  Header + single .cpp.
FetchContent_Declare(
    imguizmo
    GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
    GIT_TAG        master
)

# Lua 5.4 — embedded scripting language.  Pulled from the official PUC-Rio
# tarball mirror on GitHub.  Built as a STATIC C library (lua-c, no
# stand-alone interpreter) so the engine links one ~250 KB blob and gets
# the full Lua reference semantics + stdlib (basic + math + string + table +
# os + coroutine + io) for free.
#
# We pin to 5.4.7 (latest stable as of Q2 2026) and build manually because
# upstream ships only Makefiles, not CMake.  All 25 .c files compile clean
# under -std=c11 with -DLUA_USE_POSIX on macOS / Linux (POSIX features:
# tmpfile / popen used by the os and io libraries).
FetchContent_Declare(
    lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG        v5.4.7
    GIT_SHALLOW    TRUE
)

# Google Test — Unit testing.  Version kept in sync with Homebrew so ABI matches
# when Homebrew's gtest header is transitively picked up via Vulkan SDK include
# paths (e.g. /opt/homebrew/include).
if(NEXUS_BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.17.0
        GIT_SHALLOW    TRUE
    )
endif()

# Make available
FetchContent_MakeAvailable(spdlog glm glfw json stb imgui miniaudio)

# Lua: vendor source has no CMake — populate and build manually as a static
# library.  We exclude `lua.c` and `luac.c` (the stand-alone interpreter and
# bytecode compiler entry points); only the library .c files get compiled
# into libnexus-lua.
FetchContent_GetProperties(lua)
if(NOT lua_POPULATED)
    FetchContent_Populate(lua)
endif()
if(lua_POPULATED)
    file(GLOB _lua_sources "${lua_SOURCE_DIR}/*.c")
    list(REMOVE_ITEM _lua_sources
        "${lua_SOURCE_DIR}/lua.c"        # stand-alone interpreter — excluded
        "${lua_SOURCE_DIR}/luac.c"       # bytecode compiler tool — excluded
        "${lua_SOURCE_DIR}/onelua.c")    # amalgam alternative — excluded
    add_library(lua STATIC ${_lua_sources})
    target_include_directories(lua PUBLIC "${lua_SOURCE_DIR}")
    # Lua's luaconf.h auto-defines LUA_USE_POSIX when LUA_USE_MACOSX or
    # LUA_USE_LINUX is set, so we only set the platform-specific flag here.
    if(APPLE)
        target_compile_definitions(lua PUBLIC LUA_USE_MACOSX)
    elseif(UNIX)
        target_compile_definitions(lua PUBLIC LUA_USE_LINUX)
    endif()
    # Lua needs C99 minimum; suppress third-party warnings.
    set_target_properties(lua PROPERTIES C_STANDARD 99)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lua PRIVATE -w)
    elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(lua PRIVATE /w)
    endif()
endif()

# imguizmo: bypass add_subdirectory so upstream's own CMakeLists (which
# lacks an imgui include path) doesn't run.  We build the single .cpp
# manually below — the static target ends up consumer-equivalent without
# requiring the upstream CMake to evolve compatibly.
FetchContent_GetProperties(imguizmo)
if(NOT imguizmo_POPULATED)
    FetchContent_Populate(imguizmo)
endif()

# spdlog 1.13's bundled fmt uses `consteval` format-string validation that
# trips a Clang 17+ "not a constant expression" diagnostic when compile-time
# format-string parsing meets certain int-pad helpers.  Empty-defining
# FMT_CONSTEVAL falls back to runtime validation, restoring clean builds
# without touching spdlog's logging API or runtime format checking.
if(TARGET spdlog)
    target_compile_definitions(spdlog PUBLIC FMT_CONSTEVAL=)
endif()

# Dear ImGui doesn't have a CMakeLists.txt — build as a static library manually
if(imgui_POPULATED)
    # ImGui core library (no backends — those need GL context from the app)
    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    )
    target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    # Suppress warnings from third-party code
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(imgui PRIVATE -w)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(imgui PRIVATE /w)
    endif()
endif()

# ImGuizmo — single .cpp/.h static library linked against our `imgui` target.
# IMGUI_DEFINE_MATH_OPERATORS is propagated PUBLICly: ImGuizmo.cpp uses
# `ImVec2 - ImVec2` etc., and any consumer that includes <ImGuizmo.h> needs
# the same operators visible to compile.
#
# Upstream imguizmo gained its own CMakeLists in 2026, so on a fresh fetch
# the FetchContent_MakeAvailable() above already creates an `imguizmo`
# target via add_subdirectory.  Only build our manual target when the
# upstream didn't (older snapshots / forks).  If upstream did, just
# re-apply the math-operators define so both code paths stay equivalent.
if(imguizmo_POPULATED)
    # Upstream now stores ImGuizmo.cpp under src/; older snapshots had it at
    # the repo root.  Pick whichever exists so we work against either layout.
    if(EXISTS "${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp")
        set(_imguizmo_src "${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp")
        set(_imguizmo_inc "${imguizmo_SOURCE_DIR}/src")
    else()
        set(_imguizmo_src "${imguizmo_SOURCE_DIR}/ImGuizmo.cpp")
        set(_imguizmo_inc "${imguizmo_SOURCE_DIR}")
    endif()
    add_library(imguizmo STATIC "${_imguizmo_src}")
    target_include_directories(imguizmo PUBLIC "${_imguizmo_inc}")
    target_compile_definitions(imguizmo PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
    target_link_libraries(imguizmo PUBLIC imgui)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(imguizmo PRIVATE -w)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(imguizmo PRIVATE /w)
    endif()
endif()

if(NEXUS_BUILD_TESTS)
    FetchContent_MakeAvailable(googletest)
endif()
