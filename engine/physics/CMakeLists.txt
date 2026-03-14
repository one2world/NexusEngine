add_library(nexus-physics STATIC
    src/physics_world_2d.cpp
    src/physics_world_3d.cpp
    src/physics_system.cpp
)

target_include_directories(nexus-physics
    PUBLIC  include
    PRIVATE src
)

target_link_libraries(nexus-physics
    PUBLIC Nexus::Core Nexus::Scene
)

nexus_set_compiler_flags(nexus-physics)
nexus_set_build_defines(nexus-physics)

add_library(Nexus::Physics ALIAS nexus-physics)
