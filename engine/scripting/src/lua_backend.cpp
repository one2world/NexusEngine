// LuaBackend — embeds PUC-Rio Lua 5.4 inside ScriptEngine.
//
// Architecture:
//   ScriptEngine owns a deque<NativeFunction>; each entry is a C++ lambda
//   that the host registered via register_function(module, name, ...).
//   LuaBackend::initialize() opens a fresh lua_State, calls luaL_openlibs
//   so the standard libraries (basic / math / string / table / os / io /
//   coroutine / utf8 / package) are immediately available, overrides the
//   built-in `print` to route through ScriptEngine::print_sink, then walks
//   the function registry and installs each entry as a Lua C closure
//   reachable from the canonical "<module>.<name>" path.
//
// Marshalling rules (see push_script_value / to_script_value):
//   nil      <-> ScriptValue::nil()
//   boolean  <-> ScriptValue(bool)
//   integer  <-> ScriptValue(i32)        (Lua 5.4 has a true integer subtype)
//   number   <-> ScriptValue(float)
//   string   <-> ScriptValue(std::string)
//   table {x,y}        <-> Vec2
//   table {x,y,z}      <-> Vec3
//   table {x,y,z,w}    <-> Vec4
//   table {__nx_entity=true, id=N} <-> Entity(N)
//   plain table        <-> ScriptValue::table()  (deep copy on the way back)
//
// Errors raised from Lua during pcall are captured into last_error_ and
// reported via ScriptEngine::report_error so they show up in the editor's
// Console panel alongside other engine logs.

#include "nexus/scripting/lua_backend.h"
#include "nexus/scripting/script_engine.h"
#include "nexus/core/log.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <cstring>

namespace nexus::scripting {

// ── Forward declarations ────────────────────────────────────────────────────

static void        push_script_value(lua_State* L, const ScriptValue& v);
static ScriptValue to_script_value  (lua_State* L, int index);
static int         native_function_dispatch(lua_State* L);
static void        register_in_lua  (lua_State* L, const NativeFunction& nf);

// Lua's `luaL_openlibs` already populates these top-level tables with the
// canonical reference implementations (math.abs, string.find, table.insert,
// …).  Our C++ wrappers under the same module names exist purely so
// headless tests (which never boot a lua_State) can still exercise the
// engine via direct ScriptEngine::call_function() — when a real lua_State
// is up, the Lua natives must win.  Skip installing C++ wrappers for
// these modules so they don't shadow the canonical implementations.
static bool module_is_lua_stdlib(const std::string& module) {
    static const std::string kLuaOwned[] = {
        "math", "string", "table", "os", "io",
        "coroutine", "package", "utf8", "debug",
    };
    for (const auto& m : kLuaOwned) {
        if (module == m) return true;
    }
    return false;
}

// Public install path used by ScriptEngine after the backend is up.
void LuaBackend::install_native(const NativeFunction& nf) {
    if (L_ == nullptr) return;
    if (module_is_lua_stdlib(nf.module)) return;
    register_in_lua(L_, nf);
}

// ── Construction / destruction ──────────────────────────────────────────────

LuaBackend::LuaBackend(ScriptEngine& engine) : engine_(engine) {}

LuaBackend::~LuaBackend() { shutdown(); }

// ── Lifecycle ───────────────────────────────────────────────────────────────

bool LuaBackend::initialize() {
    if (L_ != nullptr) return true;  // idempotent

    L_ = luaL_newstate();
    if (L_ == nullptr) {
        last_error_ = "luaL_newstate: out of memory";
        NX_ERROR("[Lua] {}", last_error_);
        return false;
    }
    luaL_openlibs(L_);

    // Stash a back-pointer to this LuaBackend on the registry so the
    // native-function trampoline can recover ScriptEngine without a
    // global / TLS variable.
    lua_pushlightuserdata(L_, this);
    lua_setfield(L_, LUA_REGISTRYINDEX, "nx_lua_backend");

    // Override the built-in `print` so it honours ScriptEngine::print_sink.
    // The default implementation writes to stdout, which the editor never
    // reads — this keeps the canonical Lua name working while letting the
    // editor's Console panel display every print() call.
    lua_pushcfunction(L_, [](lua_State* L) -> int {
        // Recover the LuaBackend (and from it, the ScriptEngine).
        lua_getfield(L, LUA_REGISTRYINDEX, "nx_lua_backend");
        auto* backend = static_cast<LuaBackend*>(lua_touserdata(L, -1));
        lua_pop(L, 1);
        if (backend == nullptr) return 0;
        const int n = lua_gettop(L);
        std::string msg;
        for (int i = 1; i <= n; ++i) {
            if (i > 1) msg += "\t";  // Lua's print uses tabs by spec
            // Use Lua's own tostring so __tostring metamethods fire.
            lua_getglobal(L, "tostring");
            lua_pushvalue(L, i);
            if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
                msg += lua_tostring(L, -1);
                lua_pop(L, 1);
                continue;
            }
            size_t len = 0;
            const char* s = lua_tolstring(L, -1, &len);
            if (s) msg.append(s, len);
            lua_pop(L, 1);
        }
        if (backend->engine().has_print_sink()) {
            backend->engine().print_sink()(msg);
        } else {
            NX_INFO("[Script] {}", msg);
        }
        return 0;
    });
    lua_setglobal(L_, "print");

