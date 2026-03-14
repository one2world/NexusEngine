#pragma once

#include <spdlog/spdlog.h>
#include <memory>

namespace nexus {

class Log {
public:
    static void init();
    static void shutdown();

    static std::shared_ptr<spdlog::logger>& engine_logger() { return s_engine_logger; }
    static std::shared_ptr<spdlog::logger>& app_logger()    { return s_app_logger; }

private:
    static std::shared_ptr<spdlog::logger> s_engine_logger;
    static std::shared_ptr<spdlog::logger> s_app_logger;
};

} // namespace nexus

// Engine logging macros (null-safe: no-op if logger not initialized)
#define NX_TRACE(...)    do { if (::nexus::Log::engine_logger()) ::nexus::Log::engine_logger()->trace(__VA_ARGS__); } while(0)
#define NX_INFO(...)     do { if (::nexus::Log::engine_logger()) ::nexus::Log::engine_logger()->info(__VA_ARGS__); } while(0)
#define NX_WARN(...)     do { if (::nexus::Log::engine_logger()) ::nexus::Log::engine_logger()->warn(__VA_ARGS__); } while(0)
#define NX_ERROR(...)    do { if (::nexus::Log::engine_logger()) ::nexus::Log::engine_logger()->error(__VA_ARGS__); } while(0)
#define NX_CRITICAL(...) do { if (::nexus::Log::engine_logger()) ::nexus::Log::engine_logger()->critical(__VA_ARGS__); } while(0)

// Application logging macros (null-safe)
#define NX_APP_TRACE(...)    do { if (::nexus::Log::app_logger()) ::nexus::Log::app_logger()->trace(__VA_ARGS__); } while(0)
#define NX_APP_INFO(...)     do { if (::nexus::Log::app_logger()) ::nexus::Log::app_logger()->info(__VA_ARGS__); } while(0)
#define NX_APP_WARN(...)     do { if (::nexus::Log::app_logger()) ::nexus::Log::app_logger()->warn(__VA_ARGS__); } while(0)
#define NX_APP_ERROR(...)    do { if (::nexus::Log::app_logger()) ::nexus::Log::app_logger()->error(__VA_ARGS__); } while(0)
#define NX_APP_CRITICAL(...) do { if (::nexus::Log::app_logger()) ::nexus::Log::app_logger()->critical(__VA_ARGS__); } while(0)
