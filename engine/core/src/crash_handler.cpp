#include "nexus/core/crash_handler.h"
#include "nexus/core/log.h"
#include <csignal>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

// ── Platform-specific includes ──────────────────────────────────────────────

#if defined(__linux__) && !defined(__ANDROID__)
    #define NEXUS_PLATFORM_LINUX 1
    #include <execinfo.h>
    #include <unistd.h>
    #include <cxxabi.h>
#elif defined(__APPLE__)
    #define NEXUS_PLATFORM_MACOS 1
    #include <execinfo.h>
    #include <unistd.h>
    #include <cxxabi.h>
#elif defined(_WIN32)
    #define NEXUS_PLATFORM_WINDOWS 1
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <dbghelp.h>
    #pragma comment(lib, "dbghelp.lib")
#elif defined(__ANDROID__)
    #define NEXUS_PLATFORM_ANDROID 1
    #include <unwind.h>
    #include <dlfcn.h>
    #include <android/log.h>
#elif defined(__EMSCRIPTEN__)
    #define NEXUS_PLATFORM_EMSCRIPTEN 1
    #include <emscripten.h>
#endif

namespace nexus {

// ── Static state ────────────────────────────────────────────────────────────

static bool s_installed = false;
static CrashHandler::CrashCallback s_callback;
static std::string s_dump_dir = "crash_dumps";

#if !defined(NEXUS_PLATFORM_WINDOWS)
// POSIX signal handlers
static struct sigaction s_prev_segv;
static struct sigaction s_prev_abrt;
static struct sigaction s_prev_fpe;
static struct sigaction s_prev_bus;
static struct sigaction s_prev_ill;
#else
// Windows exception handler
static LPTOP_LEVEL_EXCEPTION_FILTER s_prev_exception_filter = nullptr;
#endif

// ── Stack trace helpers ─────────────────────────────────────────────────────

#if defined(NEXUS_PLATFORM_LINUX) || defined(NEXUS_PLATFORM_MACOS)

static std::string demangle_symbol(const char* symbol) {
    // Try to extract and demangle the C++ symbol name
    // Format: "module(mangled+0xoffset) [addr]"
    std::string result(symbol);
    const char* begin = nullptr;
    const char* end = nullptr;

    for (const char* p = symbol; *p; ++p) {
        if (*p == '(') begin = p + 1;
        else if (*p == '+' && begin) { end = p; break; }
    }

    if (begin && end && end > begin) {
        std::string mangled(begin, static_cast<size_t>(end - begin));
        int status = 0;
        char* demangled = abi::__cxa_demangle(mangled.c_str(), nullptr, nullptr, &status);
        if (status == 0 && demangled) {
            result = std::string(symbol, static_cast<size_t>(begin - symbol))
                   + demangled
                   + std::string(end);
            free(demangled);
        }
    }
    return result;
}

#elif defined(NEXUS_PLATFORM_ANDROID)

struct AndroidUnwindState {
    void** frames;
    int frame_count;
    int max_frames;
};

static _Unwind_Reason_Code android_unwind_callback(struct _Unwind_Context* context, void* arg) {
    auto* state = static_cast<AndroidUnwindState*>(arg);
    if (state->frame_count >= state->max_frames) return _URC_END_OF_STACK;

    uintptr_t pc = _Unwind_GetIP(context);
    if (pc) {
        state->frames[state->frame_count++] = reinterpret_cast<void*>(pc);
    }
    return _URC_NO_REASON;
}

#elif defined(NEXUS_PLATFORM_WINDOWS)

static LONG WINAPI windows_exception_handler(EXCEPTION_POINTERS* exception_info);

#endif

// ── CrashHandler::capture_stack_trace ───────────────────────────────────────

std::vector<std::string> CrashHandler::capture_stack_trace() {
    std::vector<std::string> trace;

#if defined(NEXUS_PLATFORM_LINUX) || defined(NEXUS_PLATFORM_MACOS)
    constexpr int MAX_FRAMES = 64;
    void* frames[MAX_FRAMES];
    int frame_count = backtrace(frames, MAX_FRAMES);
    char** symbols = backtrace_symbols(frames, frame_count);

    if (symbols) {
        for (int i = 2; i < frame_count; ++i) { // Skip signal handler frames
            trace.push_back(demangle_symbol(symbols[i]));
        }
        free(symbols);
    }

#elif defined(NEXUS_PLATFORM_ANDROID)
    constexpr int MAX_FRAMES = 64;
    void* frames[MAX_FRAMES];
    AndroidUnwindState state{frames, 0, MAX_FRAMES};
    _Unwind_Backtrace(android_unwind_callback, &state);

    for (int i = 2; i < state.frame_count; ++i) {
        Dl_info info;
        if (dladdr(frames[i], &info) && info.dli_sname) {
            std::ostringstream ss;
            ss << "#" << (i - 2) << " " << info.dli_fname << " " << info.dli_sname;
            trace.push_back(ss.str());
        } else {
            std::ostringstream ss;
            ss << "#" << (i - 2) << " [" << frames[i] << "]";
            trace.push_back(ss.str());
        }
    }

#elif defined(NEXUS_PLATFORM_WINDOWS)
    constexpr DWORD MAX_FRAMES = 64;
    void* frames[MAX_FRAMES];
    USHORT frame_count = CaptureStackBackTrace(2, MAX_FRAMES, frames, nullptr);

    HANDLE process = GetCurrentProcess();
    SymInitialize(process, NULL, TRUE);

    char symbol_buffer[sizeof(SYMBOL_INFO) + 256];
    auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbol_buffer);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 255;

