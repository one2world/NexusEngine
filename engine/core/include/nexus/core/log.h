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

// Engine logging macros
#define NX_TRACE(...)    ::nexus::Log::engine_logger()->trace(__VA_ARGS__)
#define NX_INFO(...)     ::nexus::Log::engine_logger()->info(__VA_ARGS__)
#define NX_WARN(...)     ::nexus::Log::engine_logger()->warn(__VA_ARGS__)
#define NX_ERROR(...)    ::nexus::Log::engine_logger()->error(__VA_ARGS__)
#define NX_CRITICAL(...) ::nexus::Log::engine_logger()->critical(__VA_ARGS__)

// Application logging macros
#define NX_APP_TRACE(...)    ::nexus::Log::app_logger()->trace(__VA_ARGS__)
#define NX_APP_INFO(...)     ::nexus::Log::app_logger()->info(__VA_ARGS__)
#define NX_APP_WARN(...)     ::nexus::Log::app_logger()->warn(__VA_ARGS__)
#define NX_APP_ERROR(...)    ::nexus::Log::app_logger()->error(__VA_ARGS__)
#define NX_APP_CRITICAL(...) ::nexus::Log::app_logger()->critical(__VA_ARGS__)
