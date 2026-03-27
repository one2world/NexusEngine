add_library(nexus-ui STATIC
    src/ui_core.cpp
    src/widgets.cpp
    src/ui_system.cpp
    src/bitmap_font.cpp
)

target_include_directories(nexus-ui
    PUBLIC  include
    PRIVATE src
)

target_link_libraries(nexus-ui
    PUBLIC Nexus::Core Nexus::RHI Nexus::Renderer
)

nexus_set_compiler_flags(nexus-ui)
nexus_set_build_defines(nexus-ui)

add_library(Nexus::UI ALIAS nexus-ui)
