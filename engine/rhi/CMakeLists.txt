add_library(nexus-rhi STATIC
    src/placeholder.cpp
    src/gl_functions.cpp
)

target_include_directories(nexus-rhi
    PUBLIC  include
    PRIVATE src
)

target_link_libraries(nexus-rhi
    PUBLIC Nexus::Core
)

nexus_set_compiler_flags(nexus-rhi)
nexus_set_build_defines(nexus-rhi)

add_library(Nexus::RHI ALIAS nexus-rhi)
