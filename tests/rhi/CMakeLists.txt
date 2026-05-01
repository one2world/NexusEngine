set(NEXUS_RHI_TEST_SOURCES "")

# Tests are only built when the corresponding backend is compiled.  Release
# builds must not link against a backend whose only implementation path is the
# CPU-simulated dev stub.
if(NEXUS_ENABLE_VULKAN AND NEXUS_HAVE_VULKAN_SDK)
    list(APPEND NEXUS_RHI_TEST_SOURCES test_vk_rhi.cpp)
endif()

if(EMSCRIPTEN OR NEXUS_ENABLE_WEBGL)
    list(APPEND NEXUS_RHI_TEST_SOURCES test_webgl_rhi.cpp)
endif()

if(NEXUS_ENABLE_METAL AND NEXUS_HAVE_METAL)
    list(APPEND NEXUS_RHI_TEST_SOURCES test_metal_rhi.cpp)
endif()

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
