#include "nexus/core/crash_handler.h"
#include "nexus/core/log.h"
#include <csignal>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef __linux__
#include <execinfo.h>
#include <unistd.h>
#endif

namespace nexus {

// ── Static state ───────────────────────────────────────────────────────────

static bool s_installed = false;
static CrashHandler::CrashCallback s_callback;
static std::string s_dump_dir = "crash_dumps";

// Previous signal handlers (for restore)
static struct sigaction s_prev_segv;
static struct sigaction s_prev_abrt;
static struct sigaction s_prev_fpe;

// ── CrashHandler implementation ────────────────────────────────────────────

void CrashHandler::install() {
    if (s_installed) return;

    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = static_cast<int>(SA_RESETHAND); // One-shot: restore default after handling

    sigaction(SIGSEGV, &sa, &s_prev_segv);
    sigaction(SIGABRT, &sa, &s_prev_abrt);
    sigaction(SIGFPE,  &sa, &s_prev_fpe);

    s_installed = true;
    NX_INFO("CrashHandler installed");
}

void CrashHandler::uninstall() {
    if (!s_installed) return;

    sigaction(SIGSEGV, &s_prev_segv, nullptr);
    sigaction(SIGABRT, &s_prev_abrt, nullptr);
    sigaction(SIGFPE,  &s_prev_fpe, nullptr);

    s_installed = false;
    NX_INFO("CrashHandler uninstalled");
}

void CrashHandler::set_callback(CrashCallback callback) {
    s_callback = std::move(callback);
}

void CrashHandler::set_dump_directory(const std::string& dir) {
    s_dump_dir = dir;
}

const std::string& CrashHandler::dump_directory() {
    return s_dump_dir;
}

bool CrashHandler::is_installed() {
    return s_installed;
}

void CrashHandler::signal_handler(int sig) {
    CrashInfo info;
    info.signal_number = sig;

    switch (sig) {
        case SIGSEGV: info.signal_name = "SIGSEGV"; info.message = "Segmentation fault"; break;
        case SIGABRT: info.signal_name = "SIGABRT"; info.message = "Abort signal received"; break;
        case SIGFPE:  info.signal_name = "SIGFPE";  info.message = "Floating point exception"; break;
        default:      info.signal_name = "UNKNOWN"; info.message = "Unknown signal"; break;
    }

    info.stack_trace = capture_stack_trace();

    // Invoke user callback
    if (s_callback) {
        s_callback(info);
    }

    // Write crash dump
    write_crash_dump(info);

    // Re-raise to get default behavior (core dump, etc.)
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

std::vector<std::string> CrashHandler::capture_stack_trace() {
    std::vector<std::string> trace;

#ifdef __linux__
    constexpr int MAX_FRAMES = 64;
    void* frames[MAX_FRAMES];
    int frame_count = backtrace(frames, MAX_FRAMES);
    char** symbols = backtrace_symbols(frames, frame_count);

    if (symbols) {
        for (int i = 0; i < frame_count; ++i) {
            trace.emplace_back(symbols[i]);
        }
        free(symbols);
    }
#else
    trace.emplace_back("<stack trace not available on this platform>");
#endif

    return trace;
}

void CrashHandler::write_crash_dump(const CrashInfo& info) {
    // Create dump directory if it doesn't exist
    std::error_code ec;
    std::filesystem::create_directories(s_dump_dir, ec);

    // Generate filename with timestamp
    std::time_t now = std::time(nullptr);
    char time_buf[64];
    std::strftime(time_buf, sizeof(time_buf), "%Y%m%d_%H%M%S", std::localtime(&now));

    std::string dump_path = s_dump_dir + "/crash_" + time_buf + ".log";

    std::ofstream file(dump_path);
    if (!file.is_open()) return;

    file << "=== NexusEngine Crash Report ===\n";
    file << "Signal: " << info.signal_name << " (" << info.signal_number << ")\n";
    file << "Message: " << info.message << "\n";
    file << "Time: " << time_buf << "\n";
    file << "\n=== Stack Trace ===\n";

    for (size_t i = 0; i < info.stack_trace.size(); ++i) {
        file << "  #" << i << " " << info.stack_trace[i] << "\n";
    }

    file << "\n=== End Report ===\n";
    file.close();
}

void CrashHandler::report_crash(const std::string& message) {
    CrashInfo info;
    info.signal_name = "USER_REPORT";
    info.signal_number = 0;
    info.message = message;
    info.stack_trace = capture_stack_trace();

    if (s_callback) {
        s_callback(info);
    }

    write_crash_dump(info);
    NX_ERROR("Crash reported: {}", message);
}

// ── Error codes ────────────────────────────────────────────────────────────

const char* error_code_string(ErrorCode code) {
    switch (code) {
        case ErrorCode::OK:               return "OK";
        case ErrorCode::Unknown:          return "Unknown error";
        case ErrorCode::InvalidArgument:  return "Invalid argument";
        case ErrorCode::OutOfMemory:      return "Out of memory";
        case ErrorCode::NotImplemented:   return "Not implemented";
        case ErrorCode::Timeout:          return "Operation timed out";
        case ErrorCode::FileNotFound:     return "File not found";
        case ErrorCode::FileReadError:    return "File read error";
        case ErrorCode::FileWriteError:   return "File write error";
        case ErrorCode::InvalidFormat:    return "Invalid format";
        case ErrorCode::CorruptedData:    return "Corrupted data";
        case ErrorCode::ShaderCompileError: return "Shader compile error";
        case ErrorCode::PipelineError:    return "Pipeline error";
        case ErrorCode::TextureError:     return "Texture error";
        case ErrorCode::FramebufferError: return "Framebuffer error";
        case ErrorCode::ResourceExhausted: return "Resource exhausted";
        case ErrorCode::EntityNotFound:   return "Entity not found";
        case ErrorCode::ComponentError:   return "Component error";
        case ErrorCode::SerializeError:   return "Serialization error";
        case ErrorCode::DeserializeError: return "Deserialization error";
        case ErrorCode::SceneLoadError:   return "Scene load error";
        case ErrorCode::AudioDeviceError: return "Audio device error";
        case ErrorCode::AudioFormatError: return "Audio format error";
        case ErrorCode::AudioDecodeError: return "Audio decode error";
        case ErrorCode::ConnectionFailed: return "Connection failed";
        case ErrorCode::ConnectionLost:   return "Connection lost";
        case ErrorCode::PacketError:      return "Packet error";
        case ErrorCode::AuthFailed:       return "Authentication failed";
        case ErrorCode::ScriptSyntaxError:  return "Script syntax error";
        case ErrorCode::ScriptRuntimeError: return "Script runtime error";
        case ErrorCode::ScriptLoadError:  return "Script load error";
        case ErrorCode::PhysicsOverflow:  return "Physics overflow";
        case ErrorCode::CollisionError:   return "Collision error";
    }
    return "Unknown error code";
}

} // namespace nexus