    // Replay any functions that were registered before initialize() so
    // editor / sandbox can register early without ordering pain.  Goes
    // through install_native so the same shadow-protection rules apply
    // (C++ math.abs etc. don't overwrite Lua's natives).
    for (const auto& nf : engine_.registered_functions()) {
        install_native(nf);
    }
    return true;
}

void LuaBackend::shutdown() {
    if (L_ != nullptr) {
        lua_close(L_);
        L_ = nullptr;
    }
}

// ── Execute / evaluate ──────────────────────────────────────────────────────

bool LuaBackend::execute(const std::string& script) {
    if (L_ == nullptr) { last_error_ = "Lua not initialised"; return false; }
    if (luaL_loadstring(L_, script.c_str()) != LUA_OK) {
        capture_error("load");
        return false;
    }
    if (lua_pcall(L_, /*nargs*/0, /*nresults*/0, /*errfunc*/0) != LUA_OK) {
        capture_error("execute");
        return false;
    }
    last_error_.clear();
    return true;
}

bool LuaBackend::execute_file(const std::string& filepath) {
    if (L_ == nullptr) { last_error_ = "Lua not initialised"; return false; }
    if (luaL_loadfile(L_, filepath.c_str()) != LUA_OK) {
        capture_error("load_file");
        return false;
    }
    if (lua_pcall(L_, 0, 0, 0) != LUA_OK) {
        capture_error("execute_file");
        return false;
    }
    last_error_.clear();
    return true;
}

ScriptValue LuaBackend::evaluate(const std::string& expr) {
    if (L_ == nullptr) { last_error_ = "Lua not initialised"; return ScriptValue::nil(); }
    // Wrap the expression in `return (...)` so the parser sees a complete
    // chunk that yields exactly one value on the stack.  The parens
    // protect against multi-return (`return f(),g()` would yield two).
    const std::string wrapped = "return (" + expr + ")";
    if (luaL_loadstring(L_, wrapped.c_str()) != LUA_OK) {
        capture_error("evaluate.load");
        return ScriptValue::nil();
    }
    if (lua_pcall(L_, 0, 1, 0) != LUA_OK) {
        capture_error("evaluate");
        return ScriptValue::nil();
    }
    last_error_.clear();
    return pop_value();
}

