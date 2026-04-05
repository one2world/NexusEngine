add_library(nexus-perf STATIC
    src/profiler.cpp
    src/memory_tracker.cpp
    src/render_stats.cpp
    src/occlusion_culling.cpp
    src/lod_system.cpp
    src/instancing.cpp
    src/thread_pool.cpp
    src/memory_profiler_ui.cpp
    src/gpu_profiler.cpp
    src/frame_graph.cpp
)

target_include_directories(nexus-perf
    PUBLIC  include
    PRIVATE src
)

target_link_libraries(nexus-perf
    PUBLIC
        Nexus::Core
)

nexus_set_compiler_flags(nexus-perf)
nexus_set_build_defines(nexus-perf)

add_library(Nexus::Perf ALIAS nexus-perf)
