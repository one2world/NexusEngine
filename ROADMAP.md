# NexusEngine Roadmap

> **NexusEngine** — A modern 2D + 3D hybrid game engine built with C++20, designed for flexibility, performance, and ease of use.
>
> **Last audit date:** 2026-04-03 | **Overall readiness: 82/100** (targeting 2026 commercial engine standards)

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                      Editor / Tools (ImGui)                  │
├─────────────────────────────────────────────────────────────┤
│                    Scripting Layer (Lua/sol2)                 │
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
│        Rendering Backend (OpenGL 4.5 │ Vulkan [WIP])         │
└─────────────────────────────────────────────────────────────┘
```

---

## Current State Summary (as of 2026-04-03)

### Subsystem Readiness Scores

| Subsystem | Score | Status |
|-----------|-------|--------|
| ECS / Scene Graph | 90/100 | Production-grade sparse-set, hierarchy, prefabs |
| Audio | 82/100 | Spatial 3D, bus system, DSP effects, WAV+OGG, streaming, Doppler |
| Rendering (Core) | 88/100 | PBR+IBL, CSM+PCF soft shadows, deferred, TAA, skybox, GPU skinning, light probes, decals, GPU particles |
| Testing | 80/100 | 865 tests, 11K+ lines, all subsystems covered |
| Threading | 70/100 | Job system, render thread pool, parallel_for |
| Asset Pipeline | 75/100 | Async load, hot-reload, PAK, BMP/TGA/PPM/OBJ/WAV importers |
| Editor | 72/100 | Full panel architecture, undo/redo, ImGui rendering for all panels |
| Serialization | 70/100 | JSON + binary (.nxs) scene formats with schema versioning |
| Physics | 40/100 | Custom 2D+3D with spatial hash — not battle-tested |
| Platform | 40/100 | Desktop via GLFW — no mobile/console/web |
| Networking | 65/100 | RPC + replication + UDP transport with reliable delivery |
| Scripting | 60/100 | C++ API + Lua-like interpreter backend |

### What's Implemented (Done)

- [x] CMake build system with presets, FetchContent, sanitizers
- [x] spdlog logging, GLM math, high-res timer, event bus
- [x] Job system (thread pool + parallel_for) and render thread pool
- [x] GLFW window + input (keyboard, mouse, gamepad action mapping)
- [x] OpenGL 4.5 RHI with full resource abstraction
- [x] Vulkan RHI (CPU-simulated abstraction, not real driver)
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
- [x] 865 unit tests across all subsystems, all passing
- [x] Lua-like scripting backend (evaluator with if/then/end, variables, functions)
- [x] Asset importers (BMP, TGA, PPM/PGM textures, OBJ meshes, WAV audio)
- [x] UDP network transport (handshake, reliable delivery, ACK, timeout)
- [x] TAA anti-aliasing (Halton jitter, temporal reprojection, neighborhood clamping)
- [x] Skybox renderer (cubemap + Preetham procedural sky + reflection probes)
- [x] Binary scene serialization (.nxs format, schema versioning, forward-compatible)
- [x] GPU skinning (4-bone vertex shader, SkinnedMeshRenderer)
- [x] Shader library (hot-reload, #include resolution, variant/define system)
- [x] PCF soft shadows (16-tap Poisson disk, 3x3 PCF, cubemap point light PCF)
- [x] Editor panel ImGui rendering (hierarchy tree, inspector properties, console log, asset browser)
- [x] Light probes (spherical harmonics, grid-based, trilinear interpolation)
- [x] GPU particle system (100K+ capacity, billboard rendering, emission/simulation)
- [x] Deferred decal system (depth projection, edge fade, layer sorting)
- [x] Audio streaming (WAV/OGG chunk-based playback)
- [x] Doppler effect (velocity-based pitch shifting for spatial audio)

### Remaining Gaps

- [ ] **Real Vulkan driver** — actual VkDevice/VkQueue integration
- [ ] **glTF mesh/animation importer** — cgltf-based full glTF 2.0 pipeline
- [ ] **Lightmap baker** — progressive GPU-accelerated lightmap generation
- [ ] **Mobile platform support** — Android/iOS with touch input
- [ ] **Web export** — Emscripten/WebGL 2.0 target
- [ ] **Editor specialized tools** — tilemap editor, animation timeline, particle editor
- [ ] **Documentation** — Doxygen API reference, tutorials, example projects
- [ ] **Performance profiling** — integrated memory/GPU profilers with UI

---

## Development Phases (Remaining Work)

### Phase A: Critical Blockers — Release-Blocking (P0)

> Without these, the engine cannot ship as a usable product.

#### A.1 Lua Scripting Backend ✓
- [x] Lua-like interpreter with evaluator (if/then/end, assignment, concatenation)
- [x] Function call support, variable scoping, string operations
- [x] Script component lifecycle integration
- [ ] Full sol2/LuaJIT binding (stretch goal — current interpreter is functional)

#### A.2 Asset Importers ✓
- [x] Texture import: BMP, TGA, PPM/PGM decoders (no stb_image dependency)
- [x] Mesh import: OBJ loader with vertex/index buffers
- [x] Audio import: WAV loader + OGG container parsing
- [ ] glTF 2.0 import: cgltf → vertex/index/material/skeleton (planned)

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
- [x] Simple 3x3 PCF fallback for lower-cost shadow sampling
- [x] Point light soft shadow filtering (20-tap cubemap PCF)
- [ ] Contact-hardening soft shadows (PCSS) — stretch goal

#### B.4 GPU Skinning ✓
- [x] Upload bone matrices per draw (uniform-based)
- [x] Vertex shader: weighted bone transform (4 bones per vertex)
- [x] SkinnedMeshRenderer with SkinnedVertex format
- [x] Integration with animation system output

#### B.5 Shader System Improvements ✓
- [x] Shader hot-reload (file mtime comparison → recompile → rebind)
- [x] #include directive resolution for shader files (recursive, circular detection)
- [x] Shader variant/permutation system (#define injection after #version)
- [ ] Shader cache (compiled SPIR-V / GL program binary) — planned

### Phase C: Competitive Features — Market Differentiation (P2)

> Features that differentiate from other indie engines.

#### C.1 Global Illumination — Partial ✓
- [x] Light probes (spherical harmonics L2, 9 coefficients per channel)
- [x] Light probe grid (3D placement, trilinear interpolation)
- [x] Probe-based GI sampling for dynamic objects
- [ ] Lightmap baker (progressive, GPU-accelerated) — planned

#### C.2 GPU Particle System ✓
- [x] CPU-side emission + simulation (GPU compute planned for Vulkan)
- [x] Billboard rendering with camera-aligned quads
- [x] Additive/alpha blending, soft circle falloff
- [x] 100K+ particle capacity with dead-particle compaction

#### C.3 Advanced Audio — Partial ✓
- [x] OGG container parsing + Vorbis header extraction
- [x] Streaming playback (WAV chunk-based, OGG memory-buffered)
- [x] Doppler effect (velocity-based pitch calculation)
- [ ] Reverb zones (per-area DSP settings) — planned

#### C.4 Decal System ✓
- [x] Deferred decals (depth-buffer projection, inverse transform)
- [x] Albedo modification with edge fade
- [x] Layer-sorted rendering

#### C.5 Editor Specialized Tools
- [ ] Tilemap editor (paint, auto-tile, collision shapes)
- [ ] Animation timeline editor (keyframes, curves)
- [ ] Particle editor (real-time preview, presets)
- [ ] Material editor (property panel, live preview)

### Phase D: Platform & Distribution (P3)

> Expand beyond desktop.

#### D.1 Real Vulkan Backend
- [ ] VkInstance/VkDevice/VkQueue creation
- [ ] VMA memory allocator integration
- [ ] Swapchain management
- [ ] Descriptor set management
- [ ] SPIR-V shader pipeline

#### D.2 Mobile Platform Support
- [ ] Android (NDK + Vulkan/GLES)
- [ ] iOS (Metal backend)
- [ ] Touch input abstraction
- [ ] Mobile-optimized render path

#### D.3 Web Export
- [ ] Emscripten build target
- [ ] WebGL 2.0 backend
- [ ] Asset streaming for web

#### D.4 Build & Export Pipeline
- [ ] One-click platform export
- [ ] Asset cooking (compress textures, strip debug)
- [ ] Build configuration profiles

### Phase E: Polish & Production (P4)

> Final quality bar for 1.0 release.

#### E.1 Documentation
- [ ] Doxygen API reference (auto-generated)
- [ ] Getting started tutorial
- [ ] System architecture guides
- [ ] Example projects (2D platformer, 3D FPS, top-down RPG)

#### E.2 Performance
- [ ] Integrated memory profiler with UI
- [ ] GPU profiler (per-pass timing)
- [ ] Frame graph visualization
- [ ] Automated performance regression tests

#### E.3 Stability
- [ ] Crash handler with minidump generation
- [ ] Structured error codes across all subsystems
- [ ] Integration test suite
- [ ] Stress tests (10K entities, 100K particles)

---

## Technology Stack (Actual vs. Planned)

| Category | Planned | Actual | Status |
|----------|---------|--------|--------|
| Language | C++20 | C++20 | Done |
| Build | CMake 3.21+ | CMake 3.21+ | Done |
| Graphics | Vulkan + OpenGL 4.5 | OpenGL 4.5 (Vulkan emulated) | Partial |
| Windowing | GLFW | GLFW 3.4 | Done |
| 2D Physics | Box2D | Custom implementation | Diverged |
| 3D Physics | Jolt Physics | Custom implementation | Diverged |
| Audio | miniaudio | Custom mixer (WAV+OGG, streaming, Doppler) | Mostly done |
| Scripting | Lua 5.4 (sol2) | Lua-like interpreter backend | Partial |
| UI (Editor) | Dear ImGui | ImGui abstraction layer + full panel rendering | Done |
| Math | GLM | GLM 1.0.1 | Done |
| Model Loading | cgltf + assimp | OBJ loader + BMP/TGA/PPM importers | Partial |
| Font | msdfgen + stb_truetype | Bitmap font (BMFont format) | Partial |
| Logging | spdlog | spdlog 1.13.0 | Done |
| Testing | Google Test | Google Test 1.14.0 | Done |
| Serialization | nlohmann/json + flatbuffers | nlohmann/json 3.11.3 + binary .nxs | Mostly done |

---

## Milestones (Updated)

| Milestone | Phase | Score | Key Deliverable |
|-----------|-------|-------|-----------------|
| **M-A — Usable Engine** | A (P0) | ✅ 78/100 | Scripting, asset importers, editor panels, networking |
| **M-B — Visual Parity** | B (P1) | ✅ 85/100 | TAA, skybox, soft shadows, GPU skinning, shader system |
| **M-C — Competitive** | C (P2) | 82/100 (partial) | Light probes, GPU particles, decals — editor tools pending |
| **M-D — Multi-Platform** | D (P3) | → 95/100 | Vulkan, mobile, web export |
| **M-E — 1.0 Release** | E (P4) | → 100/100 | Docs, stability, performance validation |
