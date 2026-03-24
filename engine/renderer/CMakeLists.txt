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
