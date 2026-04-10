# NexusEngine

A modern **2D + 3D hybrid game engine** built with C++20.

NexusEngine is designed from the ground up to seamlessly combine 2D and 3D rendering, physics, and gameplay in a single unified engine. Whether you're building a 2D platformer, a 3D adventure, or a game that mixes both, NexusEngine provides the tools you need.

## Features

- **Hybrid 2D/3D Rendering** — Batch-optimized 2D sprite renderer and PBR-capable 3D renderer (deferred + forward) in the same scene
- **Unified ECS** — Sparse-set Entity Component System with generation-based handles for both 2D and 3D game objects
- **Physics** — Custom 2D physics (spatial hash broadphase, SAT collision, constraints) and custom 3D physics (rigid body, joints, raycasting)
- **Scripting** — Lua-like interpreter with standard library (math, string, table, os), hot-reload, coroutines
- **Audio** — Custom audio mixer with spatial 3D audio, bus system, DSP effects, WAV + OGG Vorbis decoding, platform audio output via miniaudio
- **Animation** — Sprite animation, skeletal animation with GPU skinning, IK solver, state machines, blend trees
- **Visual Editor** — ImGui-based editor with scene editing, tilemap painting, and asset management
- **Cross-Platform** — Windows, Linux, macOS, Web (Emscripten), Android (NDK)

## Building

### Prerequisites

- CMake 3.21+
- C++20 compiler (GCC 11+, Clang 14+, MSVC 2022+)

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
│   ├── physics/       # Custom 2D + 3D physics engines
│   ├── audio/         # Custom audio mixer + miniaudio device output
│   ├── animation/     # 2D + 3D animation
│   ├── ui/            # In-game UI
│   ├── scripting/     # Lua-like scripting interpreter
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
| Build | CMake 3.21+ |
| Graphics | OpenGL 4.5 + WebGL 2.0 |
| Windowing | GLFW 3.4 |
| 2D Physics | Custom engine (spatial hash, SAT, constraints) |
| 3D Physics | Custom engine (rigid body, joints, raycasting) |
| Audio | Custom mixer + miniaudio (device output) |
| Scripting | Custom Lua-like interpreter + standard library |
| Math | GLM |
| Logging | spdlog |
| Serialization | nlohmann/json + binary .nxs |
| Editor UI | Dear ImGui |
| Testing | Google Test (975 tests) |

## License

MIT License. See [LICENSE](LICENSE) for details.
