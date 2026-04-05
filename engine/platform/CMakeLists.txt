set(NEXUS_PLATFORM_SOURCES
    src/window.cpp
    src/input.cpp
    src/input_action.cpp
    src/build_export.cpp
    src/touch_input.cpp
    src/emscripten_main.cpp
    src/android_bridge.cpp
)

add_library(nexus-platform STATIC ${NEXUS_PLATFORM_SOURCES})

target_include_directories(nexus-platform
    PUBLIC  include
    PRIVATE src
)

# GLFW is only used on desktop (not Emscripten/Android)
if(NOT EMSCRIPTEN AND NOT ANDROID)
    target_link_libraries(nexus-platform PUBLIC Nexus::Core glfw)
else()
    target_link_libraries(nexus-platform PUBLIC Nexus::Core)
endif()

nexus_set_compiler_flags(nexus-platform)
nexus_set_build_defines(nexus-platform)

add_library(Nexus::Platform ALIAS nexus-platform)
