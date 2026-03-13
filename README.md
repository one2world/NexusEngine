# NexusEngine

A modern **2D + 3D hybrid game engine** built with C++20.

NexusEngine is designed from the ground up to seamlessly combine 2D and 3D rendering, physics, and gameplay in a single unified engine. Whether you're building a 2D platformer, a 3D adventure, or a game that mixes both, NexusEngine provides the tools you need.

## Features (Planned)

- **Hybrid 2D/3D Rendering** — Batch-optimized 2D sprite renderer and PBR-capable 3D renderer in the same scene
- **Unified ECS** — Archetype-based Entity Component System for both 2D and 3D game objects
- **Physics** — Box2D for 2D, Jolt Physics for 3D, with a common query API
- **Scripting** — Lua scripting with hot-reload
- **Audio** — Spatial and non-spatial audio via miniaudio
- **Animation** — Sprite animation, skeletal animation, state machines, blend trees
- **Visual Editor** — ImGui-based editor with scene editing, tilemap painting, and asset management
- **Cross-Platform** — Windows, Linux, macOS, Web (Emscripten)

## Building

### Prerequisites

- CMake 3.21+
- C++20 compiler (GCC 11+, Clang 14+, MSVC 2022+)
- Vulkan SDK (optional, for Vulkan backend)

### Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Run Sandbox

```bash
./build/sandbox/nexus-sandbox
```

### Run Tests

```bash
cd build && ctest --output-on-failure
```

## Project Structure

```
NexusEngine/
├── engine/
│   ├── core/          # Logging, math, memory, events, timers
│   ├── platform/      # Window, input (GLFW)
│   ├── rhi/           # Rendering hardware interface
│   ├── renderer/      # 2D + 3D renderers
│   ├── scene/         # ECS, scene graph
│   ├── physics/       # Box2D + Jolt integration
│   ├── audio/         # Audio system (miniaudio)
│   ├── animation/     # 2D + 3D animation
│   ├── ui/            # In-game UI
│   ├── scripting/     # Lua scripting (sol2)
│   ├── assets/        # Asset pipeline
│   └── network/       # Multiplayer networking
├── editor/            # Visual editor application
├── runtime/           # Standalone game runtime
├── sandbox/           # Demo / test project
└── tests/             # Unit tests
```

See [ROADMAP.md](ROADMAP.md) for the complete development plan.

## Tech Stack

| Component | Library |
|-----------|---------|
| Language | C++20 |
| Build | CMake |
| Graphics | Vulkan + OpenGL 4.5 |
| Windowing | GLFW |
| 2D Physics | Box2D |
| 3D Physics | Jolt Physics |
| Audio | miniaudio |
| Scripting | Lua (sol2) |
| Math | GLM |
| Logging | spdlog |
| Serialization | nlohmann/json |
| Editor UI | Dear ImGui |

## License

MIT License. See [LICENSE](LICENSE) for details.
