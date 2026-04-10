#include "nexus/scripting/lua_stdlib.h"
#include "nexus/scripting/script_engine.h"
#include "nexus/core/log.h"

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <chrono>
#include <numeric>
#include <sstream>

namespace nexus::scripting {

// ── Helpers ─────────────────────────────────────────────────────────────────

static float to_float(const ScriptValue& v) {
    if (v.is_int())   return static_cast<float>(v.as_int());
    if (v.is_float())  return v.as_float();
    return 0.0f;
}

// ── Math Library ────────────────────────────────────────────────────────────

void register_math_library(ScriptEngine& engine) {
    // Constants
    engine.set_global("math.pi",   ScriptValue(3.14159265358979323846f));
    engine.set_global("math.huge", ScriptValue(std::numeric_limits<float>::infinity()));

    // math.abs(x)
    engine.register_function("math", "abs",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (args[0].is_int()) return ScriptValue(std::abs(args[0].as_int()));
            return ScriptValue(std::abs(to_float(args[0])));
        }, 1, 1, "Absolute value");

    // math.floor(x)
    engine.register_function("math", "floor",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            return ScriptValue(std::floor(to_float(args[0])));
        }, 1, 1, "Floor");

    // math.ceil(x)
    engine.register_function("math", "ceil",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            return ScriptValue(std::ceil(to_float(args[0])));
        }, 1, 1, "Ceiling");

    // math.sqrt(x)
    engine.register_function("math", "sqrt",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            return ScriptValue(std::sqrt(to_float(args[0])));
        }, 1, 1, "Square root");

    // math.sin(x)
    engine.register_function("math", "sin",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            return ScriptValue(std::sin(to_float(args[0])));
        }, 1, 1, "Sine");

    // math.cos(x)
    engine.register_function("math", "cos",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            return ScriptValue(std::cos(to_float(args[0])));
        }, 1, 1, "Cosine");

    // math.tan(x)
    engine.register_function("math", "tan",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            return ScriptValue(std::tan(to_float(args[0])));
        }, 1, 1, "Tangent");

    // math.asin(x)
    engine.register_function("math", "asin",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            return ScriptValue(std::asin(to_float(args[0])));
        }, 1, 1, "Arcsine");

    // math.acos(x)
    engine.register_function("math", "acos",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            return ScriptValue(std::acos(to_float(args[0])));
        }, 1, 1, "Arccosine");

    // math.atan(y [, x]) — atan2 when two args
    engine.register_function("math", "atan",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (args.size() >= 2) {
                return ScriptValue(std::atan2(to_float(args[0]), to_float(args[1])));
            }
            return ScriptValue(std::atan(to_float(args[0])));
        }, 1, 2, "Arctangent (atan2 with 2 args)");

    // math.exp(x)
    engine.register_function("math", "exp",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            return ScriptValue(std::exp(to_float(args[0])));
        }, 1, 1, "Exponential (e^x)");

    // math.log(x)
    engine.register_function("math", "log",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            return ScriptValue(std::log(to_float(args[0])));
        }, 1, 1, "Natural logarithm");

    // math.pow(x, y)
    engine.register_function("math", "pow",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            return ScriptValue(std::pow(to_float(args[0]), to_float(args[1])));
        }, 2, 2, "Power");

    // math.fmod(x, y)
    engine.register_function("math", "fmod",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            return ScriptValue(std::fmod(to_float(args[0]), to_float(args[1])));
        }, 2, 2, "Floating-point modulo");

    // math.max(a, b, ...)
    engine.register_function("math", "max",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            float result = to_float(args[0]);
            for (size_t i = 1; i < args.size(); ++i) {
                float v = to_float(args[i]);
                if (v > result) result = v;
            }
            return ScriptValue(result);
        }, 1, 255, "Maximum of arguments");

    // math.min(a, b, ...)
    engine.register_function("math", "min",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            float result = to_float(args[0]);
            for (size_t i = 1; i < args.size(); ++i) {
                float v = to_float(args[i]);
                if (v < result) result = v;
            }
            return ScriptValue(result);
        }, 1, 255, "Minimum of arguments");

    // math.random([m [, n]])
    engine.register_function("math", "random",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (args.empty()) {
                // [0, 1) float
                return ScriptValue(static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX));
            }
            if (args.size() == 1) {
                // [1, m] integer
                i32 m = args[0].is_int() ? args[0].as_int() : static_cast<i32>(to_float(args[0]));
                if (m < 1) return ScriptValue(i32(0));
                return ScriptValue(static_cast<i32>(std::rand() % m) + 1);
            }
            // [m, n] integer
            i32 m = args[0].is_int() ? args[0].as_int() : static_cast<i32>(to_float(args[0]));
            i32 n = args[1].is_int() ? args[1].as_int() : static_cast<i32>(to_float(args[1]));
            if (n < m) return ScriptValue(m);
            return ScriptValue(static_cast<i32>(std::rand() % (n - m + 1)) + m);
        }, 0, 2, "Random number");

    // math.randomseed(x)
    engine.register_function("math", "randomseed",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            i32 seed = args[0].is_int() ? args[0].as_int() : static_cast<i32>(to_float(args[0]));
            std::srand(static_cast<unsigned>(seed));
            return ScriptValue::nil();
        }, 1, 1, "Seed random number generator");
}

