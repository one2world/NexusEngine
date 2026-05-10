#pragma once

namespace nexus::scripting {
class ScriptEngine;

/// Register the full Lua standard library: basic globals + math + string +
/// table + os.  Single entry point so editor / sandbox / runtime hosts all
/// see the same name set — there is no "minimal mode" to drift from.
void register_lua_stdlib(ScriptEngine& engine);

/// Register the Lua basic library — globals every Lua program assumes:
/// `print`, `tostring`, `tonumber`, `type`, `assert`, `error`, `select`,
/// `ipairs`, `pairs`, `rawequal`, `rawget`, `rawset`, `unpack`.
/// `print` honours ScriptEngine::print_sink() when one is bound, falling
/// back to engine logging.
void register_basic_library(ScriptEngine& engine);

/// Register math library (math.abs, math.floor, math.ceil, math.sqrt, etc.)
void register_math_library(ScriptEngine& engine);

/// Register string library (string.len, string.sub, string.upper, string.lower, etc.)
void register_string_library(ScriptEngine& engine);

/// Register table library (table.insert, table.remove, table.concat, table.sort)
void register_table_library(ScriptEngine& engine);

/// Register os library (os.clock, os.time)
void register_os_library(ScriptEngine& engine);

} // namespace nexus::scripting
