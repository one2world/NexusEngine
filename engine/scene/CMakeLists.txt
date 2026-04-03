add_library(nexus-scene STATIC
    src/scene.cpp
    src/hierarchy.cpp
    src/scene_serializer.cpp
    src/system_scheduler.cpp
    src/scene_manager.cpp
    src/prefab.cpp
    src/game_state_serializer.cpp
    src/binary_serializer.cpp
)

target_include_directories(nexus-scene
    PUBLIC  include
    PRIVATE src
)

target_link_libraries(nexus-scene
    PUBLIC Nexus::Core
)

nexus_set_compiler_flags(nexus-scene)
nexus_set_build_defines(nexus-scene)

add_library(Nexus::Scene ALIAS nexus-scene)
