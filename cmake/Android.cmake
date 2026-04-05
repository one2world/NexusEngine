# ============================================================================
# Android NDK CMake overlay for NexusEngine
#
# This file is included when building for Android via the NDK toolchain.
#
# Usage:
#   cmake --preset android-arm64 -S . -B build-android
#
# Requires:
#   - Android NDK r25+ (set via ANDROID_NDK or NDK_ROOT env var)
#   - CMake 3.21+ with NDK toolchain support
# ============================================================================

message(STATUS "NexusEngine: Configuring for Android NDK")

# Validate NDK
if(NOT ANDROID_NDK AND NOT CMAKE_ANDROID_NDK)
    message(WARNING "ANDROID_NDK not set. Ensure the NDK toolchain is configured.")
endif()

# Platform settings
set(NEXUS_ENABLE_VULKAN  OFF CACHE BOOL "" FORCE)  # GLES3 first
set(NEXUS_ENABLE_OPENGL  OFF CACHE BOOL "" FORCE)
set(NEXUS_ENABLE_GLES    ON  CACHE BOOL "" FORCE)
set(NEXUS_BUILD_EDITOR   OFF CACHE BOOL "" FORCE)
set(NEXUS_BUILD_TESTS    OFF CACHE BOOL "" FORCE)

# Android API level
if(NOT ANDROID_PLATFORM)
    set(ANDROID_PLATFORM android-26)
endif()

# STL
if(NOT ANDROID_STL)
    set(ANDROID_STL c++_shared)
endif()

# Disable GLFW on Android (use native window)
set(NEXUS_USE_GLFW OFF CACHE BOOL "" FORCE)

# Helper function for Android targets
function(nexus_android_target TARGET)
    if(ANDROID)
        target_compile_definitions(${TARGET} PRIVATE
            NEXUS_PLATFORM_ANDROID=1
            NEXUS_GLES3=1
        )
        target_link_libraries(${TARGET} PRIVATE
            android
            log
            EGL
            GLESv3
        )
    endif()
endfunction()