    for (USHORT i = 0; i < frame_count; ++i) {
        DWORD64 address = reinterpret_cast<DWORD64>(frames[i]);
        if (SymFromAddr(process, address, nullptr, symbol)) {
            IMAGEHLP_LINE64 line;
            line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
            DWORD displacement;

            std::ostringstream ss;
            ss << "#" << i << " " << symbol->Name;
            if (SymGetLineFromAddr64(process, address, &displacement, &line)) {
                ss << " at " << line.FileName << ":" << line.LineNumber;
            }
            trace.push_back(ss.str());
        } else {
            std::ostringstream ss;
            ss << "#" << i << " [0x" << std::hex << address << "]";
            trace.push_back(ss.str());
        }
    }
    SymCleanup(process);

#elif defined(NEXUS_PLATFORM_EMSCRIPTEN)
    trace.emplace_back("<Emscripten stack trace: use browser DevTools for detailed trace>");
    // In Emscripten, stack traces are better captured by the browser's error handler.
    // We log to console where the browser can provide source-mapped traces.

#else
    trace.emplace_back("<stack trace not available on this platform>");
#endif

    return trace;
}

// ── Signal name helpers ─────────────────────────────────────────────────────

static const char* signal_name(int sig) {
    switch (sig) {
        case SIGSEGV: return "SIGSEGV";
        case SIGABRT: return "SIGABRT";
        case SIGFPE:  return "SIGFPE";
#if !defined(NEXUS_PLATFORM_WINDOWS)
        case SIGBUS:  return "SIGBUS";
        case SIGILL:  return "SIGILL";
#endif
        default:      return "UNKNOWN";
    }
}

static const char* signal_description(int sig) {
    switch (sig) {
        case SIGSEGV: return "Segmentation fault (invalid memory access)";
        case SIGABRT: return "Abort signal received";
        case SIGFPE:  return "Floating point exception (div by zero or overflow)";
#if !defined(NEXUS_PLATFORM_WINDOWS)
        case SIGBUS:  return "Bus error (misaligned memory access)";
        case SIGILL:  return "Illegal instruction";
#endif
        default:      return "Unknown signal";
    }
}

// ── Signal handler (POSIX) ──────────────────────────────────────────────────

#if !defined(NEXUS_PLATFORM_WINDOWS)

void CrashHandler::signal_handler(int sig) {
    CrashInfo info;
    info.signal_number = sig;
    info.signal_name = signal_name(sig);
    info.message = signal_description(sig);
    info.stack_trace = capture_stack_trace();

    // Invoke user callback
    if (s_callback) {
        s_callback(info);
    }

    // Write crash dump
    write_crash_dump(info);

#if defined(NEXUS_PLATFORM_ANDROID)
    __android_log_print(ANDROID_LOG_FATAL, "NexusEngine",
                        "CRASH: %s - %s", info.signal_name.c_str(), info.message.c_str());
    for (const auto& frame : info.stack_trace) {
        __android_log_print(ANDROID_LOG_FATAL, "NexusEngine", "  %s", frame.c_str());
    }
#endif

#if defined(NEXUS_PLATFORM_EMSCRIPTEN)
    // Log to browser console
    std::string full_msg = "NexusEngine CRASH: " + info.signal_name + " - " + info.message;
    emscripten_log(EM_LOG_ERROR, "%s", full_msg.c_str());
#endif

    // Re-raise to get default behavior (core dump, etc.)
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

#endif

// ── Windows exception handler ───────────────────────────────────────────────

#if defined(NEXUS_PLATFORM_WINDOWS)

static LONG WINAPI windows_exception_handler(EXCEPTION_POINTERS* exception_info) {
    CrashInfo info;

    DWORD code = exception_info->ExceptionRecord->ExceptionCode;
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:
            info.signal_name = "ACCESS_VIOLATION";
            info.signal_number = SIGSEGV;
            info.message = "Access violation (read/write to invalid address)";
            break;
        case EXCEPTION_STACK_OVERFLOW:
            info.signal_name = "STACK_OVERFLOW";
            info.signal_number = 0;
            info.message = "Stack overflow";
            break;
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            info.signal_name = "DIVIDE_BY_ZERO";
            info.signal_number = SIGFPE;
            info.message = "Division by zero";
            break;
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            info.signal_name = "ILLEGAL_INSTRUCTION";
            info.signal_number = 0;
            info.message = "Illegal instruction";
            break;
        case EXCEPTION_FLT_OVERFLOW:
        case EXCEPTION_FLT_UNDERFLOW:
            info.signal_name = "FLT_EXCEPTION";
            info.signal_number = SIGFPE;
            info.message = "Floating point overflow/underflow";
            break;
        default:
            info.signal_name = "EXCEPTION_" + std::to_string(code);
            info.signal_number = 0;
            info.message = "Windows exception (code: " + std::to_string(code) + ")";
            break;
    }

    info.stack_trace = CrashHandler::capture_stack_trace();

    if (s_callback) {
        s_callback(info);
    }

    CrashHandler::write_crash_dump(info);

    return EXCEPTION_CONTINUE_SEARCH;
}

