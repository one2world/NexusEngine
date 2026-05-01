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
            -Wno-array-bounds
        >
        # GCC-only warnings that Clang/AppleClang rejects as unknown.
        $<$<CXX_COMPILER_ID:GNU>:
            -Wno-stringop-overflow -Wno-maybe-uninitialized
        >
        # Apple SDK headers (Carbon, CoreAudio, CoreFoundation) emit invalid
        # UTF-8 comments, Clang-nullability annotations, variadic macros, and
        # deprecated-declaration attributes that our -Werror would otherwise
        # treat as user-code bugs. These are SDK issues, not engine issues.
        $<$<AND:$<PLATFORM_ID:Darwin>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>:
            -Wno-invalid-utf8
            -Wno-nullability-extension
            -Wno-variadic-macros
            -Wno-deprecated-declarations
            -Wno-deprecated-anon-enum-enum-conversion
            -Wno-four-char-constants
            -Wno-zero-length-array
            -Wno-gnu-anonymous-struct
            -Wno-nested-anon-types
            -Wno-unguarded-availability-new
            -Wno-missing-method-return-type
            -Wno-availability
            -Wno-deprecated-literal-operator
            -Wno-deprecated-enum-enum-conversion
            -Wno-tautological-compare
        >
    )

    # Sanitizers in debug (opt-out for toolchains where ASAN deadlocks at startup,
    # e.g. macOS 26 beta + AppleClang). Override with -DNEXUS_ENABLE_SANITIZERS=OFF.
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NEXUS_ENABLE_SANITIZERS)
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