ScriptValue LuaBackend::call(const std::string& func_name,
                              const std::vector<ScriptValue>& args) {
    if (L_ == nullptr) { last_error_ = "Lua not initialised"; return ScriptValue::nil(); }
    // Resolve `Module.func` paths by walking dotted segments.  We push
    // _G, then each segment in turn; a missing intermediate node raises
    // an error that's reported just like a runtime failure.
    lua_pushglobaltable(L_);
    size_t start = 0;
    while (start <= func_name.size()) {
        size_t dot = func_name.find('.', start);
        std::string seg = (dot == std::string::npos)
            ? func_name.substr(start)
            : func_name.substr(start, dot - start);
        lua_getfield(L_, -1, seg.c_str());
        lua_remove(L_, -2);  // pop the parent table
        if (dot == std::string::npos) break;
        start = dot + 1;
    }
    if (!lua_isfunction(L_, -1)) {
        lua_pop(L_, 1);
        last_error_ = "call: '" + func_name + "' is not a function";
        return ScriptValue::nil();
    }
    for (const auto& a : args) push_script_value(L_, a);
    if (lua_pcall(L_, static_cast<int>(args.size()), 1, 0) != LUA_OK) {
        capture_error("call");
        return ScriptValue::nil();
    }
    last_error_.clear();
    return pop_value();
}

// ── Globals ────────────────────────────────────────────────────────────────

void LuaBackend::set_global(const std::string& name, const ScriptValue& value) {
    if (L_ == nullptr) return;
    push_script_value(L_, value);
    lua_setglobal(L_, name.c_str());
}

ScriptValue LuaBackend::get_global(const std::string& name) const {
    if (L_ == nullptr) return ScriptValue::nil();
    lua_getglobal(L_, name.c_str());
    auto* mut = const_cast<LuaBackend*>(this);
    return mut->pop_value();
}

// ── Helpers ────────────────────────────────────────────────────────────────

ScriptValue LuaBackend::pop_value() {
    if (L_ == nullptr || lua_gettop(L_) == 0) return ScriptValue::nil();
    ScriptValue v = to_script_value(L_, -1);
    lua_pop(L_, 1);
    return v;
}

void LuaBackend::capture_error(const char* context) {
    const char* msg = lua_tostring(L_, -1);
    last_error_ = std::string(context) + ": " + (msg ? msg : "unknown");
    lua_pop(L_, 1);
    NX_ERROR("[Lua] {}", last_error_);
    // Forward to ScriptEngine so the editor's error sink (M31) sees it.
    ScriptError err;
    err.message = last_error_;
    if (auto& h = engine_.error_handler_for_backend()) {
        h(err);
    }
    auto& errors = engine_.mutable_errors();
    errors.push_back(std::move(err));
}

// ─────────────────────────────────────────────────────────────────────────────
// Marshalling: ScriptValue ↔ Lua stack
// ─────────────────────────────────────────────────────────────────────────────

namespace {

constexpr const char* kEntityMarker = "__nx_entity";

// Decide if a Lua table at `index` represents one of our engine vec types.
// Returns the dimension (2/3/4) when {x,y[,z[,w]]} fields exist and no
// other named keys, otherwise 0.
int detect_vec_arity(lua_State* L, int index) {
    int hits   = 0;
    int total  = 0;
    bool has_x = false, has_y = false, has_z = false, has_w = false;
    lua_pushnil(L);
    while (lua_next(L, index) != 0) {
        ++total;
        if (lua_type(L, -2) == LUA_TSTRING) {
            const char* k = lua_tostring(L, -2);
            if      (std::strcmp(k, "x") == 0) { has_x = true; ++hits; }
            else if (std::strcmp(k, "y") == 0) { has_y = true; ++hits; }
            else if (std::strcmp(k, "z") == 0) { has_z = true; ++hits; }
            else if (std::strcmp(k, "w") == 0) { has_w = true; ++hits; }
        }
        lua_pop(L, 1);
    }
    if (hits != total) return 0;
    if (has_x && has_y && has_z && has_w) return 4;
    if (has_x && has_y && has_z)          return 3;
    if (has_x && has_y)                   return 2;
    return 0;
}

bool is_entity_table(lua_State* L, int index) {
    lua_getfield(L, index, kEntityMarker);
    bool yes = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    return yes;
}

float field_number(lua_State* L, int index, const char* key) {
    lua_getfield(L, index, key);
    float v = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);
    return v;
}

} // namespace

