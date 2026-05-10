#pragma once

#include "nexus/scripting/script_value.h"
#include <string>
#include <vector>

// Forward-declare the Lua state opaquely so this header doesn't pull lua.h
// into every consumer.  Lua's lua_State is a forward-declarable struct.
struct lua_State;

namespace nexus::scripting {

class ScriptEngine;

// ─────────────────────────────────────────────────────────────────────────────
// LuaBackend — wraps a real PUC-Rio Lua 5.4 lua_State.
//
// The previous implementation hand-rolled a partial Lua-like interpreter
// and broke at the first non-trivial expression (`i + j`, conditionals,
// closures, …).  This version embeds the canonical Lua 5.4 reference
// implementation: every Lua expression / statement / control-flow
// construct / coroutine / metatable / closure is supported because the
// VM does the work.  The native-function bridge lets ScriptEngine's
// register_function() wire C++ lambdas into Lua via the standard C API
// (lua_pushcclosure + light-userdata trampoline).
//
// Public API kept stable so editor / sandbox / tests don't change.
// ─────────────────────────────────────────────────────────────────────────────

class LuaBackend {
public:
    explicit LuaBackend(ScriptEngine& engine);
    ~LuaBackend();

    LuaBackend(const LuaBackend&)            = delete;
    LuaBackend& operator=(const LuaBackend&) = delete;

    /// Open a fresh lua_State, install the standard libraries
    /// (basic + math + string + table + os + io + coroutine + utf8 + package),
    /// and route Lua's `print` through ScriptEngine::print_sink when bound.
    /// Returns false if Lua state allocation fails (out-of-memory).
    bool initialize();

    /// Close the lua_State and release all script-side memory.
    void shutdown();

    /// Execute a Lua chunk.  Errors are captured into last_error() and
    /// also forwarded through ScriptEngine::report_error so ScriptEngine
    /// users see them in errors().
    bool execute(const std::string& script);

    /// Execute a script file.  Wraps luaL_dofile so file:line error
    /// reporting points at the actual on-disk source.
    bool execute_file(const std::string& filepath);

    /// Look up a global by name and call it with the supplied arguments.
    /// Returns ScriptValue::nil() when the global isn't a callable or
    /// when the call raises an error (last_error() is set in that case).
    ScriptValue call(const std::string& func_name,
                     const std::vector<ScriptValue>& args = {});

    [[nodiscard]] bool is_initialized() const { return L_ != nullptr; }
    [[nodiscard]] const std::string& last_error() const { return last_error_; }

    /// Set / get a Lua global.  Vec / Entity values are marshalled into
    /// canonical Lua tables (see script_value_lua.cpp).
    void set_global(const std::string& name, const ScriptValue& value);
    [[nodiscard]] ScriptValue get_global(const std::string& name) const;

    /// Evaluate a single Lua expression and return its value.  Used by
    /// the editor's Watch panel and by inline `:exec` flows that take an
    /// expression instead of a chunk.  Wraps the input in `return (...)`
    /// so the parser sees a complete chunk, then loads + pcalls it.
    /// Returns ScriptValue::nil() on parse / runtime error (with
    /// last_error_ populated).
    ScriptValue evaluate(const std::string& expr);

    /// Install a NativeFunction registered through ScriptEngine into
    /// the running lua_State.  Called by ScriptEngine::register_function
    /// after pre-init setup (initialize() replays every existing entry,
    /// post-init register_function() calls invoke this directly).
    void install_native(const NativeFunction& nf);

    /// Direct access to the underlying lua_State for advanced callers
    /// (engine_bindings registers C functions, ScriptEngine pushes
    /// native lambdas into the global table).  Returns nullptr when
    /// not initialised.
    [[nodiscard]] lua_State* state() const { return L_; }
    [[nodiscard]] ScriptEngine& engine() const { return engine_; }

private:
    /// Pop the topmost value off the Lua stack and return it as a
    /// ScriptValue.  No-op (returns nil) when the stack is empty.
    ScriptValue pop_value();

    /// Capture the topmost Lua value as the active error message.
    /// `context` is prepended to the message ("execute"/"call"/...).
    void capture_error(const char* context);

    ScriptEngine& engine_;
    lua_State*    L_{nullptr};
    std::string   last_error_;
};

} // namespace nexus::scripting
