add_library(nexus-renderer STATIC
    src/batch_renderer_2d.cpp
    src/forward_renderer_3d.cpp
    src/material.cpp
    src/debug_renderer.cpp
    src/pbr_renderer.cpp
    src/shadow_map.cpp
    src/post_process.cpp
    src/deferred_renderer.cpp
    src/terrain.cpp
    src/fog.cpp
    src/ssao.cpp
    src/particle_renderer.cpp
    src/tilemap_renderer.cpp
    src/skybox.cpp
    src/taa.cpp
    src/skinned_mesh_renderer.cpp
    src/shader_library.cpp
    src/light_probe.cpp
    src/gpu_particles.cpp
    src/decal_system.cpp
)

target_include_directories(nexus-renderer
    PUBLIC  include
    PRIVATE src
)

target_link_libraries(nexus-renderer
    PUBLIC Nexus::Core Nexus::RHI Nexus::Animation
)

nexus_set_compiler_flags(nexus-renderer)
nexus_set_build_defines(nexus-renderer)

add_library(Nexus::Renderer ALIAS nexus-renderer)
