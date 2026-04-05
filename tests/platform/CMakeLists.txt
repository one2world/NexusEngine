add_executable(nexus-test-platform
    test_input_action.cpp
    test_touch_input.cpp
)

target_link_libraries(nexus-test-platform
    PRIVATE Nexus::Platform GTest::gtest_main
)

nexus_set_compiler_flags(nexus-test-platform)

include(GoogleTest)
gtest_discover_tests(nexus-test-platform)
