add_library(nexus-platform STATIC
    src/window.cpp
    src/input.cpp
    src/input_action.cpp
    src/build_export.cpp
    src/touch_input.cpp
)

target_include_directories(nexus-platform
    PUBLIC  include
    PRIVATE src
)

target_link_libraries(nexus-platform
    PUBLIC  Nexus::Core glfw
)

nexus_set_compiler_flags(nexus-platform)
nexus_set_build_defines(nexus-platform)

add_library(Nexus::Platform ALIAS nexus-platform)
