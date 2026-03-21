add_library(nexus-scripting STATIC
    src/script_value.cpp
    src/script_engine.cpp
    src/script_component.cpp
    src/engine_bindings.cpp
)

target_include_directories(nexus-scripting
    PUBLIC  include
    PRIVATE src
)

target_link_libraries(nexus-scripting
    PUBLIC Nexus::Core Nexus::Scene
    PRIVATE Nexus::Platform Nexus::Audio Nexus::Physics
)

nexus_set_compiler_flags(nexus-scripting)
nexus_set_build_defines(nexus-scripting)

add_library(Nexus::Scripting ALIAS nexus-scripting)
