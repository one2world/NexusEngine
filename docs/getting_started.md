# Getting Started with NexusEngine

## Prerequisites

- C++20 compatible compiler (GCC 11+, Clang 14+, MSVC 2022+)
- CMake 3.21 or higher
- OpenGL 4.5 compatible GPU
- Git

## Building

```bash
# Clone the repository
git clone https://github.com/your-org/NexusEngine.git
cd NexusEngine

# Configure and build
cmake --preset default
cmake --build build --parallel

# Run tests
ctest --test-dir build --output-on-failure
```

## Project Structure

```
NexusEngine/
  engine/          - Core engine libraries
  editor/          - Editor application (ImGui-based)
  tests/           - Unit and integration tests
  sandbox/         - Sandbox/demo application
  docs/            - Documentation
  CMakeLists.txt   - Root build configuration
  ROADMAP.md       - Development roadmap and feature status
```

## Creating Your First Scene

### 1. Set up the ECS

```cpp
#include <nexus/scene/registry.h>
#include <nexus/scene/components.h>

nexus::Registry registry;

// Create an entity
auto player = registry.create();
registry.add_component<nexus::TagComponent>(player, {"Player"});
registry.add_component<nexus::Transform3DComponent>(player, {});

// Set position
auto& transform = registry.get_component<nexus::Transform3DComponent>(player);
transform.position = nexus::Vec3(0.0f, 1.0f, 0.0f);
```

### 2. Add a Camera

```cpp
auto camera_entity = registry.create();
registry.add_component<nexus::TagComponent>(camera_entity, {"MainCamera"});
registry.add_component<nexus::Transform3DComponent>(camera_entity, {});

nexus::CameraComponent cam;
cam.fov = 60.0f;
cam.near_plane = 0.1f;
cam.far_plane = 1000.0f;
registry.add_component<nexus::CameraComponent>(camera_entity, cam);
```

### 3. Set Up Rendering

```cpp
#include <nexus/renderer/forward_renderer_3d.h>

// Initialize the renderer with the RHI
nexus::ForwardRenderer3D renderer;
renderer.init(&rhi);

// In the game loop:
renderer.begin_frame(camera);
renderer.submit(mesh_vbo, mesh_ibo, index_count, model_matrix, material);
renderer.end_frame();
```

### 4. Add Lighting

```cpp
// Directional light
nexus::DirectionalLightComponent sun;
sun.direction = nexus::Vec3(-0.5f, -1.0f, -0.3f);
sun.color = nexus::Vec3(1.0f, 0.95f, 0.9f);
sun.intensity = 1.2f;

auto sun_entity = registry.create();
registry.add_component<nexus::DirectionalLightComponent>(sun_entity, sun);
```

### 5. Load Assets

```cpp
#include <nexus/assets/asset_registry.h>

nexus::AssetRegistry assets;
assets.set_base_path("assets/");

// Async load a texture
auto tex_handle = assets.load_async<nexus::Texture>("textures/brick.bmp");

// Load a mesh
auto mesh_handle = assets.load<nexus::Mesh>("meshes/cube.obj");

// Load a glTF scene
#include <nexus/assets/gltf_importer.h>
nexus::assets::GltfScene gltf_scene;
nexus::assets::parse_gltf(json_string, bin_data, base_dir, gltf_scene);
```

### 6. Audio

```cpp
#include <nexus/audio/audio_engine.h>

nexus::audio::AudioEngine audio;
audio.init();

// Play a sound
auto buffer = audio.load_buffer("sounds/explosion.wav");
auto source = audio.create_source();
audio.play(source, buffer);

// 3D spatial audio
audio.set_source_position(source, nexus::Vec3(10.0f, 0.0f, 5.0f));
audio.set_listener_position(nexus::Vec3(0.0f));
```

### 7. Input

```cpp
#include <nexus/platform/input.h>

// Keyboard
if (nexus::Input::key_pressed(nexus::Key::Space)) {
    // Jump
}

// Mouse
nexus::Vec2 mouse_delta = nexus::Input::mouse_delta();

// Touch (mobile)
#include <nexus/platform/touch_input.h>
nexus::TouchInput touch;
touch.update(current_time);
if (touch.gestures().tapped) {
    // Handle tap at touch.gestures().tap_position
}
```

## Running the Editor

```bash
./build/nexus-editor
```

The editor provides:
- **Hierarchy Panel**: View and manage scene entities
- **Inspector Panel**: Edit component properties
- **Viewport**: 3D/2D scene rendering
- **Console**: Log output with filtering
- **Asset Browser**: Navigate and import assets

## Running Tests

```bash
# All tests
ctest --test-dir build --output-on-failure -j$(nproc)

# Specific subsystem
ctest --test-dir build -R "ECS" --output-on-failure
ctest --test-dir build -R "StressTest" --output-on-failure
ctest --test-dir build -R "Renderer" --output-on-failure
```

## Configuration

### CMake Presets

- `default` — Debug build with sanitizers
- `release` — Optimized release build
- `ci` — CI/CD build configuration

### Build Defines

- `NEXUS_HAS_IMGUI` — Enable ImGui editor panels
- `NEXUS_DEBUG` — Enable debug assertions and logging
- `NEXUS_PROFILE` — Enable profiling instrumentation
