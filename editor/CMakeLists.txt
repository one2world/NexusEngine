# Editor library (testable)
add_library(nexus-editor-lib STATIC
    src/panel.cpp
    src/editor_panels.cpp
    src/undo_redo.cpp
    src/editor_state.cpp
    src/gizmo.cpp
    src/editor_tools.cpp
    src/asset_thumbnail_cache.cpp
    src/asset_thumbnail_stb.cpp
    src/component_registry.cpp
    src/asset_drop_importer.cpp
    src/layer_registry.cpp
    src/material_asset.cpp
    src/prefab_asset.cpp
    src/animation_asset.cpp
)

# stb_image is a third-party single-header that emits sign/cast warnings
# under our strict `-Werror` flag set.  Silence them only for the isolated
# implementation TU so first-party code still benefits from -Werror.
if(NOT MSVC)
    set_source_files_properties(
        src/asset_thumbnail_stb.cpp
        PROPERTIES COMPILE_FLAGS
        "-Wno-sign-conversion -Wno-implicit-int-conversion -Wno-cast-qual -Wno-unused-but-set-variable -Wno-unused-function -Wno-double-promotion"
    )
else()
    set_source_files_properties(
        src/asset_thumbnail_stb.cpp
        PROPERTIES COMPILE_FLAGS "/W0"
    )
endif()

target_include_directories(nexus-editor-lib
    PUBLIC include
    PRIVATE src
    PRIVATE ${stb_SOURCE_DIR}
)

target_link_libraries(nexus-editor-lib
    PUBLIC
        Nexus::Core
        Nexus::Scene
        Nexus::Renderer
        Nexus::RHI
        Nexus::Platform
        Nexus::Perf
        Nexus::Assets
        Nexus::Animation
        imgui
        imguizmo
)

target_compile_definitions(nexus-editor-lib PUBLIC NEXUS_HAS_IMGUI=1)

nexus_set_compiler_flags(nexus-editor-lib)
nexus_set_build_defines(nexus-editor-lib)

add_library(Nexus::EditorLib ALIAS nexus-editor-lib)
