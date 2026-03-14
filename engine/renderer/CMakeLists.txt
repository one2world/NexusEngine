add_library(nexus-renderer STATIC
    src/batch_renderer_2d.cpp
    src/forward_renderer_3d.cpp
)

target_include_directories(nexus-renderer
    PUBLIC  include
    PRIVATE src
)

target_link_libraries(nexus-renderer
    PUBLIC Nexus::Core Nexus::RHI
)

nexus_set_compiler_flags(nexus-renderer)
nexus_set_build_defines(nexus-renderer)

add_library(Nexus::Renderer ALIAS nexus-renderer)
