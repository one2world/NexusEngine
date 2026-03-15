add_executable(nexus-test-ui
    test_ui_core.cpp
    test_widgets.cpp
    test_ui_system.cpp
)

target_link_libraries(nexus-test-ui
    PRIVATE
        Nexus::Core
        Nexus::RHI
        Nexus::Renderer
        Nexus::UI
        gtest
        gtest_main
)

nexus_set_compiler_flags(nexus-test-ui)

include(GoogleTest)
gtest_discover_tests(nexus-test-ui)
