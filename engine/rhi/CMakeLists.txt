set(NEXUS_RHI_SOURCES
    src/gl_functions.cpp
    src/gl_rhi.cpp
)

if(NEXUS_ENABLE_VULKAN)
    list(APPEND NEXUS_RHI_SOURCES src/vk_rhi.cpp)
endif()

# WebGL backend always compiles (CPU-simulated on non-Emscripten platforms)
list(APPEND NEXUS_RHI_SOURCES src/webgl_rhi.cpp)

add_library(nexus-rhi STATIC ${NEXUS_RHI_SOURCES})

target_include_directories(nexus-rhi
    PUBLIC  include
    PRIVATE src
)

target_link_libraries(nexus-rhi
    PUBLIC Nexus::Core
)

if(NEXUS_ENABLE_VULKAN)
    target_compile_definitions(nexus-rhi PUBLIC NEXUS_ENABLE_VULKAN=1)
endif()

if(NEXUS_ENABLE_OPENGL)
    target_compile_definitions(nexus-rhi PUBLIC NEXUS_ENABLE_OPENGL=1)
endif()

if(NEXUS_ENABLE_WEBGL)
    target_compile_definitions(nexus-rhi PUBLIC NEXUS_ENABLE_WEBGL=1)
endif()

nexus_set_compiler_flags(nexus-rhi)
nexus_set_build_defines(nexus-rhi)

add_library(Nexus::RHI ALIAS nexus-rhi)