// ── String Library ──────────────────────────────────────────────────────────

void register_string_library(ScriptEngine& engine) {
    // string.len(s)
    engine.register_function("string", "len",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            return ScriptValue(static_cast<i32>(args[0].as_string().size()));
        }, 1, 1, "String length");

    // string.sub(s, i [, j]) — 1-based, Lua-style
    engine.register_function("string", "sub",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            const std::string& s = args[0].as_string();
            i32 len = static_cast<i32>(s.size());
            i32 i = args[1].is_int() ? args[1].as_int() : static_cast<i32>(to_float(args[1]));
            i32 j = (args.size() >= 3)
                ? (args[2].is_int() ? args[2].as_int() : static_cast<i32>(to_float(args[2])))
                : -1;

            // Lua negative indexing: -1 = last char
            if (i < 0) i = len + i + 1;
            if (j < 0) j = len + j + 1;

            // Clamp to valid range
            if (i < 1) i = 1;
            if (j > len) j = len;
            if (i > j) return ScriptValue(std::string(""));

            // Convert to 0-based
            return ScriptValue(s.substr(static_cast<size_t>(i - 1),
                                        static_cast<size_t>(j - i + 1)));
        }, 2, 3, "Substring (1-based, Lua-style)");

    // string.upper(s)
    engine.register_function("string", "upper",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            std::string s = args[0].as_string();
            std::transform(s.begin(), s.end(), s.begin(), ::toupper);
            return ScriptValue(s);
        }, 1, 1, "Convert to uppercase");

    // string.lower(s)
    engine.register_function("string", "lower",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            std::string s = args[0].as_string();
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            return ScriptValue(s);
        }, 1, 1, "Convert to lowercase");

    // string.rep(s, n)
    engine.register_function("string", "rep",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            const std::string& s = args[0].as_string();
            i32 n = args[1].is_int() ? args[1].as_int() : static_cast<i32>(to_float(args[1]));
            if (n <= 0) return ScriptValue(std::string(""));
            std::string result;
            result.reserve(s.size() * static_cast<size_t>(n));
            for (i32 i = 0; i < n; ++i) result += s;
            return ScriptValue(result);
        }, 2, 2, "Repeat string n times");

    // string.reverse(s)
    engine.register_function("string", "reverse",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            std::string s = args[0].as_string();
            std::reverse(s.begin(), s.end());
            return ScriptValue(s);
        }, 1, 1, "Reverse string");

    // string.byte(s [, i]) — 1-based
    engine.register_function("string", "byte",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            const std::string& s = args[0].as_string();
            i32 i = (args.size() >= 2)
                ? (args[1].is_int() ? args[1].as_int() : static_cast<i32>(to_float(args[1])))
                : 1;
            if (i < 1 || i > static_cast<i32>(s.size())) return ScriptValue::nil();
            return ScriptValue(static_cast<i32>(static_cast<u8>(s[static_cast<size_t>(i - 1)])));
        }, 1, 2, "Character code at position (1-based)");

    // string.char(n)
    engine.register_function("string", "char",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            i32 n = args[0].is_int() ? args[0].as_int() : static_cast<i32>(to_float(args[0]));
            return ScriptValue(std::string(1, static_cast<char>(n)));
        }, 1, 1, "Character from code");

    // string.find(s, pattern [, init]) — plain text search, not regex
    engine.register_function("string", "find",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            const std::string& s = args[0].as_string();
            const std::string& pattern = args[1].as_string();
            i32 init = (args.size() >= 3)
                ? (args[2].is_int() ? args[2].as_int() : static_cast<i32>(to_float(args[2])))
                : 1;

            // Lua negative indexing
            if (init < 0) init = static_cast<i32>(s.size()) + init + 1;
            if (init < 1) init = 1;

            size_t start = static_cast<size_t>(init - 1);
            if (start >= s.size()) return ScriptValue::nil();

            size_t pos = s.find(pattern, start);
            if (pos == std::string::npos) return ScriptValue::nil();

            // Return 1-based start position
            return ScriptValue(static_cast<i32>(pos + 1));
        }, 2, 3, "Find substring (plain text, returns 1-based position)");

    // string.format(fmt, ...) — supports %s, %d, %f
    engine.register_function("string", "format",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            const std::string& fmt = args[0].as_string();
            std::string result;
            size_t arg_idx = 1;

            for (size_t i = 0; i < fmt.size(); ++i) {
                if (fmt[i] == '%' && i + 1 < fmt.size()) {
                    char spec = fmt[i + 1];
                    if (spec == '%') {
                        result += '%';
                        ++i;
                    } else if (spec == 's' && arg_idx < args.size()) {
                        result += args[arg_idx].to_string();
                        ++arg_idx;
                        ++i;
                    } else if (spec == 'd' && arg_idx < args.size()) {
                        if (args[arg_idx].is_int()) {
                            result += std::to_string(args[arg_idx].as_int());
                        } else {
                            result += std::to_string(static_cast<i32>(to_float(args[arg_idx])));
                        }
                        ++arg_idx;
                        ++i;
                    } else if (spec == 'f' && arg_idx < args.size()) {
                        // Default %f precision (6 decimal places)
                        char buf[64];
                        std::snprintf(buf, sizeof(buf), "%f", static_cast<double>(to_float(args[arg_idx])));
                        result += buf;
                        ++arg_idx;
                        ++i;
                    } else {
                        result += fmt[i];
                    }
                } else {
                    result += fmt[i];
                }
            }
            return ScriptValue(result);
        }, 1, 255, "Simple string format (%s, %d, %f)");
}