// Windows also registers signal handlers for SIGABRT etc.
void CrashHandler::signal_handler(int sig) {
    CrashInfo info;
    info.signal_number = sig;
    info.signal_name = signal_name(sig);
    info.message = signal_description(sig);
    info.stack_trace = capture_stack_trace();

    if (s_callback) {
        s_callback(info);
    }

    write_crash_dump(info);

    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

#endif

// ── Install / Uninstall ─────────────────────────────────────────────────────

void CrashHandler::install() {
    if (s_installed) return;

#if defined(NEXUS_PLATFORM_WINDOWS)
    s_prev_exception_filter = SetUnhandledExceptionFilter(windows_exception_handler);
    // Also register POSIX-compatible signal handlers for SIGABRT, SIGFPE
    std::signal(SIGABRT, signal_handler);
    std::signal(SIGFPE, signal_handler);
    std::signal(SIGSEGV, signal_handler);

#elif defined(NEXUS_PLATFORM_EMSCRIPTEN)
    // Emscripten: limited signal support
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = static_cast<int>(SA_RESETHAND);
    sigaction(SIGABRT, &sa, &s_prev_abrt);
    sigaction(SIGFPE,  &sa, &s_prev_fpe);

#else
    // POSIX (Linux, macOS, Android)
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = static_cast<int>(SA_RESETHAND);

    sigaction(SIGSEGV, &sa, &s_prev_segv);
    sigaction(SIGABRT, &sa, &s_prev_abrt);
    sigaction(SIGFPE,  &sa, &s_prev_fpe);
    sigaction(SIGBUS,  &sa, &s_prev_bus);
    sigaction(SIGILL,  &sa, &s_prev_ill);
#endif

    s_installed = true;

#if defined(NEXUS_PLATFORM_WINDOWS)
    constexpr const char* platform_name = "Windows";
#elif defined(NEXUS_PLATFORM_MACOS)
    constexpr const char* platform_name = "macOS";
#elif defined(NEXUS_PLATFORM_ANDROID)
    constexpr const char* platform_name = "Android";
#elif defined(NEXUS_PLATFORM_EMSCRIPTEN)
    constexpr const char* platform_name = "Emscripten";
#elif defined(NEXUS_PLATFORM_LINUX)
    constexpr const char* platform_name = "Linux";
#else
    constexpr const char* platform_name = "Unknown";
#endif
    NX_INFO("CrashHandler installed (platform: {})", platform_name);
}

void CrashHandler::uninstall() {
    if (!s_installed) return;

#if defined(NEXUS_PLATFORM_WINDOWS)
    SetUnhandledExceptionFilter(s_prev_exception_filter);
    std::signal(SIGABRT, SIG_DFL);
    std::signal(SIGFPE, SIG_DFL);
    std::signal(SIGSEGV, SIG_DFL);

#elif defined(NEXUS_PLATFORM_EMSCRIPTEN)
    sigaction(SIGABRT, &s_prev_abrt, nullptr);
    sigaction(SIGFPE,  &s_prev_fpe, nullptr);

#else
    sigaction(SIGSEGV, &s_prev_segv, nullptr);
    sigaction(SIGABRT, &s_prev_abrt, nullptr);
    sigaction(SIGFPE,  &s_prev_fpe, nullptr);
    sigaction(SIGBUS,  &s_prev_bus, nullptr);
    sigaction(SIGILL,  &s_prev_ill, nullptr);
#endif

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

// ── Crash dump writer ───────────────────────────────────────────────────────

void CrashHandler::write_crash_dump(const CrashInfo& info) {
    std::error_code ec;
    std::filesystem::create_directories(s_dump_dir, ec);

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

    // Platform info
    file << "Platform: "
#if defined(NEXUS_PLATFORM_WINDOWS)
         << "Windows"
#elif defined(NEXUS_PLATFORM_MACOS)
         << "macOS"
#elif defined(NEXUS_PLATFORM_ANDROID)
         << "Android"
#elif defined(NEXUS_PLATFORM_EMSCRIPTEN)
         << "Emscripten/WebAssembly"
#elif defined(NEXUS_PLATFORM_LINUX)
         << "Linux"
#else
         << "Unknown"
#endif
         << "\n";

    file << "\n=== Stack Trace ===\n";
    for (size_t i = 0; i < info.stack_trace.size(); ++i) {
        file << "  #" << i << " " << info.stack_trace[i] << "\n";
    }

    file << "\n=== End Report ===\n";
    file.close();
}

// ── Manual crash report ─────────────────────────────────────────────────────

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

// ── Error codes ─────────────────────────────────────────────────────────────

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
