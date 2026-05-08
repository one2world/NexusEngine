#pragma once

#include "nexus/scripting/script_value.h"
#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include <functional>
#include <memory>

namespace nexus::scripting { class LuaBackend; }

namespace nexus::scripting {

// ─────────────────────────────────────────────────────────────────────────────
// Coroutine — cooperative multitasking for scripts
// ─────────────────────────────────────────────────────────────────────────────

class Coroutine {
public:
    enum class Status : u8 { Running, Suspended, Dead };

    using Body = std::function<void(Coroutine&)>;

    explicit Coroutine(Body body)
        : body_(std::move(body)), status_(Status::Suspended) {}

    Status status() const { return status_; }

    /// Resume the coroutine. Returns true if still alive.
    bool resume();

    /// Yield with optional value. Called from within body.
    void yield(ScriptValue value = ScriptValue::nil());

    /// Wait for duration (seconds). Caller checks time.
    void wait(float seconds);

    /// Get the last yielded value.
    ScriptValue last_yield() const { return last_yield_; }

    /// Remaining wait time.
    float wait_time() const { return wait_time_; }

    /// Tick wait timer. Returns true if done waiting.
    bool tick_wait(float dt);

    /// Is this coroutine waiting?
    bool is_waiting() const { return waiting_; }

    u32 id() const { return id_; }

private:
    static u32 next_id();

    u32 id_{next_id()};
    Body body_;
    Status status_{Status::Suspended};
    ScriptValue last_yield_;
    float wait_time_{0.0f};
    bool waiting_{false};
    bool yielded_{false};
    bool started_{false};
};

// ─────────────────────────────────────────────────────────────────────────────
// ScriptEngine — manages scripts, bindings, coroutines, and hot-reload
// ─────────────────────────────────────────────────────────────────────────────

class ScriptEngine {
public:
    ScriptEngine();
    ~ScriptEngine();

    /// Register a native C++ function accessible from scripts.
    void register_function(const std::string& module,
                            const std::string& name,
                            ScriptValue::FunctionType func,
                            u32 min_args = 0, u32 max_args = 255,
                            const std::string& description = "");

    /// Register a global variable.
    void set_global(const std::string& name, ScriptValue value);
    ScriptValue get_global(const std::string& name) const;

    /// Call a registered function by "module.name" or just "name".
    ScriptValue call_function(const std::string& qualified_name,
                               const std::vector<ScriptValue>& args = {});

    /// Get all registered functions.
    const std::deque<NativeFunction>& registered_functions() const { return functions_; }

    /// Find a function by module.name.
    const NativeFunction* find_function(const std::string& module,
                                         const std::string& name) const;

    // ── Coroutines ──────────────────────────────────────────────────────

    /// Start a new coroutine. Returns its ID.
    u32 start_coroutine(Coroutine::Body body);

    /// Tick all coroutines (advance waits, resume ready ones).
    void update_coroutines(float dt);

    /// Kill a coroutine.
    void stop_coroutine(u32 id);

    /// Number of active coroutines.
    u32 active_coroutine_count() const;

    // ── Hot Reload ──────────────────────────────────────────────────────

    /// Register a script source with a file path for hot-reload.
    void register_script(const std::string& name, const std::string& source,
                          const std::string& file_path = "");

    /// Check if any registered scripts have changed on disk.
    bool check_hot_reload();

    /// Force reload a script by name.
    bool reload_script(const std::string& name);

    /// Get script source by name.
    std::string get_script_source(const std::string& name) const;

    /// Registered script count.
    u32 script_count() const;

    // ── Error handling ──────────────────────────────────────────────────

    using ErrorHandler = std::function<void(const ScriptError&)>;

    void set_error_handler(ErrorHandler handler) { error_handler_ = std::move(handler); }
    const std::vector<ScriptError>& errors() const { return errors_; }
    void clear_errors() { errors_.clear(); }

    // ── print() routing ────────────────────────────────────────────────
    //
    // The shared `print` binding (see scripting/engine_bindings.cpp) calls
    // this sink with the joined message before the engine logger sees it.
    // The editor wires this to ConsolePanel::add_message so Lua-side
    // `print("hello")` flows directly into the Console panel; when no
    // sink is set the binding falls back to NX_INFO so headless / sandbox
    // builds continue to log normally.
    using PrintSink = std::function<void(const std::string&)>;
    void set_print_sink(PrintSink sink) { print_sink_ = std::move(sink); }
    const PrintSink& print_sink() const { return print_sink_; }
    bool has_print_sink() const { return static_cast<bool>(print_sink_); }

    /// The global context.
    ScriptContext& globals() { return globals_; }
    const ScriptContext& globals() const { return globals_; }

    // ── Lua Backend ────────────────────────────────────────────────────

    /// Get (or lazily create) the Lua scripting backend.
    LuaBackend& lua_backend();

private:
    void report_error(const std::string& message,
                       const std::string& source = "",
                       u32 line = 0);

    ScriptContext globals_;
    std::deque<NativeFunction> functions_;
    std::unordered_map<std::string, NativeFunction*> function_lookup_;
    std::vector<std::unique_ptr<Coroutine>> coroutines_;

    struct ScriptSource {
        std::string name;
        std::string source;
        std::string file_path;
        u64 last_modified{0};
    };
    std::unordered_map<std::string, ScriptSource> scripts_;

    ErrorHandler error_handler_;
    std::vector<ScriptError> errors_;
    PrintSink print_sink_;

    std::unique_ptr<LuaBackend> lua_backend_;
};

} // namespace nexus::scripting
