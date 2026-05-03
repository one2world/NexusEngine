add_library(nexus-animation STATIC
    src/sprite_animation.cpp
    src/skeleton.cpp
    src/animation_clip.cpp
    src/animation_state_machine.cpp
    src/particle_system.cpp
    src/ik_solver.cpp
    src/blend_tree.cpp
    src/animator_system.cpp
    src/particle_emitter_system.cpp
)

target_include_directories(nexus-animation
    PUBLIC  include
    PRIVATE src
)

target_link_libraries(nexus-animation
    PUBLIC Nexus::Core Nexus::Scene
)

nexus_set_compiler_flags(nexus-animation)
nexus_set_build_defines(nexus-animation)

add_library(Nexus::Animation ALIAS nexus-animation)
