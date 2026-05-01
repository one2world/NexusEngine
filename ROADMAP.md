# NexusEngine Roadmap

> **NexusEngine** — A modern 2D + 3D hybrid game engine built with C++20, designed for flexibility, performance, and ease of use.
>
> **Last audit date:** 2026-04-17 | **Release standard:** every item below must ship as a real, production implementation. CPU simulation, fallback shims, type-only stubs, and "planned" placeholders are **not** acceptable substitutes for shipped functionality.

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                      Editor / Tools (ImGui)                  │
├─────────────────────────────────────────────────────────────┤
│              Scripting Layer (Lua-like Interpreter)            │
├──────────────────────┬──────────────────────────────────────┤
│   2D Subsystem       │         3D Subsystem                  │
│  ┌────────────────┐  │  ┌────────────────────────────────┐  │
│  │ Sprite Renderer │  │  │ Forward / Deferred Renderer    │  │
│  │ Tilemap Engine  │  │  │ PBR Material System + IBL      │  │
│  │ 2D Physics      │  │  │ Skeletal Animation             │  │
│  │ 2D Particles    │  │  │ 3D Physics (Custom)            │  │
│  │ Spine/Anim      │  │  │ 3D Particles                   │  │
│  └────────────────┘  │  └────────────────────────────────┘  │
├──────────────────────┴──────────────────────────────────────┤
│                    Scene Graph (Sparse-Set ECS)               │
├─────────────────────────────────────────────────────────────┤
│  Audio │ Input │ Networking │ UI System │ Asset Pipeline     │
├─────────────────────────────────────────────────────────────┤
│           Platform Abstraction Layer (GLFW)                   │
│              Windows │ Linux │ macOS                          │
├─────────────────────────────────────────────────────────────┤
│        Rendering Backend (OpenGL 4.5 │ Vulkan 1.3 │ WebGL 2) │
└─────────────────────────────────────────────────────────────┘
```

---

## Current State Summary (as of 2026-04-17)

### Subsystem Readiness Scores

> Scores reflect **shipped, production-grade** functionality. Anything implemented via CPU simulation,
> fallback path, or stub does **not** count toward the score.

| Subsystem | Score | Status |
|-----------|-------|--------|
| ECS / Scene Graph | 94/100 | Production-grade sparse-set, hierarchy, prefabs with hierarchy preservation, stress-tested 10K entities |
| Audio | 93/100 | Spatial 3D, bus system, DSP effects, WAV+OGG (Vorbis decode), streaming, Doppler, reverb zones, miniaudio device output |
| Rendering (Core) | 80/100 | PBR+IBL, CSM+PCSS shadows, deferred with light culling, TAA, skybox, GPU skinning, light probes, decals, lightmap baker, mobile renderer (OpenGL 4.5 only — Vulkan 1.3 and WebGL 2.0 native drivers required for full score) |
| Testing | 98/100 | 984 tests, all subsystems covered, stress tests, profiler tests, perf regression, RHI tests, integration tests |
| Threading | 70/100 | Job system, render thread pool, parallel_for |
| Asset Pipeline | 90/100 | Async load, hot-reload, PAK, BMP/TGA/PPM/OBJ/WAV/glTF importers, shader cache, web asset streaming (manifest, chunked download, LRU cache) |
| Editor | 72/100 | Full panel architecture, undo/redo, ImGui rendering for all panels |
| Serialization | 70/100 | JSON + binary (.nxs) scene formats with schema versioning |
| Physics | 45/100 | Custom 2D+3D with spatial hash, hinge joint world-space axis + angle limits |
| Platform | 65/100 | Desktop GLFW, touch input, build/export, Emscripten main loop, Android NDK bridge, CMakePresets, cross-platform crash handler (iOS/Metal pipeline required for full score) |
| Networking | 72/100 | RPC + replication + UDP transport with reliable delivery, default interpolation, delta compression |
| Scripting | 75/100 | C++ API + Lua-like interpreter backend with loops, tables, control flow, standard library (math, string, table, os) |
| Profiling | 80/100 | Memory profiler UI, GPU profiler, frame graph (DAG scheduling + culling), perf regression suite (GPU-compute particle simulation required for full score) |

**Overall readiness: 82/100.** The remaining 18 points are tied to the strict release requirements
in the "Release-Blocking Requirements" section below — none of those items may be checked off
using CPU simulation or stubbed code.

### What's Implemented (Done)

- [x] CMake build system with presets, FetchContent, sanitizers
- [x] spdlog logging, GLM math, high-res timer, event bus
- [x] Job system (thread pool + parallel_for) and render thread pool
- [x] GLFW window + input (keyboard, mouse, gamepad action mapping)
- [x] OpenGL 4.5 RHI with full resource abstraction
- [ ] Vulkan 1.3 RHI — real `VkInstance` / `VkDevice` / `VkQueue` driver path (current `vk_rhi` is a header-only abstraction and does not call into a Vulkan loader; release-blocking)
- [x] Batch Renderer 2D (auto-batching, frustum culling, tilemap, glyph)
- [x] Forward Renderer 3D (multi-light, shadow integration)
- [x] Deferred Renderer (G-buffer, light volumes)
- [x] PBR material system (metallic-roughness, glTF-compatible)
- [x] IBL (irradiance map, prefiltered specular, BRDF LUT)
- [x] Cascaded Shadow Maps + point light cubemap shadows
- [x] SSAO (configurable kernel, blur pass)
- [x] Post-processing stack (Bloom, FXAA, Tonemapping ACES, Vignette)
- [x] Heightmap terrain with LOD + splatmap (4 layers)
- [x] Sparse-set ECS with generation-based handles, lifecycle callbacks
- [x] Hierarchy system with transform propagation (2D + 3D)
- [x] Scene serialization (JSON) with transactional rollback
- [x] Scene manager (multi-scene, additive loading)
- [x] Prefab system (serialize/instantiate)
- [x] Custom 2D physics (spatial hash, constraints, raycast, debug draw)
- [x] Custom 3D physics (spatial hash, constraints, raycast, body sleeping)
- [x] Spatial audio, audio bus system, DSP effects (reverb, filters)
- [x] Skeleton system (bone hierarchy, skin matrices, IK solver)
- [x] Animation clips, blend trees (1D/2D/additive), state machine
- [x] Sprite animation, tween system, CPU particle system
- [x] Retained-mode UI with widget tree and draw commands
- [x] Bitmap font text rendering via glyph atlas
- [x] Script engine C++ framework (functions, coroutines, hot-reload)
- [x] Asset registry, async loading, hot-reload, PAK packaging
- [x] Network RPC framework, serialization, replication/prediction stubs
- [x] Editor panels (viewport+gizmo, hierarchy, inspector, console, asset browser)
- [x] Editor undo/redo, play/pause/stop state management
- [x] Profiler (CPU frame timing, GPU timestamps, scoped profiling)
- [x] LOD system, occlusion culling, instanced rendering
- [x] Debug renderer (lines, boxes, spheres, grids, frustums)
- [x] Debug overlay (FPS, frame time, draw calls)
- [x] Game state save/load system
- [x] Tilemap component + serialization
- [x] 932 unit tests across all subsystems, all passing
- [x] Lua-like scripting backend (evaluator with if/then/end, variables, functions)
- [x] Asset importers (BMP, TGA, PPM/PGM textures, OBJ meshes, WAV audio)
- [x] UDP network transport (handshake, reliable delivery, ACK, timeout)
- [x] TAA anti-aliasing (Halton jitter, temporal reprojection, neighborhood clamping)
- [x] Skybox renderer (cubemap + Preetham procedural sky + reflection probes)
- [x] Binary scene serialization (.nxs format, schema versioning, forward-compatible)
- [x] GPU skinning (4-bone vertex shader, SkinnedMeshRenderer)
- [x] Shader library (hot-reload, #include resolution, variant/define system, binary cache)
- [x] PCF + PCSS soft shadows (16-tap Poisson, 3x3 PCF, cubemap PCF, contact-hardening PCSS)
- [x] Editor panel ImGui rendering (hierarchy tree, inspector properties, console log, asset browser)
- [x] Light probes (spherical harmonics, grid-based, trilinear interpolation)
- [x] GPU particle system (100K+ capacity, billboard rendering, emission/simulation)
- [x] Deferred decal system (depth projection, edge fade, layer sorting)
- [x] Audio streaming (WAV/OGG chunk-based playback)
- [x] Doppler effect (velocity-based pitch shifting for spatial audio)
- [x] glTF 2.0 importer (JSON+GLB, accessors, mesh/material/animation/skin extraction)
- [x] Shader cache (source-level caching with platform-aware hashing)
- [x] Lightmap baker (CPU hemisphere sampling, Möller–Trumbore intersection, tonemap)
- [x] Reverb zones (AABB/sphere spatial zones, fade blending, priority system)
- [x] Memory profiler UI (per-tag stats, formatted display, history ring buffer)
- [x] GPU profiler (per-pass timing, running averages, peak tracking, frame history)
- [x] Touch input system (multi-touch, gestures: tap, pan, pinch, rotation, swipe)
- [x] Mobile renderer (GLES3 shaders, simplified PBR, quality tiers, up to 4 lights)
- [x] Stress tests (10K entity creation/destruction, component iteration performance)
- [x] Build/export pipeline (platform profiles, asset cooker, step-based build)
- [x] Frame graph (declarative pass scheduling, topological sort, dead-pass culling, timing)
- [x] Automated performance regression tests (entity ops, component iteration, hierarchy, vector math)
- [x] Doxygen configuration (auto-generated API reference)
- [x] Architecture documentation guide
- [x] Getting started tutorial
- [x] Example projects (2D Platformer, 3D FPS, Top-Down RPG)
- [ ] WebGL 2.0 RHI backend — real GLES3/WebGL2 driver path on Emscripten only; the desktop CPU-simulated path is a development scaffold and must be removed or hidden behind a non-shipping flag before release
- [x] Emscripten main loop integration (requestAnimationFrame, canvas management)
- [x] Android NDK native activity bridge (lifecycle, window, assets, touch forwarding)
- [~] Vulkan type definitions (opaque handles, queue families, swapchain config, frame sync) — scaffolding only; **does not** count as a Vulkan backend
- [x] CMakePresets.json (desktop debug/release, CI, web/Emscripten, Android ARM64/x86_64)
- [x] Web HTML shell template (loading screen, canvas resize, touch handling)
- [x] Platform CMake overlays (cmake/Emscripten.cmake, cmake/Android.cmake)
- [x] Cross-platform crash handler (POSIX signals, Windows SEH, Android NDK unwind, Emscripten, symbol demangling)
- [x] Lua scripting loops (while/for) and table constructors
- [x] Physics HingeJoint world-space axis transform, angular constraint, angle limits
- [x] Prefab instantiation with hierarchy preservation (parent-child relationships)
- [x] OGG Vorbis decoder (codebook parsing, setup header, packet decoding, overlap-add)
- [x] Deferred renderer CPU light volume culling (frustum test, distance sort, capped at 32)
- [x] Network default interpolation (float-aligned lerp) and delta snapshot compression
- [x] Web asset streaming (manifest JSON, chunked download, LRU cache, progress tracking, manifest generation from PAK)
- [x] Lua standard library (math, string, table, os modules — 40+ functions)
- [x] Integration tests (8 end-to-end pipeline tests: ECS+serialization, ECS+scripting, assets, animation, physics, network, full lifecycle)
- [x] Audio device output via miniaudio (callback-based, cross-platform speaker output)
- [x] 1027 unit/integration tests, all passing (adds 44 VulkanRHI + 9 MetalRHI covering real driver paths)

### Release-Blocking Requirements (must be real implementations)

The following items are **mandatory** for a 1.0 release. None may be marked complete using
CPU simulation, fallback paths, type-only stubs, or "planned" placeholders.

- [~] **Vulkan 1.3 driver** — real `VkInstance` / `VkPhysicalDevice` / `VkDevice` / `VkQueue` acquired through the Vulkan loader and validated against the Vulkan SDK (incl. MoltenVK portability); real `VkBuffer`+`VkDeviceMemory` and `VkImage`+`VkImageView`+`VkDeviceMemory` allocation; command pool + debug messenger in debug builds. **Remaining**: SPIR-V runtime compilation into `VkShaderModule` + full `VkPipeline` graphics object + VMA allocator + descriptor set management + swapchain present.
- [~] **iOS / Metal backend** — real `MTLDevice` / `MTLCommandQueue` driver, real `MTLBuffer` / `MTLTexture` / `MTLRenderPipelineState` / `MTLDepthStencilState` compilation, real `MTLCommandBuffer` + `MTLRenderCommandEncoder` encoding for draw / indexed-draw / set-viewport / clear / blend state. **Remaining**: `CAMetalLayer` swapchain binding, richer GLSL→MSL translation for the full renderer family, code signing through Xcode toolchain.
- [ ] **WebGL 2.0 native backend** — Emscripten build emits a real WebGL2 driver; remove the desktop CPU-simulated path from shippable artifacts.
- [ ] **GPU compute particle simulation** — real compute shader path on Vulkan/GLES 3.1; CPU emission path is dev-only.
- [x] **Web export** — Emscripten main loop, HTML shell, CMake preset, web asset streaming (Emscripten SDK required to build).
- [x] **Mobile runtime (Android)** — Android NDK bridge, touch input, mobile renderer, CMake presets (NDK required to build).

---

## Development Phases (Remaining Work)

### Phase A: Critical Blockers — Release-Blocking (P0)

> Without these, the engine cannot ship as a usable product.

#### A.1 Lua Scripting Backend ✓
- [x] Lua-like interpreter with evaluator (if/then/end, assignment, concatenation)
- [x] Function call support, variable scoping, string operations
- [x] Script component lifecycle integration
- [x] Standard library modules: math (20 functions + constants), string (10 functions), table (6 functions), os (2 functions)
- [ ] Full sol2/LuaJIT binding (stretch goal — current interpreter + stdlib is functional)

#### A.2 Asset Importers ✓
- [x] Texture import: BMP, TGA, PPM/PGM decoders (no stb_image dependency)
- [x] Mesh import: OBJ loader with vertex/index buffers
- [x] Audio import: WAV loader + OGG container parsing
- [x] glTF 2.0 import: nlohmann/json parser → vertex/index/material/skeleton/animation

#### A.3 Editor Panel Rendering (ImGui) ✓
- [x] InspectorPanel: component property editors (float, vec, color, checkbox)
- [x] HierarchyPanel: entity tree with drag-drop reparenting, context menu
- [x] ConsolePanel: scrollable log with level filtering, color coding
- [x] AssetBrowserPanel: grid view with icons, search, navigation breadcrumbs
- [x] ViewportPanel: 3D/2D rendering with camera and light integration

#### A.4 Network Transport Layer ✓
- [x] UDP socket abstraction (send/recv, non-blocking)
- [x] Connection handshake (SYN/ACK with timeout)
- [x] Reliable delivery (sequence numbers, ACK bitfield, retransmit)
- [x] Integration with existing RPC/replication framework

#### A.5 Binary Scene Serialization ✓
- [x] Binary format: header (magic "NXS\0", version, entity count) + packed components
- [x] Schema version field with forward-compatibility check
- [x] Backward-compatible reader (skip unknown component types)
- [x] Save/load API parallel to existing JSON interface

### Phase B: Visual Quality — Industry Standard (P1)

> Required to match 2026 visual expectations (Godot 5 / Unity 6 baseline).

#### B.1 Skybox & Environment ✓
- [x] Cubemap skybox renderer
- [x] Procedural sky (Preetham model with turbidity/sun direction)
- [x] Reflection probes (baked cubemap capture + parallax correction)

#### B.2 Temporal Anti-Aliasing (TAA) ✓
- [x] Per-frame sub-pixel jitter (Halton sequence)
- [x] Motion vector generation (per-object + camera)
- [x] Temporal reprojection with neighborhood clamping
- [x] History buffer management (double-buffered ping-pong)

#### B.3 Soft Shadows ✓
- [x] PCF (Percentage Closer Filtering) for CSM — 16-tap Poisson disk
- [x] 3x3 PCF kernel for low-cost shadow sampling tier (quality knob, not a fallback)
- [x] Point light soft shadow filtering (20-tap cubemap PCF)
- [x] Contact-hardening soft shadows (PCSS) — blocker search + variable penumbra PCF

#### B.4 GPU Skinning ✓
- [x] Upload bone matrices per draw (uniform-based)
- [x] Vertex shader: weighted bone transform (4 bones per vertex)
- [x] SkinnedMeshRenderer with SkinnedVertex format
- [x] Integration with animation system output

#### B.5 Shader System Improvements ✓
- [x] Shader hot-reload (file mtime comparison → recompile → rebind)
- [x] #include directive resolution for shader files (recursive, circular detection)
- [x] Shader variant/permutation system (#define injection after #version)
- [x] Shader cache (source-level binary caching, platform-aware hashing)

### Phase C: Competitive Features — Market Differentiation (P2)

> Features that differentiate from other indie engines.

#### C.1 Global Illumination — Partial ✓
- [x] Light probes (spherical harmonics L2, 9 coefficients per channel)
- [x] Light probe grid (3D placement, trilinear interpolation)
- [x] Probe-based GI sampling for dynamic objects
- [x] Lightmap baker (CPU hemisphere sampling, Möller–Trumbore intersection, tonemap)

#### C.2 GPU Particle System — Partial
- [x] Billboard rendering with camera-aligned quads
- [x] Additive/alpha blending, soft circle falloff
- [x] 100K+ particle capacity with dead-particle compaction
- [~] CPU-side emission + simulation (development scaffold; **not** acceptable for release)
- [ ] Real GPU compute simulation on Vulkan / GLES 3.1 — release-blocking

#### C.3 Advanced Audio ✓
- [x] OGG Vorbis decoder (container parsing, codebook/floor/residue setup, packet decode, overlap-add)
- [x] Streaming playback (WAV chunk-based, OGG memory-buffered)
- [x] Doppler effect (velocity-based pitch calculation)
- [x] Reverb zones (AABB/sphere spatial zones, fade blending, priority system)
- [x] Platform audio output via miniaudio (callback-based device, cross-platform)

#### C.4 Decal System ✓
- [x] Deferred decals (depth-buffer projection, inverse transform)
- [x] Albedo modification with edge fade
- [x] Layer-sorted rendering

#### C.5 Editor Specialized Tools ✓
- [x] Tilemap editor (paint, erase, fill, rectangle, pick, tile palette, grid)
- [x] Animation timeline editor (dopesheet/curve modes, keyframes, cubic Hermite interpolation)
- [x] Particle editor (real-time preview, presets, emitter property editing)
- [x] Material editor (property panel, save callback, live editing)

### Phase D: Platform & Distribution (P3)

> Expand beyond desktop.

#### D.1 Real Vulkan 1.3 Backend — Release-Blocking
- [~] Vulkan type definitions (opaque handles, queue families, swapchain config, frame sync, physical device info) — scaffolding only
- [ ] `VkInstance` / `VkPhysicalDevice` / `VkDevice` / `VkQueue` creation through the Vulkan loader
- [ ] VMA memory allocator integration
- [ ] Swapchain management with present queue + frame pacing
- [ ] Descriptor set / descriptor indexing management
- [ ] SPIR-V shader compilation pipeline (glslang or shaderc)
- [ ] Validation-layer-clean run on reference scenes

#### D.2 Mobile Platform Support — Partial
- [x] Android NDK native activity bridge (lifecycle, window, assets, input forwarding)
- [x] Android CMake toolchain overlay (API 26+, ARM64/x86_64 presets)
- [x] Touch input abstraction (multi-touch, tap/pan/pinch/rotation/swipe gestures)
- [x] Mobile-optimized render path (GLES3 shaders, quality tiers, simplified PBR)
- [ ] iOS / Metal backend — real `MTLDevice` + Metal shading language pipeline + `CAMetalLayer` present + Xcode code-sign; release-blocking for parity with Android

#### D.3 Web Export — Partial
- [x] Emscripten build target (CMakePresets, toolchain overlay, linker flags)
- [x] Emscripten main loop (requestAnimationFrame integration, canvas management)
- [x] HTML shell template (loading screen, DPI-aware canvas, touch prevention)
- [x] Asset streaming for web (streaming manifest, chunked download, LRU cache, progress tracking)
- [~] WebGL 2.0 RHI on Emscripten — real GLES3/WebGL2 driver path
- [ ] Remove the desktop CPU-simulated WebGL path from shippable artifacts (gate behind `NEXUS_DEV_ONLY`); release-blocking

#### D.4 Build & Export Pipeline ✓
- [x] Export profiles (per-platform configuration: graphics API, compression, signing)
- [x] Asset cooker (texture compression settings, mesh/audio/script processing)
- [x] Build pipeline (step-based: validate → cook → compile → build → package → sign)
- [x] Platform-specific steps (APK signing, iOS code sign, WebGL HTML shell)

### Phase E: Polish & Production (P4)

> Final quality bar for 1.0 release.

#### E.1 Documentation ✓
- [x] Doxygen API reference (Doxyfile configured, auto-generated)
- [x] Getting started tutorial (docs/getting_started.md)
- [x] System architecture guides (docs/architecture.md)
- [x] Example projects (2D Platformer, 3D FPS, Top-Down RPG)

#### E.2 Performance ✓
- [x] Integrated memory profiler with UI (per-tag stats, history, formatted display)
- [x] GPU profiler (per-pass timing, running averages, peak tracking, frame history)
- [x] Frame graph (declarative pass DAG, topological sort, dead-pass culling, per-pass timing)
- [x] Automated performance regression tests (entity ops, component iteration, hierarchy, vector math)

#### E.3 Stability ✓
- [x] Cross-platform crash handler (POSIX, Windows SEH, Android, Emscripten, C++ symbol demangling)
- [x] Structured error codes across all subsystems (ErrorCode enum, 40+ codes)
- [x] ErrorResult type for consistent error reporting
- [x] Integration test suite (8 cross-subsystem pipeline tests: ECS+serialization, ECS+scripting, assets+loader, animation, physics, network, full lifecycle)
- [x] Stress tests (10K entities, component iteration, rapid create/destroy)
- [x] 984 total tests, all passing

---

## Technology Stack (Actual vs. Planned)

| Category | Originally Planned | Actual Implementation | Status |
|----------|-------------------|----------------------|--------|
| Language | C++20 | C++20 | Done |
| Build | CMake 3.21+ | CMake 3.21+ with presets | Done |
| Graphics | Vulkan + OpenGL 4.5 | OpenGL 4.5 shipped; WebGL 2.0 real on Emscripten only; Vulkan 1.3 driver and iOS/Metal backend required for release | Partial |
| Windowing | GLFW | GLFW 3.4 | Done |
| 2D Physics | Box2D | Custom engine (spatial hash, SAT, raycast) | Done (custom) |
| 3D Physics | Jolt Physics | Custom engine (rigid body, joints, body sleeping, raycast) | Done (custom) |
| Audio | miniaudio | Custom mixer + miniaudio device output, stb_vorbis OGG decoding (WAV+OGG, spatial, DSP, Doppler, reverb zones) | Done |
| Scripting | Lua 5.4 (sol2) | Custom Lua-like interpreter + stdlib (math/string/table/os, 40+ functions) | Done (custom) |
| UI (Editor) | Dear ImGui | Dear ImGui 1.91 + GLFW/OpenGL3 backends + panel rendering | Done |
| Math | GLM | GLM 1.0.1 | Done |
| Model Loading | cgltf + assimp | Custom importers: OBJ + glTF 2.0 (JSON+GLB) + BMP/TGA/PPM | Done (custom) |
| Font | msdfgen + stb_truetype | Bitmap font (BMFont/glyph atlas) | Partial |
| Logging | spdlog | spdlog 1.13.0 | Done |
| Testing | Google Test | Google Test 1.14.0 (984 tests) | Done |
| Serialization | nlohmann/json + flatbuffers | nlohmann/json 3.11.3 + custom binary .nxs | Mostly done |

---

## Milestones (Updated)

| Milestone | Phase | Score | Key Deliverable |
|-----------|-------|-------|-----------------|
| **M-A — Usable Engine** | A (P0) | ✅ 78/100 | Scripting, asset importers, editor panels, networking |
| **M-B — Visual Parity** | B (P1) | ✅ 85/100 | TAA, skybox, soft shadows, GPU skinning, shader system |
| **M-C — Competitive** | C (P2) | ⚠ 80/100 | Light probes, decals, lightmap baker, reverb zones, editor tools (GPU-compute particles required for full credit) |
| **M-D — Multi-Platform** | D (P3) | ⚠ 70/100 | Emscripten loop, Android NDK bridge, CMakePresets shipped; **Vulkan 1.3 driver, iOS/Metal backend, and removal of CPU-simulated WebGL path required for full credit** |
| **M-E — 1.0 Release** | E (P4) | ⚠ 82/100 | Docs, frame graph, perf regression, example projects, web streaming, Lua stdlib, integration tests, audio device output, 984 tests passing — **gated on M-C and M-D release-blocking items above** |
