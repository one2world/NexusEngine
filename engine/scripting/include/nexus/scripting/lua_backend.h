#pragma once

#include "nexus/scripting/script_value.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace nexus::scripting {

class ScriptEngine;

// ─────────────────────────────────────────────────────────────────────────────
// LuaBackend — bridges ScriptEngine's registered functions to a Lua-like
// scripting evaluation layer.
//
// When sol2/Lua is available this class will host the actual lua_State.
// For now it provides a lightweight evaluator that parses Lua-style syntax
// and routes all calls through the ScriptEngine function registry.
// ─────────────────────────────────────────────────────────────────────────────

class LuaBackend {
public:
    explicit LuaBackend(ScriptEngine& engine);
    ~LuaBackend();

    /// Initialize the backend and prepare execution state.
    bool initialize();

    /// Shutdown and release resources.
    void shutdown();

    /// Execute a script string. Returns true on success.
    bool execute(const std::string& script);

    /// Execute a script file. Returns true on success.
    bool execute_file(const std::string& filepath);

    /// Call a named function with arguments.
    ScriptValue call(const std::string& func_name,
                     const std::vector<ScriptValue>& args = {});

    /// Check if initialized.
    [[nodiscard]] bool is_initialized() const { return initialized_; }

    /// Get last error message.
    [[nodiscard]] const std::string& last_error() const { return last_error_; }

    /// Register a global variable accessible from scripts.
    void set_global(const std::string& name, const ScriptValue& value);

    /// Get a global variable.
    [[nodiscard]] ScriptValue get_global(const std::string& name) const;

private:
    // Parsed representation of a function call expression.
    struct ParsedCall {
        std::string module;
        std::string function;
        std::vector<ScriptValue> args;
    };

    /// Execute a single line of script, handling assignment, control flow, etc.
    bool execute_line(const std::string& line, u32 line_number,
                      const std::string& source_name);

    /// Evaluate an expression and return its value.
    ScriptValue evaluate_expression(const std::string& expr);

    /// Parse a function call expression such as "Module.func(a, b)".
    ParsedCall parse_function_call(const std::string& expr);

    /// Parse a single literal value token (number, string, bool, nil, variable).
    ScriptValue parse_value(const std::string& token);

    /// Split a comma-separated argument list, respecting parentheses and strings.
    std::vector<std::string> tokenize_args(const std::string& args_str);

    /// Strip leading and trailing whitespace.
    static std::string trim(const std::string& s);

    /// Check if a string looks like a function call (contains balanced parens).
    static bool is_function_call(const std::string& expr);

    /// Handle string concatenation with the ".." operator.
    ScriptValue evaluate_concatenation(const std::string& expr);

    /// Parse a table constructor: {key=val, ...} or {val1, val2, ...}.
    ScriptValue parse_table_constructor(const std::string& expr);

    /// Collect lines for a multi-line block (while/for/function ... end).
    std::vector<std::string> collect_block(std::istringstream& stream,
                                            u32& line_number);

    /// Execute a while loop body.
    bool execute_while(const std::string& condition,
                       const std::vector<std::string>& body,
                       u32 line_number, const std::string& source);

    /// Execute a numeric for loop body.
    bool execute_for(const std::string& var, i32 start, i32 stop, i32 step,
                     const std::vector<std::string>& body,
                     u32 line_number, const std::string& source);

    /// Report an error with context.
    void set_error(const std::string& message, const std::string& source,
                   u32 line);

    ScriptEngine& engine_;
    bool initialized_{false};
    std::string last_error_;

    // Local/global variables for script scope.
    std::unordered_map<std::string, ScriptValue> globals_;
};

} // namespace nexus::scripting
