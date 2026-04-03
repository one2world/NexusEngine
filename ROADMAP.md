# NexusEngine Roadmap

> **NexusEngine** — A modern 2D + 3D hybrid game engine built with C++20, designed for flexibility, performance, and ease of use.
>
> **Last audit date:** 2026-03-27 | **Overall readiness: 62/100** (targeting 2026 commercial engine standards)

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

## Current State Summary (as of 2026-03-27)

### Subsystem Readiness Scores

| Subsystem | Score | Status |
|-----------|-------|--------|
| ECS / Scene Graph | 90/100 | Production-grade sparse-set, hierarchy, prefabs |
| Audio | 75/100 | Spatial 3D, bus system, DSP effects, WAV only |
| Rendering (Core) | 72/100 | PBR+IBL, CSM shadows, deferred, post-FX, terrain |
| Testing | 80/100 | 865 tests, 11K+ lines, all subsystems covered |
| Threading | 70/100 | Job system, render thread pool, parallel_for |
| Asset Pipeline | 60/100 | Async load, hot-reload, PAK packaging — no importers |
| Editor | 55/100 | Full panel architecture, undo/redo — panels are stubs |
| Serialization | 50/100 | JSON scene format with rollback — no binary format |
| Physics | 40/100 | Custom 2D+3D with spatial hash — not battle-tested |
| Platform | 40/100 | Desktop via GLFW — no mobile/console/web |
| Networking | 35/100 | RPC + replication framework — no transport layer |
| Scripting | 20/100 | C++ API framework complete — no Lua backend |

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

### Critical Gaps (Not Yet Implemented)

- [ ] **Lua scripting backend** — sol2/LuaJIT binding to make scripting usable
- [ ] **Asset importers** — stb_image (textures), cgltf (meshes), dr_wav/stb_vorbis (audio)
- [ ] **Network transport** — actual UDP/TCP socket implementation
- [ ] **Editor panel rendering** — ImGui draw code for inspector/hierarchy/console
- [ ] **TAA anti-aliasing** — temporal jitter + reprojection (FXAA is outdated)
- [ ] **Skybox renderer** — cubemap/procedural sky rendering
- [ ] **Binary serialization** — fast scene format with schema versioning
- [ ] **GPU skinning** — vertex shader bone transforms for skeletal meshes
- [ ] **Shader system** — hot-reload, variants, preprocessor includes
- [ ] **Real Vulkan driver** — actual VkDevice/VkQueue integration

---

## Development Phases (Remaining Work)

### Phase A: Critical Blockers — Release-Blocking (P0)

> Without these, the engine cannot ship as a usable product.

#### A.1 Lua Scripting Backend
- [ ] Integrate sol2 (header-only) via FetchContent
- [ ] Bind ScriptEngine to Lua VM (create_state, execute, call)
- [ ] Expose core API: Entity/Registry, Transform, Input, Audio, Physics
- [ ] Connect script component lifecycle: on_create, on_update, on_destroy
- [ ] Hot-reload: detect .lua file changes, reload VM state
- [ ] Error handling: Lua stack traces mapped to file:line

#### A.2 Asset Importers
- [ ] Texture import: stb_image → RHI texture (PNG, JPG, TGA, HDR)
- [ ] Mesh import: cgltf → vertex/index buffers (glTF 2.0)
- [ ] Audio import: dr_wav + stb_vorbis → AudioBuffer (WAV, OGG)
- [ ] Material import: glTF PBR material → engine Material
- [ ] Skeleton/animation import: glTF skin + animation → engine format

#### A.3 Editor Panel Rendering (ImGui)
- [ ] InspectorPanel: component property editors (float, vec, color, enum)
- [ ] HierarchyPanel: entity tree with drag-drop reparenting, context menu
- [ ] ConsolePanel: scrollable log with level filtering, color coding
- [ ] AssetBrowserPanel: grid view with thumbnails, search, navigation
- [ ] ViewportPanel: verify gizmo rendering, camera controls

#### A.4 Network Transport Layer
- [ ] UDP socket abstraction (send/recv, non-blocking)
- [ ] Connection handshake (SYN/ACK with timeout)
- [ ] Reliable delivery (sequence numbers, ACK bitfield, retransmit)
- [ ] Packet fragmentation for large payloads
- [ ] Integration with existing RPC/replication framework