// ── Table Library ───────────────────────────────────────────────────────────

void register_table_library(ScriptEngine& engine) {
    // table.insert(t, [pos,] value)
    engine.register_function("table", "insert",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (!args[0].is_table()) return ScriptValue::nil();
            auto tbl = args[0].as_table();

            // Find current array length (highest consecutive integer key starting at "1")
            i32 len = 0;
            while (tbl->count(std::to_string(len + 1))) ++len;

            if (args.size() == 2) {
                // table.insert(t, value) — append
                tbl->insert_or_assign(std::to_string(len + 1), args[1]);
            } else if (args.size() >= 3) {
                // table.insert(t, pos, value) — insert at pos, shift others
                i32 pos = args[1].is_int() ? args[1].as_int() : static_cast<i32>(to_float(args[1]));
                if (pos < 1) pos = 1;
                if (pos > len + 1) pos = len + 1;

                // Shift elements up
                for (i32 i = len; i >= pos; --i) {
                    std::string from = std::to_string(i);
                    std::string to = std::to_string(i + 1);
                    auto it = tbl->find(from);
                    if (it != tbl->end()) {
                        tbl->insert_or_assign(to, it->second);
                    }
                }
                tbl->insert_or_assign(std::to_string(pos), args[2]);
            }
            return ScriptValue::nil();
        }, 2, 3, "Insert value into array part of table");

    // table.remove(t [, pos])
    engine.register_function("table", "remove",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (!args[0].is_table()) return ScriptValue::nil();
            auto tbl = args[0].as_table();

            i32 len = 0;
            while (tbl->count(std::to_string(len + 1))) ++len;

            i32 pos = (args.size() >= 2)
                ? (args[1].is_int() ? args[1].as_int() : static_cast<i32>(to_float(args[1])))
                : len;

            if (pos < 1 || pos > len) return ScriptValue::nil();

            // Save removed value
            ScriptValue removed = (*tbl)[std::to_string(pos)];

            // Shift elements down
            for (i32 i = pos; i < len; ++i) {
                std::string from = std::to_string(i + 1);
                std::string to = std::to_string(i);
                auto it = tbl->find(from);
                if (it != tbl->end()) {
                    tbl->insert_or_assign(to, it->second);
                }
            }
            tbl->erase(std::to_string(len));

            return removed;
        }, 1, 2, "Remove element from array part of table");

    // table.concat(t [, sep])
    engine.register_function("table", "concat",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (!args[0].is_table()) return ScriptValue(std::string(""));
            auto tbl = args[0].as_table();
            std::string sep = (args.size() >= 2 && args[1].is_string())
                ? args[1].as_string() : "";

            std::string result;
            i32 i = 1;
            while (true) {
                auto it = tbl->find(std::to_string(i));
                if (it == tbl->end()) break;
                if (i > 1) result += sep;
                result += it->second.to_string();
                ++i;
            }
            return ScriptValue(result);
        }, 1, 2, "Concatenate array elements with separator");

    // table.sort(t) — sorts array part
    engine.register_function("table", "sort",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (!args[0].is_table()) return ScriptValue::nil();
            auto tbl = args[0].as_table();

            // Collect array part
            std::vector<ScriptValue> arr;
            i32 i = 1;
            while (true) {
                auto it = tbl->find(std::to_string(i));
                if (it == tbl->end()) break;
                arr.push_back(it->second);
                ++i;
            }

            // Sort: numbers by value, strings lexicographically
            std::sort(arr.begin(), arr.end(),
                [](const ScriptValue& a, const ScriptValue& b) {
                    if (a.is_number() && b.is_number()) {
                        return a.as_float() < b.as_float();
                    }
                    if (a.is_string() && b.is_string()) {
                        return a.as_string() < b.as_string();
                    }
                    return a.to_string() < b.to_string();
                });

            // Write back
            for (size_t idx = 0; idx < arr.size(); ++idx) {
                tbl->insert_or_assign(std::to_string(idx + 1), arr[idx]);
            }
            return ScriptValue::nil();
        }, 1, 1, "Sort array part of table");

    // table.getn(t) — get array length
    engine.register_function("table", "getn",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (!args[0].is_table()) return ScriptValue(i32(0));
            auto tbl = args[0].as_table();

            i32 len = 0;
            while (tbl->count(std::to_string(len + 1))) ++len;
            return ScriptValue(len);
        }, 1, 1, "Get array length (highest consecutive integer key)");

    // table.keys(t) — get all keys as a table (extension)
    engine.register_function("table", "keys",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (!args[0].is_table()) return ScriptValue::table();
            auto tbl = args[0].as_table();

            auto result = ScriptValue::table();
            auto result_tbl = result.as_table();
            i32 idx = 1;
            for (const auto& [key, _] : *tbl) {
                result_tbl->insert_or_assign(std::to_string(idx), ScriptValue(key));
                ++idx;
            }
            return result;
        }, 1, 1, "Get all keys as a table (extension)");
}

// ── OS Library ──────────────────────────────────────────────────────────────

void register_os_library(ScriptEngine& engine) {
    // os.clock() — CPU time in seconds
    engine.register_function("os", "clock",
        [](const std::vector<ScriptValue>&) -> ScriptValue {
            return ScriptValue(static_cast<float>(
                static_cast<double>(std::clock()) / CLOCKS_PER_SEC));
        }, 0, 0, "CPU time in seconds");

    // os.time() — epoch time in seconds
    engine.register_function("os", "time",
        [](const std::vector<ScriptValue>&) -> ScriptValue {
            return ScriptValue(static_cast<i32>(std::time(nullptr)));
        }, 0, 0, "Epoch time in seconds");
}

// ── Register All ────────────────────────────────────────────────────────────

void register_lua_stdlib(ScriptEngine& engine) {
    register_math_library(engine);
    register_string_library(engine);
    register_table_library(engine);
    register_os_library(engine);
}

} // namespace nexus::scripting
