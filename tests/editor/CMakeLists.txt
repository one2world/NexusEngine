add_executable(nexus-test-editor
    test_panel.cpp
    test_editor_panels.cpp
    test_undo_redo.cpp
    test_editor_state.cpp
    test_asset_thumbnail_cache.cpp
    test_component_registry.cpp
    test_asset_drop_importer.cpp
    test_layer_registry.cpp
    test_material_asset.cpp
    test_prefab_asset.cpp
    test_animation_asset.cpp
    test_audio_asset.cpp
    test_build_scenes.cpp
    test_particle_editor.cpp
)

target_link_libraries(nexus-test-editor
    PRIVATE
        Nexus::Core
        Nexus::Assets
        Nexus::EditorLib
        Nexus::Perf
        gtest
        gtest_main
)

nexus_set_compiler_flags(nexus-test-editor)

include(GoogleTest)
gtest_discover_tests(nexus-test-editor)
