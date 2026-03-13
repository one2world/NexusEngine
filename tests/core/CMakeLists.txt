add_executable(nexus-test-core test_math_types.cpp)

target_link_libraries(nexus-test-core
    PRIVATE
        Nexus::Core
        GTest::gtest
        GTest::gtest_main
)

nexus_set_compiler_flags(nexus-test-core)

include(GoogleTest)
gtest_discover_tests(nexus-test-core)
