add_executable(nexus-test-assets
    test_asset_handle.cpp
    test_asset_registry.cpp
    test_asset_loader.cpp
    test_asset_package.cpp
    test_web_streaming.cpp
)

target_link_libraries(nexus-test-assets
    PRIVATE
        Nexus::Core
        Nexus::Assets
        gtest
        gtest_main
)

nexus_set_compiler_flags(nexus-test-assets)

include(GoogleTest)
gtest_discover_tests(nexus-test-assets)