#### A.5 Binary Scene Serialization
- [ ] Binary format: header (magic, version, entity count) + packed components
- [ ] Schema version field with forward-compatibility check
- [ ] Backward-compatible reader (skip unknown component types)
- [ ] Save/load API parallel to existing JSON interface

### Phase B: Visual Quality — Industry Standard (P1)

> Required to match 2026 visual expectations (Godot 5 / Unity 6 baseline).

#### B.1 Skybox & Environment
- [ ] Cubemap skybox renderer
- [ ] Procedural sky (Hosek-Wilkie or Preetham model)
- [ ] Reflection probes (baked cubemap capture)
- [ ] Parallax-corrected cubemap sampling

#### B.2 Temporal Anti-Aliasing (TAA)
- [ ] Per-frame sub-pixel jitter (Halton sequence)
- [ ] Motion vector generation (per-object + camera)
- [ ] Temporal reprojection with neighborhood clamping
- [ ] History buffer management with disocclusion detection

#### B.3 Soft Shadows
- [ ] PCF (Percentage Closer Filtering) for CSM
- [ ] Poisson disk or rotated Vogel sampling
- [ ] Contact-hardening soft shadows (PCSS) — stretch goal
- [ ] Point light soft shadow filtering

#### B.4 GPU Skinning
- [ ] Upload bone matrices as UBO/SSBO per draw
- [ ] Vertex shader: weighted bone transform (4 bones per vertex)
- [ ] SkinnedMeshRenderer component
- [ ] Integration with animation system output

#### B.5 Shader System Improvements
- [ ] Shader hot-reload (file watcher → recompile → rebind)
- [ ] #include directive resolution for shader files
- [ ] Shader variant/permutation system (#define-based)
- [ ] Shader cache (compiled SPIR-V / GL program binary)

### Phase C: Competitive Features — Market Differentiation (P2)

> Features that differentiate from other indie engines.

#### C.1 Global Illumination
- [ ] Light probes (spherical harmonics, grid-placed)
- [ ] Lightmap baker (progressive, GPU-accelerated)
- [ ] Probe-based GI blending for dynamic objects

#### C.2 GPU Particle System
- [ ] Compute shader emission + simulation
- [ ] GPU storage buffer for particle data
- [ ] Sort-free additive/alpha rendering
- [ ] 100K+ particle capacity

#### C.3 Advanced Audio
- [ ] OGG/MP3 decoding (stb_vorbis, dr_mp3)
- [ ] Streaming playback for large files
- [ ] Doppler effect for spatial audio
- [ ] Reverb zones (per-area DSP settings)

#### C.4 Decal System
- [ ] Deferred decals (project onto G-buffer)
- [ ] Normal + albedo modification
- [ ] Decal atlas for efficient batching

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
| Audio | miniaudio | Custom mixer (WAV-only) | Partial |
| Scripting | Lua 5.4 (sol2) | C++ framework only | Not started |
| UI (Editor) | Dear ImGui | ImGui (panels stubbed) | Partial |
| Math | GLM | GLM 1.0.1 | Done |
| Model Loading | cgltf + assimp | Framework only (no importers) | Not started |
| Font | msdfgen + stb_truetype | Bitmap font (BMFont format) | Partial |
| Logging | spdlog | spdlog 1.13.0 | Done |
| Testing | Google Test | Google Test 1.14.0 | Done |
| Serialization | nlohmann/json + flatbuffers | nlohmann/json 3.11.3 only | Partial |

---

## Milestones (Updated)

| Milestone | Phase | Score | Key Deliverable |
|-----------|-------|-------|-----------------|
| **M-A — Usable Engine** | A (P0) | → 78/100 | Users can script gameplay, import assets, use editor |
| **M-B — Visual Parity** | B (P1) | → 85/100 | TAA, skybox, soft shadows, GPU skinning |
| **M-C — Competitive** | C (P2) | → 92/100 | GI, GPU particles, decals, editor tools |
| **M-D — Multi-Platform** | D (P3) | → 95/100 | Vulkan, mobile, web export |
| **M-E — 1.0 Release** | E (P4) | → 100/100 | Docs, stability, performance validation |
