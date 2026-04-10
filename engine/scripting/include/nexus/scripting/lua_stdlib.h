#pragma once

namespace nexus::scripting {
class ScriptEngine;

/// Register Lua standard library modules (math, string, table, os).
void register_lua_stdlib(ScriptEngine& engine);

/// Register math library (math.abs, math.floor, math.ceil, math.sqrt, etc.)
void register_math_library(ScriptEngine& engine);

/// Register string library (string.len, string.sub, string.upper, string.lower, etc.)
void register_string_library(ScriptEngine& engine);

/// Register table library (table.insert, table.remove, table.concat, table.sort)
void register_table_library(ScriptEngine& engine);

/// Register os library (os.clock, os.time)
void register_os_library(ScriptEngine& engine);

} // namespace nexus::scripting
