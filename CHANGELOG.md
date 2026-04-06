# Changelog

All notable changes to NexusEngine will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
