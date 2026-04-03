#include "nexus/scripting/script_engine.h"
#include "nexus/scripting/lua_backend.h"
#include "nexus/core/log.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace nexus::scripting {

// ── Coroutine ───────────────────────────────────────────────────────────────

u32 Coroutine::next_id() {
    static u32 counter = 0;
    return ++counter;
}

bool Coroutine::resume() {
    if (status_ == Status::Dead) return false;

    if (waiting_) return true; // still waiting, don't resume yet

    status_ = Status::Running;
    yielded_ = false;

    if (!started_) {
        started_ = true;
        body_(*this);
        if (!yielded_) {
            status_ = Status::Dead;
        }
    }

    return status_ != Status::Dead;
}

void Coroutine::yield(ScriptValue value) {
    last_yield_ = std::move(value);
    yielded_ = true;
    status_ = Status::Suspended;
}

void Coroutine::wait(float seconds) {
    wait_time_ = seconds;
    waiting_ = true;
    yield();
}

bool Coroutine::tick_wait(float dt) {
    if (!waiting_) return true;
    wait_time_ -= dt;
    if (wait_time_ <= 0.0f) {
        wait_time_ = 0.0f;
        waiting_ = false;
        return true;
    }
    return false;
}

// ── ScriptEngine ────────────────────────────────────────────────────────────

ScriptEngine::ScriptEngine() = default;
ScriptEngine::~ScriptEngine() = default;

void ScriptEngine::register_function(const std::string& module,
                                       const std::string& name,
                                       ScriptValue::FunctionType func,
                                       u32 min_args, u32 max_args,
                                       const std::string& description) {
    NativeFunction nf;
    nf.name = name;
    nf.module = module;
    nf.func = std::move(func);
    nf.min_args = min_args;
    nf.max_args = max_args;
    nf.description = description;
    functions_.push_back(std::move(nf));

    // Build lookup key
    std::string key = module.empty() ? name : (module + "." + name);
    function_lookup_[key] = &functions_.back();

    // Also register in globals as a ScriptValue function
    globals_.set(key, ScriptValue(functions_.back().func));
}

void ScriptEngine::set_global(const std::string& name, ScriptValue value) {
    globals_.set(name, std::move(value));
}

ScriptValue ScriptEngine::get_global(const std::string& name) const {
    return globals_.get(name);
}

ScriptValue ScriptEngine::call_function(const std::string& qualified_name,
                                          const std::vector<ScriptValue>& args) {
    auto it = function_lookup_.find(qualified_name);
    if (it == function_lookup_.end()) {
        report_error("Function not found: " + qualified_name);
        return ScriptValue::nil();
    }

    auto* nf = it->second;
    u32 arg_count = static_cast<u32>(args.size());
    if (arg_count < nf->min_args) {
        report_error(qualified_name + ": expected at least " +
                      std::to_string(nf->min_args) + " args, got " +
                      std::to_string(arg_count));
        return ScriptValue::nil();
    }
    if (arg_count > nf->max_args) {
        report_error(qualified_name + ": expected at most " +
                      std::to_string(nf->max_args) + " args, got " +
                      std::to_string(arg_count));
        return ScriptValue::nil();
    }

    try {
        return nf->func(args);
    } catch (const std::exception& e) {
        report_error(qualified_name + ": " + e.what());
        return ScriptValue::nil();
    }
}

const NativeFunction* ScriptEngine::find_function(const std::string& module,
                                                     const std::string& name) const {
    std::string key = module.empty() ? name : (module + "." + name);
    auto it = function_lookup_.find(key);
    return it != function_lookup_.end() ? it->second : nullptr;
}

// ── Coroutines ──────────────────────────────────────────────────────────────

u32 ScriptEngine::start_coroutine(Coroutine::Body body) {
    auto co = std::make_unique<Coroutine>(std::move(body));
    u32 id = co->id();
    coroutines_.push_back(std::move(co));
    return id;
}

