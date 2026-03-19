add_executable(nexus-test-scripting
    test_script_value.cpp
    test_script_engine.cpp
    test_script_component.cpp
    test_engine_bindings.cpp
)

target_link_libraries(nexus-test-scripting
    PRIVATE
        Nexus::Core
        Nexus::Scene
        Nexus::Scripting
        gtest
        gtest_main
)

nexus_set_compiler_flags(nexus-test-scripting)

include(GoogleTest)
gtest_discover_tests(nexus-test-scripting)
