#pragma once

#include "nexus/core/types.h"
#include <string>
#include <functional>
#include <vector>

namespace nexus {

// ============================================================================
// CrashHandler - crash detection, signal handling, and minidump generation
// ============================================================================

struct CrashInfo {
    std::string signal_name;
    int signal_number{0};
    std::string message;
    std::vector<std::string> stack_trace;
    std::string dump_path;
    bool dump_written{false};
};

class CrashHandler {
public:
    using CrashCallback = std::function<void(const CrashInfo&)>;

    /// Install signal handlers for SIGSEGV, SIGABRT, SIGFPE, SIGBUS.
    static void install();

    /// Uninstall signal handlers (restore defaults).
    static void uninstall();

    /// Set a callback to be invoked before the crash dump is written.
    static void set_callback(CrashCallback callback);

    /// Set the output directory for minidump files.
    static void set_dump_directory(const std::string& dir);

    /// Get the dump directory.
    static const std::string& dump_directory();

    /// Manually trigger a crash report (for structured error handling).
    static void report_crash(const std::string& message);

    /// Check if crash handler is installed.
    static bool is_installed();

private:
    static void signal_handler(int sig);
    static void write_crash_dump(const CrashInfo& info);
    static std::vector<std::string> capture_stack_trace();
};

// ============================================================================
// Structured error codes - consistent error reporting across subsystems
// ============================================================================

enum class ErrorCode : u32 {
    OK = 0,

    // General (1xx)
    Unknown        = 100,
    InvalidArgument = 101,
    OutOfMemory    = 102,
    NotImplemented = 103,
    Timeout        = 104,

    // File I/O (2xx)
    FileNotFound   = 200,
    FileReadError  = 201,
    FileWriteError = 202,
    InvalidFormat  = 203,
    CorruptedData  = 204,

    // Rendering (3xx)
    ShaderCompileError = 300,
    PipelineError      = 301,
    TextureError       = 302,
    FramebufferError   = 303,
    ResourceExhausted  = 304,

    // Scene (4xx)
    EntityNotFound  = 400,
    ComponentError  = 401,
    SerializeError  = 402,
    DeserializeError = 403,
    SceneLoadError  = 404,

    // Audio (5xx)
    AudioDeviceError = 500,
    AudioFormatError = 501,
    AudioDecodeError = 502,

    // Network (6xx)
    ConnectionFailed = 600,
    ConnectionLost   = 601,
    PacketError      = 602,
    AuthFailed       = 603,

    // Scripting (7xx)
    ScriptSyntaxError  = 700,
    ScriptRuntimeError = 701,
    ScriptLoadError    = 702,

    // Physics (8xx)
    PhysicsOverflow = 800,
    CollisionError  = 801,
};

/// Convert error code to human-readable string.
const char* error_code_string(ErrorCode code);

/// Structured error result type.
struct ErrorResult {
    ErrorCode code{ErrorCode::OK};
    std::string message;

    bool ok() const { return code == ErrorCode::OK; }
    explicit operator bool() const { return ok(); }
};

} // namespace nexus
