add_library(nexus-physics STATIC src/placeholder.cpp)

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
