set(NEXUS_RHI_TEST_SOURCES "")

if(NEXUS_ENABLE_VULKAN)
    list(APPEND NEXUS_RHI_TEST_SOURCES test_vk_rhi.cpp)
endif()

# WebGL tests always run in CPU-simulated mode on desktop
list(APPEND NEXUS_RHI_TEST_SOURCES test_webgl_rhi.cpp)

if(NEXUS_RHI_TEST_SOURCES)
    add_executable(nexus-test-rhi ${NEXUS_RHI_TEST_SOURCES})

    target_link_libraries(nexus-test-rhi
        PRIVATE
            Nexus::Core
            Nexus::RHI
            gtest
            gtest_main
    )

    nexus_set_compiler_flags(nexus-test-rhi)

    include(GoogleTest)
    gtest_discover_tests(nexus-test-rhi)
endif()
