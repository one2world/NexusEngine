# Editor library (testable)
add_library(nexus-editor-lib STATIC
    src/panel.cpp
    src/editor_panels.cpp
    src/undo_redo.cpp
    src/editor_state.cpp
    src/gizmo.cpp
    src/editor_tools.cpp
)

target_include_directories(nexus-editor-lib
    PUBLIC include
    PRIVATE src
)

target_link_libraries(nexus-editor-lib
    PUBLIC
        Nexus::Core
        Nexus::Scene
        Nexus::Renderer
        imgui
)

target_compile_definitions(nexus-editor-lib PUBLIC NEXUS_HAS_IMGUI=1)

nexus_set_compiler_flags(nexus-editor-lib)
nexus_set_build_defines(nexus-editor-lib)

add_library(Nexus::EditorLib ALIAS nexus-editor-lib)
