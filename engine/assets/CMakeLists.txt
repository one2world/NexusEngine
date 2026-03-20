add_library(nexus-assets STATIC
    src/asset_handle.cpp
    src/asset_registry.cpp
    src/asset_loader.cpp
    src/asset_package.cpp
)

target_include_directories(nexus-assets
    PUBLIC  include
    PRIVATE src
)

target_link_libraries(nexus-assets
    PUBLIC Nexus::Core
)

nexus_set_compiler_flags(nexus-assets)
nexus_set_build_defines(nexus-assets)

add_library(Nexus::Assets ALIAS nexus-assets)