void ScriptEngine::update_coroutines(float dt) {
    for (auto& co : coroutines_) {
        if (co->status() == Coroutine::Status::Dead) continue;

        if (co->is_waiting()) {
            if (!co->tick_wait(dt)) continue;
        }

        co->resume();
    }

    // Remove dead coroutines
    coroutines_.erase(
        std::remove_if(coroutines_.begin(), coroutines_.end(),
            [](const auto& co) { return co->status() == Coroutine::Status::Dead; }),
        coroutines_.end());
}

void ScriptEngine::stop_coroutine(u32 id) {
    for (auto& co : coroutines_) {
        if (co->id() == id) {
            // Mark dead so it gets cleaned up
            co->yield();
            coroutines_.erase(
                std::remove_if(coroutines_.begin(), coroutines_.end(),
                    [id](const auto& c) { return c->id() == id; }),
                coroutines_.end());
            return;
        }
    }
}

u32 ScriptEngine::active_coroutine_count() const {
    u32 count = 0;
    for (auto& co : coroutines_) {
        if (co->status() != Coroutine::Status::Dead) ++count;
    }
    return count;
}

// ── Hot Reload ──────────────────────────────────────────────────────────────

void ScriptEngine::register_script(const std::string& name,
                                     const std::string& source,
                                     const std::string& file_path) {
    ScriptSource ss;
    ss.name = name;
    ss.source = source;
    ss.file_path = file_path;

    if (!file_path.empty()) {
        std::error_code ec;
        auto ftime = std::filesystem::last_write_time(file_path, ec);
        if (!ec) {
            ss.last_modified = static_cast<u64>(
                ftime.time_since_epoch().count());
        }
    }

    scripts_[name] = std::move(ss);
}

bool ScriptEngine::check_hot_reload() {
    bool any_reloaded = false;

    for (auto& [name, ss] : scripts_) {
        if (ss.file_path.empty()) continue;

        std::error_code ec;
        auto ftime = std::filesystem::last_write_time(ss.file_path, ec);
        if (ec) continue;

        u64 current_time = static_cast<u64>(ftime.time_since_epoch().count());
        if (current_time != ss.last_modified) {
            if (reload_script(name)) {
                any_reloaded = true;
            }
        }
    }

    return any_reloaded;
}

bool ScriptEngine::reload_script(const std::string& name) {
    auto it = scripts_.find(name);
    if (it == scripts_.end()) return false;

    auto& ss = it->second;
    if (ss.file_path.empty()) return false;

    std::ifstream file(ss.file_path);
    if (!file.is_open()) {
        report_error("Failed to reload script: " + ss.file_path, ss.file_path);
        return false;
    }

    std::stringstream buf;
    buf << file.rdbuf();
    ss.source = buf.str();

    std::error_code ec;
    auto ftime = std::filesystem::last_write_time(ss.file_path, ec);
    if (!ec) {
        ss.last_modified = static_cast<u64>(ftime.time_since_epoch().count());
    }

    NX_INFO("Script reloaded: {}", name);
    return true;
}

std::string ScriptEngine::get_script_source(const std::string& name) const {
    auto it = scripts_.find(name);
    return it != scripts_.end() ? it->second.source : "";
}

u32 ScriptEngine::script_count() const {
    return static_cast<u32>(scripts_.size());
}

// ── Lua Backend ─────────────────────────────────────────────────────────────

LuaBackend& ScriptEngine::lua_backend() {
    if (!lua_backend_) {
        lua_backend_ = std::make_unique<LuaBackend>(*this);
        lua_backend_->initialize();
    }
    return *lua_backend_;
}

// ── Errors ──────────────────────────────────────────────────────────────────

void ScriptEngine::report_error(const std::string& message,
                                  const std::string& source,
                                  u32 line) {
    ScriptError err;
    err.message = message;
    err.source_file = source;
    err.line = line;
    errors_.push_back(err);

    NX_ERROR("Script error: {} ({}:{})", message, source, line);

    if (error_handler_) {
        error_handler_(err);
    }
}

} // namespace nexus::scripting
