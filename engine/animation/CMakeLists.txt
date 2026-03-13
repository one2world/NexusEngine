add_library(nexus-animation STATIC src/placeholder.cpp)

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
