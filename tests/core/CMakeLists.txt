add_executable(nexus-test-core
    test_math_types.cpp
    test_containers.cpp
    test_memory.cpp
    test_string_id.cpp
    test_job_system.cpp
)

target_link_libraries(nexus-test-core
    PRIVATE
        Nexus::Core
        gtest
        gtest_main
)

nexus_set_compiler_flags(nexus-test-core)

include(GoogleTest)
gtest_discover_tests(nexus-test-core)
