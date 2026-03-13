# NexusEngine Roadmap

> **NexusEngine** — A modern 2D + 3D hybrid game engine built with C++17/20, designed for flexibility, performance, and ease of use.

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                      Editor / Tools                         │
├─────────────────────────────────────────────────────────────┤
│                    Scripting Layer (Lua)                     │
├──────────────────────┬──────────────────────────────────────┤
│   2D Subsystem       │         3D Subsystem                 │
│  ┌────────────────┐  │  ┌────────────────────────────────┐  │
│  │ Sprite Renderer │  │  │ Forward / Deferred Renderer   │  │
│  │ Tilemap Engine  │  │  │ PBR Material System           │  │
│  │ 2D Physics      │  │  │ Skeletal Animation            │  │
│  │ 2D Particles    │  │  │ 3D Physics (Bullet/Jolt)      │  │
│  │ Spine/Anim      │  │  │ 3D Particles                  │  │
│  └────────────────┘  │  └────────────────────────────────┘  │
├──────────────────────┴──────────────────────────────────────┤
│                    Scene Graph (Unified ECS)                 │
├─────────────────────────────────────────────────────────────┤
│  Audio │ Input │ Networking │ UI System │ Asset Pipeline    │
├─────────────────────────────────────────────────────────────┤
│           Platform Abstraction Layer (PAL)                   │
│     Windows │ Linux │ macOS │ Web (Emscripten)              │
├─────────────────────────────────────────────────────────────┤
│        Rendering Backend (Vulkan │ OpenGL │ Metal)          │
└─────────────────────────────────────────────────────────────┘
```

---

## Phase 0: Foundation (Weeks 1–4)

> Build the skeleton: build system, platform layer, logging, math, and a window.

### 0.1 Build System & Project Structure
- [ ] CMake-based build system with presets (Debug / Release / Profile)
- [ ] Dependency management via CMake FetchContent (or vcpkg)
- [ ] CI pipeline: GitHub Actions for Linux/Windows/macOS
- [ ] Code style: `.clang-format`, `.clang-tidy` config
- [ ] Unit test framework integration (Google Test)

### 0.2 Core Library (`nexus-core`)
- [ ] **Logging** — spdlog-based, multi-sink (console + file), compile-time level filtering
- [ ] **Math** — glm wrapper with engine-specific types: `Vec2`, `Vec3`, `Vec4`, `Mat4`, `Quat`, `Transform`, `AABB`, `Rect`
- [ ] **Memory** — Arena allocator, pool allocator, linear allocator, aligned allocation helpers
- [ ] **Containers** — Flat hash map, slot map, ring buffer, sparse set
- [ ] **String** — String interning (`StringId`), string formatting utilities
- [ ] **File I/O** — Virtual file system (VFS) with mount points, async file reads
- [ ] **Time** — High-resolution timer, fixed timestep, frame clock
- [ ] **Event System** — Type-safe event bus with priority and filtering
- [ ] **Job System** — Thread pool with task graph, fiber-based coroutine support

### 0.3 Platform Abstraction Layer
- [ ] Window creation and management (GLFW backend)
- [ ] Input abstraction: keyboard, mouse, gamepad (raw + mapped actions)
- [ ] Clipboard, drag-and-drop support
- [ ] Multi-monitor and DPI awareness
- [ ] Platform-specific paths (config dir, save dir, etc.)

### Deliverable
A window opens on all 3 desktop platforms, logging works, input is received, math types are tested.

---

## Phase 1: Rendering Foundation (Weeks 5–10)

> Get triangles on screen with a clean rendering abstraction.

### 1.1 Rendering Hardware Interface (RHI)
- [ ] Abstract GPU API layer supporting **Vulkan** (primary) and **OpenGL 4.5** (fallback)
- [ ] Resource types: Buffer, Texture, Shader, Pipeline, RenderPass, Framebuffer
- [ ] Command buffer abstraction with deferred submission
- [ ] Shader compilation pipeline: GLSL → SPIR-V (via shaderc)
- [ ] Shader reflection for automatic descriptor set layout
- [ ] GPU memory allocator (VMA for Vulkan)

### 1.2 2D Renderer
- [ ] **Batch Renderer** — Auto-batching by texture/shader, supports quads, lines, circles
- [ ] **Sprite System** — Sprite sheets, texture atlases, UV animation
- [ ] **Text Rendering** — SDF font rendering (msdfgen), Unicode support
- [ ] **Camera 2D** — Orthographic projection, smooth follow, screen shake
- [ ] **Layer/Sort** — Z-order sorting, render layers, transparency handling

### 1.3 3D Renderer (Basic)
- [ ] **Forward Renderer** — Multi-pass with directional + point + spot lights
- [ ] **Mesh System** — Static mesh loading (glTF 2.0 via cgltf), vertex formats
- [ ] **Material System** — Uber-shader approach, material instances, property blocks
- [ ] **Camera 3D** — Perspective + orthographic, frustum culling
- [ ] **Skybox** — Cubemap skybox, procedural sky (Preetham / Hosek-Wilkie)
- [ ] **Debug Rendering** — Wireframe, bounding boxes, grid, gizmos

### Deliverable
Render 2D sprites and 3D meshes in the same scene. Hot-reload shaders.

---

## Phase 2: Scene & Entity System (Weeks 11–14)

> Unified data model for all game objects, 2D and 3D.

### 2.1 Entity Component System (ECS)
- [ ] Archetype-based ECS (inspired by flecs/entt)
- [ ] Core components: `Transform2D`, `Transform3D`, `SpriteRenderer`, `MeshRenderer`, `Camera`, `Light`, `RigidBody2D`, `RigidBody3D`
- [ ] System scheduler with dependency resolution and parallel execution
- [ ] Entity prefabs / templates (serializable)
- [ ] Hierarchy / parent-child transform propagation

### 2.2 Scene Graph
- [ ] Scene serialization (JSON + binary formats)
- [ ] Scene loading / unloading with streaming support
- [ ] Multi-scene composition (additive loading)
- [ ] 2D/3D mode flag per scene with automatic renderer selection

### Deliverable
Load a scene file that contains both 2D and 3D entities, render them correctly.

---

## Phase 3: Physics (Weeks 15–18)

> Integrate physics for both 2D and 3D worlds.

### 3.1 2D Physics (Box2D)
- [ ] Box2D integration with ECS components
- [ ] Collision shapes: box, circle, polygon, edge, chain
- [ ] Triggers, raycasting, contact callbacks
- [ ] Joint types: revolute, distance, prismatic, weld
- [ ] Physics debug draw overlay

### 3.2 3D Physics (Jolt Physics)
- [ ] Jolt Physics integration with ECS components
- [ ] Collision shapes: box, sphere, capsule, convex hull, triangle mesh
- [ ] Character controller (kinematic)
- [ ] Raycasting and shape casting
- [ ] Constraints: hinge, slider, cone, fixed
- [ ] Physics debug draw overlay

### 3.3 Unified Physics Interface
- [ ] Common query API: raycast, overlap, sweep (works in both 2D/3D)
- [ ] Physics world stepping synced to fixed timestep
- [ ] Collision layers and masks (shared between 2D/3D)

### Deliverable
2D platformer physics and 3D rigid body simulation running in the same engine.

---

## Phase 4: Audio (Weeks 19–20)

> Spatial and non-spatial audio with an event-driven API.

### 4.1 Audio System
- [ ] Backend: miniaudio (cross-platform)
- [ ] Audio resource: WAV, OGG, MP3 loading
- [ ] Sound playback: play, pause, stop, loop, volume, pitch
- [ ] 3D spatial audio: distance attenuation, panning, Doppler
- [ ] Audio bus / mixer: master, SFX, music, voice channels
- [ ] Audio event system (trigger by name, parameter control)

### Deliverable
Play background music and spatial SFX tied to entities.

---

## Phase 5: Animation (Weeks 21–24)

> Bring 2D sprites and 3D characters to life.

### 5.1 2D Animation
- [ ] Sprite animation: frame-based, timeline editor data
- [ ] Spine / Spriter runtime integration (optional)
- [ ] Tweening library: ease functions, property animation
- [ ] Particle system 2D: emission shapes, affectors, texture animation

### 5.2 3D Animation
- [ ] Skeletal animation: bone hierarchy, skinning (GPU)
- [ ] Animation clips: glTF animation import
- [ ] Animation blending: lerp, additive, masked layers
- [ ] Animation state machine: states, transitions, blend trees
- [ ] Morph targets / blend shapes
- [ ] IK solver (two-bone, FABRIK)

### 5.3 Particle System 3D
- [ ] GPU particle system: compute shader emission/simulation
- [ ] Emitter shapes: box, sphere, cone, mesh surface
- [ ] Affectors: gravity, wind, turbulence, color/size over lifetime
- [ ] Ribbon / trail renderer

### Deliverable
Animated 2D characters and 3D skinned characters with state machines.

---

## Phase 6: Advanced Rendering (Weeks 25–30)

> Level up visual quality.

### 6.1 PBR & Lighting
- [ ] PBR material model (metallic-roughness workflow)
- [ ] Image-based lighting (IBL): irradiance + prefiltered env maps
- [ ] Shadow mapping: cascaded shadow maps (CSM) for directional light
- [ ] Point/spot light shadow maps (omnidirectional / perspective)
- [ ] Screen-space ambient occlusion (SSAO)
- [ ] Bloom, tone mapping (ACES), gamma correction

### 6.2 Deferred Rendering Path
- [ ] G-Buffer: albedo, normal, metallic-roughness, depth
- [ ] Deferred lighting pass
- [ ] Light volumes for efficient many-light rendering
- [ ] Hybrid forward+deferred for transparent objects

### 6.3 Post-Processing Stack
- [ ] Configurable post-process pipeline
- [ ] Effects: FXAA/TAA, motion blur, depth of field, chromatic aberration, vignette
- [ ] Color grading with LUT support
- [ ] Custom post-process shader support

### 6.4 Terrain & Environment
- [ ] Heightmap terrain with LOD (quadtree-based)
- [ ] Terrain splatmap painting (4+ texture layers)
- [ ] Vegetation: instanced grass, billboarded trees
- [ ] Water rendering: planar reflections, flow maps
- [ ] Fog: linear, exponential, height-based

### Deliverable
Visually compelling 3D scenes with PBR, shadows, post-processing. 2D rendering still crisp and performant.

---

## Phase 7: UI System (Weeks 31–33)

> In-game UI for both 2D and 3D projects.

### 7.1 UI Framework
- [ ] Retained-mode UI with layout engine (flexbox-like)
- [ ] Widgets: Label, Button, Slider, ScrollView, TextInput, Image, ProgressBar
- [ ] Theming / skinning system (data-driven)
- [ ] UI anchoring and responsive scaling
- [ ] Event routing: hover, click, focus, keyboard navigation
- [ ] Gamepad-friendly navigation (focus system)
- [ ] 9-slice sprite support for UI panels
- [ ] World-space UI (billboarded or screen-attached)

### Deliverable
A fully functional in-game UI that works for menus, HUDs, and dialogs.

---

## Phase 8: Scripting (Weeks 34–36)

> Expose the engine to gameplay programmers via Lua.

### 8.1 Lua Scripting
- [ ] Lua 5.4 / LuaJIT integration via sol2
- [ ] Bind core engine API: ECS, Input, Audio, Physics, UI, Scene
- [ ] Script component: `OnCreate`, `OnUpdate`, `OnDestroy`, `OnCollision` callbacks
- [ ] Hot-reload scripts at runtime
- [ ] Coroutine-based async scripting (wait, delay, tween)
- [ ] Script debugging: error reporting with stack traces, breakpoints (via IDE plugin)

### Deliverable
Write gameplay logic entirely in Lua with hot-reload support.

---

## Phase 9: Asset Pipeline (Weeks 37–40)

> Efficient content creation and loading.

### 9.1 Asset System
- [ ] Unified asset handle system with reference counting
- [ ] Asset types: Texture, Mesh, Material, Shader, Audio, Font, Scene, Prefab, Script, Animation
- [ ] Async asset loading with progress callbacks
- [ ] Asset hot-reload (detect file changes → reimport)

### 9.2 Import Pipeline
- [ ] Texture import: PNG, JPG, TGA, HDR → compressed formats (BC/ASTC)
- [ ] Mesh import: glTF 2.0, FBX (via assimp fallback), OBJ
- [ ] Audio import: WAV, OGG, MP3 → engine format
- [ ] Asset cooking: binary asset bundles for release builds
- [ ] Texture atlas packer (automatic sprite sheet generation)

### 9.3 Resource Packaging
- [ ] PAK file system for distribution
- [ ] Asset dependency graph and tree-shaking
- [ ] Streaming support for large assets (terrain, audio)

### Deliverable
Drop assets into a folder, engine auto-imports and hot-reloads them.

---

## Phase 10: Networking (Weeks 41–44)

> Multiplayer-ready networking layer.

### 10.1 Network Layer
- [ ] Transport: ENet (reliable UDP) + WebSocket (for web builds)
- [ ] Client-server architecture with authority model
- [ ] Serialization: bitpacking, delta compression
- [ ] Entity replication: automatic component sync
- [ ] RPC system: annotate functions as client/server RPCs
- [ ] Lag compensation: client-side prediction, server reconciliation
- [ ] Network simulation: artificial latency, packet loss, jitter (debug)

### Deliverable
A basic multiplayer game with synchronized entities.

---

## Phase 11: Editor (Weeks 45–52)

> Visual editor for creating games without touching code.

### 11.1 Editor Core (Dear ImGui-based)
- [ ] Dockable panel system: viewport, hierarchy, inspector, console, asset browser
- [ ] Scene viewport with manipulator gizmos (translate, rotate, scale)
- [ ] Entity hierarchy tree with drag-and-drop reparenting
- [ ] Inspector panel: auto-generated property editors from component reflection
- [ ] Undo/redo system with command pattern
- [ ] Multi-selection and group operations
- [ ] Play/Pause/Step controls with editor ↔ runtime state isolation

### 11.2 Asset Browser
- [ ] Thumbnail generation for textures, meshes, materials
- [ ] Drag-and-drop from browser to scene
- [ ] Asset search and filtering
- [ ] Import settings editor

### 11.3 Specialized Editors
- [ ] **Tilemap Editor** — Paint tiles, auto-tiling rules, collision shapes
- [ ] **Animation Editor** — Timeline, keyframe editing, state machine graph
- [ ] **Particle Editor** — Real-time particle tweaking with presets
- [ ] **Material Editor** — Node-based shader graph (stretch goal)
- [ ] **Physics Editor** — Collision shape editing, joint visualization

### 11.4 Build & Export
- [ ] One-click build for Windows, Linux, macOS
- [ ] Web export (Emscripten/WebGL)
- [ ] Build profiles and configuration
- [ ] Asset cooking integrated into build pipeline

### Deliverable
A functional editor where you can build a complete game visually.

---

## Phase 12: Polish & Optimization (Weeks 53–56)

> Production-quality performance and stability.

### 12.1 Performance
- [ ] Profiler: CPU frame timeline, GPU timing queries
- [ ] Memory tracker: allocation stats, leak detection
- [ ] Render stats overlay: draw calls, triangles, FPS, memory
- [ ] Occlusion culling (software rasterizer or GPU-driven)
- [ ] LOD system for meshes
- [ ] Instanced rendering for repeated objects
- [ ] Multi-threaded rendering (command buffer recording on worker threads)

### 12.2 Quality
- [ ] Comprehensive unit and integration tests
- [ ] Automated visual regression testing
- [ ] Crash reporting with minidumps
- [ ] Documentation: API reference (Doxygen), tutorials, examples

### Deliverable
Stable, well-documented engine ready for game development.

---

## Technology Stack

| Category | Choice | Rationale |
|---|---|---|
| Language | C++20 | Performance, ecosystem, industry standard |
| Build | CMake 3.21+ | Cross-platform, widely supported |
| Graphics | Vulkan + OpenGL 4.5 | Modern + fallback |
| Windowing | GLFW | Lightweight, cross-platform |
| 2D Physics | Box2D | Mature, well-documented |
| 3D Physics | Jolt Physics | Modern, high-performance |
| Audio | miniaudio | Single-header, cross-platform |
| Scripting | Lua 5.4 (sol2) | Fast, embeddable, game-industry proven |
| UI (Editor) | Dear ImGui | Immediate-mode, rapid iteration |
| Math | GLM | Standard for OpenGL/Vulkan |
| Model Loading | cgltf + assimp | glTF native + fallback |
| Font | msdfgen + stb_truetype | SDF text + bitmap fallback |
| Logging | spdlog | Fast, feature-rich |
| Testing | Google Test | Industry standard |
| Serialization | nlohmann/json + flatbuffers | Human-readable + binary |

---

## Directory Structure

```
NexusEngine/
├── CMakeLists.txt              # Root build configuration
├── cmake/                      # CMake modules and toolchains
│   ├── Dependencies.cmake      # FetchContent / vcpkg deps
│   └── CompilerFlags.cmake     # Compiler settings
├── engine/
│   ├── core/                   # Logging, math, memory, containers, events
│   │   ├── include/nexus/core/
│   │   └── src/
│   ├── platform/               # Window, input, filesystem
│   │   ├── include/nexus/platform/
│   │   └── src/
│   ├── rhi/                    # Rendering hardware interface
│   │   ├── include/nexus/rhi/
│   │   └── src/
│   ├── renderer/               # 2D and 3D renderers
│   │   ├── include/nexus/renderer/
│   │   └── src/
│   ├── scene/                  # ECS, scene graph
│   │   ├── include/nexus/scene/
│   │   └── src/
│   ├── physics/                # 2D (Box2D) + 3D (Jolt) physics
│   │   ├── include/nexus/physics/
│   │   └── src/
│   ├── audio/                  # Audio system
│   │   ├── include/nexus/audio/
│   │   └── src/
│   ├── animation/              # 2D + 3D animation
│   │   ├── include/nexus/animation/
│   │   └── src/
│   ├── ui/                     # In-game UI
│   │   ├── include/nexus/ui/
│   │   └── src/
│   ├── scripting/              # Lua binding
│   │   ├── include/nexus/scripting/
│   │   └── src/
│   ├── assets/                 # Asset pipeline
│   │   ├── include/nexus/assets/
│   │   └── src/
│   └── network/                # Networking
│       ├── include/nexus/network/
│       └── src/
├── editor/                     # Editor application (ImGui)
│   ├── include/
│   └── src/
├── runtime/                    # Standalone game runtime
│   └── src/
├── sandbox/                    # Test/demo project
│   ├── assets/
│   └── src/
├── tests/                      # Unit and integration tests
│   ├── core/
│   ├── renderer/
│   ├── physics/
│   └── ...
├── docs/                       # Documentation
├── tools/                      # Build scripts, asset tools
├── .clang-format
├── .clang-tidy
├── .gitignore
├── LICENSE
└── README.md
```

---

## Milestones Summary

| Milestone | Phase | Key Result |
|---|---|---|
| **M0 — Bootstrap** | 0 | Window + Input + Logging |
| **M1 — First Pixels** | 1 | 2D sprites + 3D meshes rendered |
| **M2 — World Building** | 2 | ECS + Scene loading |
| **M3 — Physics** | 3 | 2D + 3D physics integrated |
| **M4 — Sound** | 4 | Spatial audio working |
| **M5 — Motion** | 5 | Skeletal + sprite animation |
| **M6 — Beauty** | 6 | PBR + shadows + post-FX |
| **M7 — Interface** | 7 | In-game UI system |
| **M8 — Scripting** | 8 | Lua gameplay scripting |
| **M9 — Pipeline** | 9 | Full asset import/export |
| **M10 — Online** | 10 | Multiplayer networking |
| **M11 — Editor** | 11 | Visual scene editor |
| **M12 — Ship It** | 12 | Optimized + documented |
