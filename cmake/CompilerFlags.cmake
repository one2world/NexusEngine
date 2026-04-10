# Compiler warning flags
function(nexus_set_compiler_flags target)
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:
            /W4 /WX /permissive- /Zc:__cplusplus
            /wd4201  # nameless struct/union
        >
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:
            -Wall -Wextra -Wpedantic -Werror
            -Wno-unused-parameter
            -Wconversion -Wsign-conversion
            -Wno-array-bounds -Wno-stringop-overflow -Wno-maybe-uninitialized
        >
    )

    # Sanitizers in debug
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(${target} PRIVATE
            $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:
                -fsanitize=address,undefined
            >
        )
        target_link_options(${target} PRIVATE
            $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:
                -fsanitize=address,undefined
            >
        )
    endif()

    # Profile build type
    if(CMAKE_BUILD_TYPE STREQUAL "Profile")
        target_compile_options(${target} PRIVATE -O2 -g -DNDEBUG)
    endif()
endfunction()

# Define NEXUS_DEBUG / NEXUS_RELEASE
function(nexus_set_build_defines target)
    target_compile_definitions(${target} PRIVATE
        $<$<CONFIG:Debug>:NEXUS_DEBUG>
        $<$<CONFIG:Release>:NEXUS_RELEASE>
        $<$<CONFIG:RelWithDebInfo>:NEXUS_RELEASE>
        NEXUS_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        NEXUS_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        NEXUS_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    )
endfunction()