static void push_script_value(lua_State* L, const ScriptValue& v) {
    using T = ScriptValue::Type;
    switch (v.type()) {
        case T::Nil:    lua_pushnil(L);                         return;
        case T::Bool:   lua_pushboolean(L, v.as_bool());        return;
        case T::Int:    lua_pushinteger(L, v.as_int());         return;
        case T::Float:  lua_pushnumber(L, v.as_float());        return;
        case T::String: {
            const std::string& s = v.as_string();
            lua_pushlstring(L, s.data(), s.size());
            return;
        }
        case T::Vec2: {
            auto p = v.as_vec2();
            lua_createtable(L, 0, 2);
            lua_pushnumber(L, p.x); lua_setfield(L, -2, "x");
            lua_pushnumber(L, p.y); lua_setfield(L, -2, "y");
            return;
        }
        case T::Vec3: {
            auto p = v.as_vec3();
            lua_createtable(L, 0, 3);
            lua_pushnumber(L, p.x); lua_setfield(L, -2, "x");
            lua_pushnumber(L, p.y); lua_setfield(L, -2, "y");
            lua_pushnumber(L, p.z); lua_setfield(L, -2, "z");
            return;
        }
        case T::Vec4: {
            auto p = v.as_vec4();
            lua_createtable(L, 0, 4);
            lua_pushnumber(L, p.x); lua_setfield(L, -2, "x");
            lua_pushnumber(L, p.y); lua_setfield(L, -2, "y");
            lua_pushnumber(L, p.z); lua_setfield(L, -2, "z");
            lua_pushnumber(L, p.w); lua_setfield(L, -2, "w");
            return;
        }
        case T::Entity: {
            lua_createtable(L, 0, 2);
            lua_pushboolean(L, 1);
            lua_setfield(L, -2, kEntityMarker);
            lua_pushinteger(L, static_cast<lua_Integer>(v.as_entity()));
            lua_setfield(L, -2, "id");
            return;
        }
        case T::Table: {
            auto tbl = v.as_table();
            lua_createtable(L, 0, static_cast<int>(tbl ? tbl->size() : 0));
            if (tbl) {
                for (const auto& [k, val] : *tbl) {
                    push_script_value(L, val);
                    lua_setfield(L, -2, k.c_str());
                }
            }
            return;
        }
        case T::Function:
            // Lua-side native functions are registered via register_in_lua;
            // pushing an arbitrary FunctionType inline isn't a path the
            // engine uses today.  Push nil so callers see a clean failure.
            lua_pushnil(L);
            return;
    }
    lua_pushnil(L);
}

