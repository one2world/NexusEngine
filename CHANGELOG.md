# Changelog

All notable changes to NexusEngine will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.2] - 2026-04-10

### Added
- **Audio Device Output**: Cross-platform audio output via miniaudio. Callback-based device opens the platform's default playback device (ALSA, PulseAudio, WASAPI, CoreAudio, WebAudio) and feeds samples from the custom mixer. AudioDevice wired into both runtime and editor entry points.
- **Dear ImGui Integration**: Added Dear ImGui 1.91 as a real FetchContent dependency. Editor panels now render with ImGui when `NEXUS_HAS_IMGUI` is defined (enabled by default for editor builds).
- **Real OGG Vorbis Decoding**: Replaced the previous stub Vorbis decoder (which generated synthetic noise from packet energy estimation) with stb_vorbis — a battle-tested single-header decoder that produces correct PCM output.
- Total test count: **984**, all passing.

### Fixed
- **AudioDevice wiring**: AudioDevice is now instantiated and opened in `runtime_main.cpp` and `editor_main.cpp`, so audio actually plays through speakers instead of being silently mixed into nowhere.
- **ImGui GL loader conflict**: Fixed build failure where GLFW's default `<GL/gl.h>` include defined `GL_VERSION_1_0`/`GL_VERSION_1_1`, causing ImGui's built-in GL loader (`imgui_impl_opengl3_loader.h`) to skip its own GL 1.0/1.1 typedefs. Fix: define `GLFW_INCLUDE_NONE` before compiling ImGui GLFW backend.
- **README.md**: Removed false claims about Box2D, Jolt Physics, and miniaudio. Corrected 2D physics claim (no constraints — those are 3D-only). Now honestly documents all custom implementations.
- **ROADMAP.md**: Updated technology stack table with "Originally Planned" vs "Actual Implementation" columns.

## [1.0.1] - 2026-04-10

### Added
- **Web Asset Streaming**: Complete streaming manifest system with JSON serialization, chunked download pipeline, LRU cache with eviction, progress tracking, ready callbacks, and manifest generation from PAK packages. Desktop simulation for testing, Emscripten hooks ready.
- **Lua Standard Library**: 40+ standard library functions across four modules: math (abs, floor, ceil, sqrt, sin, cos, tan, asin, acos, atan, exp, log, pow, fmod, max, min, random, randomseed + pi/huge constants), string (len, sub, upper, lower, rep, reverse, byte, char, find, format), table (insert, remove, concat, sort, getn, keys), os (clock, time).
- **Integration Tests**: 8 end-to-end cross-subsystem pipeline tests covering ECS+serialization round-trip, ECS+scripting entity creation, asset registry+loader pipeline, animation clip sampling, physics simulation stepping, network serialization/RPC round-trip, full init-simulate-shutdown lifecycle, and delta compression integrity.
- Total test count increased from 932 to **975**, all passing.

### Fixed
- GCC 13 `-Werror` build compatibility (suppressed spurious `-Warray-bounds`, `-Wstringop-overflow`, `-Wmaybe-uninitialized` in third-party headers).

## [1.0.0] - 2026-04-06

### Added
- **ECS / Scene Graph**: Sparse-set ECS with generation-based handles, hierarchy system, transform propagation, prefab system, scene serialization (JSON + binary).
- **Rendering**: Forward and deferred renderers, PBR material system (metallic-roughness, glTF-compatible), IBL (irradiance, prefiltered specular, BRDF LUT), cascaded shadow maps, PCSS soft shadows, SSAO, post-processing (Bloom, FXAA, TAA, tonemapping ACES, vignette), heightmap terrain with LOD, skybox, GPU particles, decal system, lightmap baker, mobile renderer.
- **RHI**: OpenGL 4.5 backend with full resource abstraction; WebGL 2.0 backend (Emscripten).
- **2D Rendering**: Batch renderer with auto-batching, frustum culling, tilemap rendering, glyph atlas text rendering.
- **Physics**: Custom 2D and 3D physics with spatial hash broadphase, constraints (distance, hinge with angle limits), raycasting, body sleeping, debug draw.
- **Audio**: Spatial 3D audio, bus system, DSP effects (reverb, low/high-pass filters), WAV and OGG Vorbis decoding, streaming playback, Doppler effect, reverb zones.
- **Animation**: Skeleton system (bone hierarchy, skin matrices), IK solver, animation clips, blend trees (1D/2D/additive), state machine, sprite animation, tween system, CPU particle system.
- **Scripting**: Script engine with C++ function registration, Lua-like interpreter backend (loops, tables, control flow, coroutines, hot-reload). Live bindings for Input, Audio, and Physics subsystems.
- **UI**: Retained-mode widget system with canvas-based draw commands.
- **Assets**: Asset registry with async loading, hot-reload, PAK packaging. Importers for BMP, TGA, PPM, OBJ, glTF/GLB, WAV, OGG. Shader cache with binary program caching.
- **Networking**: RPC framework, UDP transport with reliable delivery, entity replication with interpolation, delta compression, serialization.
- **Editor**: ImGui-based editor with viewport (gizmo), hierarchy, inspector, console, asset browser panels. Undo/redo system, play/pause/stop state management.
- **Profiling**: CPU frame timing, GPU timestamps, scoped profiling, memory profiler UI, frame graph (DAG scheduling + culling), performance regression test suite.
- **Platform**: GLFW windowing (Windows, Linux, macOS), touch input abstraction, gamepad support, action mapping, cross-platform crash handler. Build/export pipeline with per-platform profiles. Emscripten main loop support. Android NDK bridge.
- **Build System**: CMake 3.21+ with presets (default, release, ci, web, android-arm64), FetchContent dependencies, compiler warnings-as-errors, AddressSanitizer + UBSan in Debug.
- **Testing**: 932 unit tests across 14 subsystems using Google Test, including stress tests and performance regression suite.
- **CI/CD**: GitHub Actions workflow for Linux (GCC 13), Windows (MSVC), and macOS (Clang).
- **Examples**: Platformer 2D, FPS 3D, and top-down RPG example projects.

### Known Limitations
- Vulkan backend is a CPU-simulated abstraction, not a real driver implementation.
- Texture compression in the asset cooker is pass-through (no BCn/ASTC/ETC2 encoding yet).
- FBX mesh import is not supported; use glTF/GLB or OBJ instead.
- Physics subsystem is functional but not yet performance-tuned for large-scale simulations.
- Android platform bridge is a stub on non-Android builds.
