# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

NexusEngine — a C++20 hybrid 2D + 3D game engine. See `README.md` for feature overview and `ROADMAP.md` for subsystem readiness scores and outstanding work. `docs/architecture.md` is the authoritative architecture reference.

## Build & Test

All builds use CMake 3.21+ presets defined in `CMakePresets.json`. There is **no makefile shortcut** — use presets directly.

```bash
# Desktop Debug (default) — builds engine + editor + sandbox + tests
cmake --preset default
cmake --build build --parallel

# Desktop Release
cmake --preset release && cmake --build build-release --parallel

# CI preset (RelWithDebInfo, no sandbox)
cmake --preset ci && cmake --build build-ci --parallel

# Web (Emscripten, requires EMSDK)
cmake --preset web && cmake --build build-web --parallel

# Android (requires ANDROID_NDK)
cmake --preset android-arm64 && cmake --build build-android-arm64 --parallel
```

### Running tests

```bash
ctest --test-dir build --output-on-failure           # all tests
ctest --test-dir build -R "ECS" --output-on-failure  # one subsystem (regex)
ctest --test-dir build -R "StressTest"               # stress suite
```

Test targets live under `tests/<subsystem>/` and are registered in `tests/CMakeLists.txt`. The full suite is ~984 GoogleTest cases across core, scene, rhi, renderer, physics, audio, animation, ui, scripting, assets, network, editor, perf, platform, integration.

### Executables

After a `default` build:
- `build/sandbox/nexus-sandbox` — demo app
- `build/editor/app/nexus-editor` — ImGui editor (requires `NEXUS_BUILD_EDITOR=ON`)

### Lint / format

`.clang-format` (Google base, 4-space indent, 100-col, `PointerAlignment: Left`) and `.clang-tidy` are configured. Identifier naming enforced by clang-tidy: `CamelCase` types/enums, `lower_case` functions/vars/namespaces, private members with trailing `_`.

## High-Level Architecture

Engine modules are added to CMake in dependency order (see `CMakeLists.txt`): `core → platform → rhi → scene → animation → renderer → physics → audio → ui → scripting → assets → network → perf`. When adding a new module, place it in the order that matches its dependencies.

### Module layout convention

Every engine module follows:
```
engine/<module>/
  CMakeLists.txt
  include/nexus/<module>/*.h   # public headers
  src/*.cpp                    # implementation
```
Public headers are included as `#include <nexus/<module>/foo.h>`. All code lives under namespace `nexus` (subsystems use nested namespaces like `nexus::assets`, `nexus::audio`).

### Key subsystem boundaries

- **`core`** — primitives every module can depend on: `types.h` (aliases like `u32`, `u64`), GLM-backed `math.h`, `log.h` (spdlog), `event.h`, `job_system.h`, `memory.h`, `string_id.h`, `timer.h`. No dependencies on other engine modules.
- **`rhi`** — backend-agnostic `RHI` interface with OpenGL 4.5, Vulkan, and WebGL2 backends (`gl_rhi.h`, `vk_rhi.h`, `webgl_rhi.h`). **Note**: ROADMAP flags Vulkan as a CPU-simulated abstraction, not a real driver — treat `vk_rhi` as non-production. Renderers depend on the RHI interface, never on a backend directly.
- **`scene`** — sparse-set ECS. `Entity` is a `u64` handle (index + generation). `Registry` owns per-component sparse sets. Hierarchy/transforms propagate via the hierarchy system. Use `registry.add_component<T>(entity, ...)` / `get_component<T>` / component views.
- **`renderer`** — multiple renderers coexist: Forward 3D, Deferred, PBR + IBL, Batch Renderer 2D, Mobile (GLES3). Shadow pipeline is CSM + point-light cubemap with PCF/PCSS. Post-processing and lightmap baker live here.
- **`physics`** — custom 2D and 3D engines (not Box2D/Jolt). Spatial-hash broadphase, SAT/AABB/sphere colliders, joints (distance/hinge/ball), raycasting, body sleeping.
- **`audio`** — custom mixer (`AudioEngine`) with bus hierarchy, DSP effects (Schroeder reverb, filters), 3D spatialization + Doppler, reverb zones. Device output is routed through **miniaudio** (vendored via FetchContent). WAV decoded in-house, OGG Vorbis via `stb_vorbis`.
- **`scripting`** — Lua-like interpreter + C++ bindings, with a standard library (math/string/table/os) and coroutines. Not upstream Lua.
- **`assets`** — `AssetRegistry` with handle + refcount, async loader, hot-reload file watcher, `.pak` archive format, importers for BMP/TGA/PPM/OBJ/WAV/glTF 2.0 (JSON + GLB). Web build adds streaming via manifest + chunked download + LRU cache.
- **`editor`** — built as a library (always) plus optional `editor/app` executable. Panels are gated on `NEXUS_HAS_IMGUI`; ImGui is vendored and built as a static library in `cmake/Dependencies.cmake` (no backends — the app provides the GL/GLFW backends and must define `GLFW_INCLUDE_NONE` before including them to avoid GL loader conflicts).

### Dependency strategy

Third-party libraries are fetched via `FetchContent` in `cmake/Dependencies.cmake`: spdlog 1.13, GLM 1.0.1, GLFW 3.4, nlohmann/json 3.11.3, stb (master), Dear ImGui 1.91.8, miniaudio 0.11.21, GoogleTest 1.14.0. **Do not switch to system-installed copies** — presets assume FetchContent.

### Cross-platform notes

- Emscripten and Android builds disable the editor, tests, OpenGL, and Vulkan; they enable `NEXUS_ENABLE_WEBGL` or `NEXUS_ENABLE_GLES` instead. See `cmake/Emscripten.cmake` and `cmake/Android.cmake`.
- Crash handler and main-loop abstraction are cross-platform (`engine/core/src/crash_handler.cpp`, Emscripten main-loop integration in the platform layer).

### Compile-time flags

- `NEXUS_HAS_IMGUI` — ImGui editor panels (set automatically when ImGui is available)
- `NEXUS_DEBUG` / `NEXUS_PROFILE` — assertions and profiling instrumentation
- `NEXUS_PLATFORM_WEB` / `NEXUS_PLATFORM_ANDROID` — platform overlays set by presets

## Conventions specific to this repo

- **Many small files, <800 lines per file.** Engine subsystems split by responsibility (e.g. `forward_renderer_3d.h`, `deferred_renderer.h`, `pbr_renderer.h` are separate files, not one mega-header).
- **No raw `new`/`delete`** — use `std::unique_ptr`/`std::make_unique`. Engine uses handle-based types (`ShaderHandle`, `BufferHandle`, `AssetHandle`) rather than exposing raw pointers across module boundaries.
- **Handles, not pointers, cross subsystem boundaries** (RHI, assets, ECS all use opaque integer/struct handles).
- **Public API uses `nexus::` namespace**; subsystem internals use nested namespaces.
- When touching the renderer, confirm which renderer path (Forward/Deferred/PBR/Batch2D/Mobile) you're modifying — they coexist and have different material expectations.
- When adding a new engine module, add it to the root `CMakeLists.txt` in the correct dependency position, and add a matching `tests/<module>/` target to `tests/CMakeLists.txt`.

## Docs

- `docs/architecture.md` — subsystem-by-subsystem architecture reference
- `docs/getting_started.md` — ECS / renderer / audio / input usage examples
- `ROADMAP.md` — per-subsystem readiness scores and known gaps (Vulkan, threading, editor completeness)
- `CHANGELOG.md` — versioned release notes
