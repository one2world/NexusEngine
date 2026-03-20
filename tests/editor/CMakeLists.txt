add_executable(nexus-test-editor
    test_panel.cpp
    test_editor_panels.cpp
    test_undo_redo.cpp
    test_editor_state.cpp
)

target_link_libraries(nexus-test-editor
    PRIVATE
        Nexus::Core
        Nexus::EditorLib
        gtest
        gtest_main
)

nexus_set_compiler_flags(nexus-test-editor)

include(GoogleTest)
gtest_discover_tests(nexus-test-editor)
