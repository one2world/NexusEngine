# Editor executable
add_executable(nexus-editor
    ../src/editor_main.cpp
    ../src/imgui_backends.cpp
)

target_include_directories(nexus-editor
    PRIVATE ../include ../src
)

# ImGui backends need these flags — suppress third-party warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(../src/imgui_backends.cpp PROPERTIES COMPILE_FLAGS "-w")
endif()

target_link_libraries(nexus-editor
    PRIVATE
        Nexus::EditorLib
        Nexus::Core
        Nexus::Platform
        Nexus::RHI
        Nexus::Renderer
        Nexus::Scene
        Nexus::Physics
        Nexus::Audio
        Nexus::Animation
        Nexus::UI
        Nexus::Scripting
        Nexus::Assets
        Nexus::Network
        imgui
        glfw
)

nexus_set_compiler_flags(nexus-editor)
nexus_set_build_defines(nexus-editor)