static ScriptValue to_script_value(lua_State* L, int index) {
    const int t = lua_type(L, index);
    switch (t) {
        case LUA_TNIL:
        case LUA_TNONE:
            return ScriptValue::nil();
        case LUA_TBOOLEAN:
            return ScriptValue(static_cast<bool>(lua_toboolean(L, index)));
        case LUA_TNUMBER:
            if (lua_isinteger(L, index)) {
                return ScriptValue(static_cast<i32>(lua_tointeger(L, index)));
            }
            return ScriptValue(static_cast<float>(lua_tonumber(L, index)));
        case LUA_TSTRING: {
            size_t len = 0;
            const char* s = lua_tolstring(L, index, &len);
            return ScriptValue(std::string(s, len));
        }
        case LUA_TTABLE: {
            // Normalise: convert relative indices to absolute so lua_next
            // doesn't drift if the caller pushes during iteration.
            int abs = lua_absindex(L, index);
            if (is_entity_table(L, abs)) {
                lua_getfield(L, abs, "id");
                u32 id = static_cast<u32>(lua_tointeger(L, -1));
                lua_pop(L, 1);
                return ScriptValue::entity(id);
            }
            int arity = detect_vec_arity(L, abs);
            switch (arity) {
                case 2: return ScriptValue(Vec2(field_number(L, abs, "x"),
                                                field_number(L, abs, "y")));
                case 3: return ScriptValue(Vec3(field_number(L, abs, "x"),
                                                field_number(L, abs, "y"),
                                                field_number(L, abs, "z")));
                case 4: return ScriptValue(Vec4(field_number(L, abs, "x"),
                                                field_number(L, abs, "y"),
                                                field_number(L, abs, "z"),
                                                field_number(L, abs, "w")));
                default: break;
            }
            // Generic table: copy keys.  We use string keys only — Lua
            // arrays end up with numeric keys which we stringify.
            ScriptValue out = ScriptValue::table();
            auto tbl = out.as_table();
            lua_pushnil(L);
            while (lua_next(L, abs) != 0) {
                std::string key;
                if (lua_type(L, -2) == LUA_TSTRING) {
                    key = lua_tostring(L, -2);
                } else if (lua_type(L, -2) == LUA_TNUMBER) {
                    key = std::to_string(
                        static_cast<long long>(lua_tointeger(L, -2)));
                } else {
                    lua_pop(L, 1);
                    continue;
                }
                (*tbl)[key] = to_script_value(L, -1);
                lua_pop(L, 1);
            }
            return out;
        }
        default:
            return ScriptValue::nil();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Native function bridge
// ─────────────────────────────────────────────────────────────────────────────

// Stack: args 1..N, upvalue(1) = NativeFunction*
static int native_function_dispatch(lua_State* L) {
    auto* nf = static_cast<const NativeFunction*>(
        lua_touserdata(L, lua_upvalueindex(1)));
    if (nf == nullptr) {
        return luaL_error(L, "native function trampoline: missing upvalue");
    }
    const int top = lua_gettop(L);
    const auto n  = static_cast<u32>(top);
    if (n < nf->min_args) {
        return luaL_error(L, "%s: expected at least %d args, got %d",
                          nf->name.c_str(), nf->min_args, top);
    }
    if (n > nf->max_args) {
        return luaL_error(L, "%s: expected at most %d args, got %d",
                          nf->name.c_str(), nf->max_args, top);
    }
    std::vector<ScriptValue> args;
    args.reserve(n);
    for (int i = 1; i <= top; ++i) {
        args.push_back(to_script_value(L, i));
    }
    try {
        ScriptValue result = nf->func(args);
        push_script_value(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "%s: %s", nf->name.c_str(), e.what());
    } catch (...) {
        return luaL_error(L, "%s: unknown C++ exception", nf->name.c_str());
    }
}

// Install a NativeFunction at "<module>.<name>" (or just "<name>" when
// module is empty).  Creates the module table on first use.
static void register_in_lua(lua_State* L, const NativeFunction& nf) {
    // Closure: push light userdata = &nf, then create a C closure that
    // captures it as upvalue(1).  ScriptEngine guarantees NativeFunction
    // entries live for the lifetime of the engine (deque, no reallocation).
    lua_pushlightuserdata(L, const_cast<NativeFunction*>(&nf));
    lua_pushcclosure(L, native_function_dispatch, 1);

    if (nf.module.empty()) {
        lua_setglobal(L, nf.name.c_str());
        return;
    }
    // Get-or-create the module table at the global scope.
    lua_getglobal(L, nf.module.c_str());
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, nf.module.c_str());
    }
    // Stack: [closure, module_table].  Set module[name] = closure.
    lua_pushvalue(L, -2);                // duplicate closure on top
    lua_setfield(L, -2, nf.name.c_str()); // module[name] = closure
    lua_pop(L, 2);                        // pop module + original closure
}

} // namespace nexus::scripting
