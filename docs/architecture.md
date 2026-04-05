# NexusEngine Architecture Guide

## Overview

NexusEngine is a modern 2D+3D hybrid game engine built with C++20. It features a modular architecture with clearly separated subsystems, a sparse-set ECS, PBR rendering, and cross-platform support.

## Module Hierarchy

```
engine/
  core/        - Types, math (GLM), logging (spdlog), event bus, allocators
  rhi/         - Render Hardware Interface (OpenGL 4.5 + Vulkan abstraction)
  renderer/    - Forward/Deferred/PBR/Mobile renderers, shadows, particles, etc.
  scene/       - ECS registry, hierarchy, components, serialization, prefabs
  physics/     - Custom 2D+3D physics (spatial hash, constraints, raycasting)
  audio/       - Spatial audio, bus system, DSP effects, streaming, reverb zones
  animation/   - Skeleton, clips, blend trees, state machine, IK
  ui/          - Retained-mode UI widget system
  assets/      - Asset registry, async loading, PAK packaging, importers
  network/     - RPC framework, UDP transport, replication
  scripting/   - Script engine framework + Lua-like interpreter
  perf/        - Profiler, memory tracker, LOD, occlusion culling, thread pool
  platform/    - Window (GLFW), input, touch input, build/export pipeline

editor/        - ImGui-based editor (panels, tools, undo/redo)
tests/         - Google Test suite (905+ tests across all subsystems)
```

## Core Subsystems

### Entity Component System (ECS)

The ECS uses a **sparse-set architecture** with generation-based entity handles:

- **Entity**: A `u64` handle encoding index (32-bit) + generation (32-bit)
- **Registry**: Central storage with per-component-type sparse sets
- **Components**: Plain structs (TagComponent, Transform2D/3D, SpriteRenderer, Camera, etc.)
- **Systems**: Free functions or classes operating on component views
- **Hierarchy**: Parent/child relationships with automatic transform propagation

Key files: `engine/scene/include/nexus/scene/registry.h`, `components.h`, `hierarchy.h`

### Rendering Pipeline

NexusEngine supports multiple rendering paths:

1. **Forward Renderer 3D** - Multi-light with shadow integration
2. **Deferred Renderer** - G-buffer pass + light volumes
3. **PBR Renderer** - Metallic-roughness workflow with IBL
4. **Batch Renderer 2D** - Auto-batching with frustum culling
5. **Mobile Renderer** - GLES3-compatible simplified PBR

Shadow techniques: Cascaded Shadow Maps (CSM), Point Light Cubemap Shadows, PCF (16-tap Poisson, 3x3), PCSS (contact-hardening).

Post-processing: Bloom, FXAA, TAA, Tonemapping (ACES), Vignette, SSAO, Fog.

Advanced: Light probes (SH L2), GPU particles (100K+), deferred decals, lightmap baker, skybox (cubemap + Preetham procedural).

### Render Hardware Interface (RHI)

The RHI provides a backend-agnostic API:

```cpp
class RHI {
    ShaderHandle create_shader(const char* vert, const char* frag);
    BufferHandle create_buffer(BufferUsage usage, const void* data, u32 size);
    TextureHandle create_texture(const TextureDesc& desc);
    FramebufferHandle create_framebuffer(const FramebufferDesc& desc);
    PipelineHandle create_pipeline(const PipelineDesc& desc);
    void draw_indexed(u32 index_count);
    // ...
};
```

Backends: OpenGL 4.5 (production), Vulkan (abstraction layer).

### Audio System

- **AudioEngine**: Mixer with per-source 3D spatialization
- **AudioBus**: Hierarchical mixing with per-bus effects
- **DSP Effects**: Low-pass, high-pass, reverb (Schroeder), effect chains
- **Streaming**: WAV chunk-based, OGG memory-buffered
- **Reverb Zones**: AABB/sphere spatial zones with fade blending
- **Doppler**: Velocity-based pitch shifting

### Asset Pipeline

- **AssetRegistry**: Handle-based asset management with reference counting
- **Async Loading**: Background thread loading with completion callbacks
- **Hot-Reload**: File watcher for live asset updates in editor
- **PAK Packaging**: Archive format for distribution
- **Importers**: BMP, TGA, PPM/PGM, OBJ, WAV, glTF 2.0 (JSON+GLB)
- **Shader Cache**: Platform-aware source caching with hash-based lookup

### Editor

Built on an ImGui abstraction layer (`NEXUS_HAS_IMGUI` conditional compilation):

- **HierarchyPanel**: Entity tree with drag-drop reparenting
- **InspectorPanel**: Component property editors
- **ConsolePanel**: Scrollable log with level filtering
- **AssetBrowserPanel**: Grid view with navigation
- **ViewportPanel**: 3D/2D rendering viewport
- **Specialized Tools**: Tilemap editor, animation timeline, particle editor, material editor
- **Undo/Redo**: Command pattern with transaction support

### Physics

Custom implementations (not Box2D/Jolt):

- **Spatial Hash**: O(1) broad-phase with configurable cell size
- **2D Physics**: AABB + circle colliders, constraints, raycasting, debug draw
- **3D Physics**: Sphere + box colliders, body sleeping, raycast, gravity

### Networking

- **UDP Transport**: Non-blocking sockets, handshake, reliable delivery with ACK bitfield
- **RPC Framework**: Remote procedure calls with serialization
- **Replication**: Entity state replication stubs with prediction

## Build System

CMake 3.21+ with FetchContent for dependencies:

```bash
cmake --preset default
cmake --build build --parallel
ctest --test-dir build
```

Dependencies: GLFW, GLM, spdlog, nlohmann/json, Google Test

## Threading Model

- **Main Thread**: Game loop, input, scene updates
- **Render Thread Pool**: Parallel draw call submission
- **Job System**: `parallel_for` for data-parallel work
- **Audio Thread**: Separate mixer thread (planned)
- **Asset Loading**: Background threads with completion callbacks

## Memory Management

- **Linear Allocator**: Frame-temporary allocations (reset per frame)
- **Pool Allocator**: Fixed-size block allocation
- **Memory Tracker**: Per-tag allocation stats, leak detection
- **Memory Profiler UI**: Real-time display with history

## Profiling

- **CPU Profiler**: Scoped timing with hierarchical display (120-frame history)
- **GPU Profiler**: Per-render-pass timing with running averages
- **Render Stats**: Draw calls, triangles, state changes per frame
- **Memory Profiler**: Per-subsystem allocation tracking
