#include "nexus/scripting/script_value.h"
#include <sstream>

namespace nexus::scripting {

// ── ScriptValue ─────────────────────────────────────────────────────────────

float ScriptValue::as_float() const {
    if (type_ == Type::Float) return std::get<float>(data_);
    if (type_ == Type::Int)   return static_cast<float>(std::get<i32>(data_));
    return 0.0f;
}

bool ScriptValue::truthy() const {
    if (type_ == Type::Nil) return false;
    if (type_ == Type::Bool) return std::get<bool>(data_);
    return true;
}

ScriptValue ScriptValue::call(const std::vector<ScriptValue>& args) const {
    if (type_ != Type::Function) return nil();
    return std::get<FunctionType>(data_)(args);
}

ScriptValue ScriptValue::get_field(const std::string& key) const {
    if (type_ != Type::Table) return nil();
    auto& tbl = *std::get<TableType>(data_);
    auto it = tbl.find(key);
    return it != tbl.end() ? it->second : nil();
}

void ScriptValue::set_field(const std::string& key, ScriptValue val) {
    if (type_ != Type::Table) return;
    (*std::get<TableType>(data_))[key] = std::move(val);
}

bool ScriptValue::operator==(const ScriptValue& other) const {
    if (type_ != other.type_) return false;
    switch (type_) {
        case Type::Nil:      return true;
        case Type::Bool:     return as_bool() == other.as_bool();
        case Type::Int:      return as_int() == other.as_int();
        case Type::Float:    return as_float() == other.as_float();
        case Type::String:   return as_string() == other.as_string();
        case Type::Entity:   return as_entity() == other.as_entity();
        default:             return false; // functions/tables compare by identity
    }
}

std::string ScriptValue::to_string() const {
    switch (type_) {
        case Type::Nil:      return "nil";
        case Type::Bool:     return as_bool() ? "true" : "false";
        case Type::Int:      return std::to_string(as_int());
        case Type::Float: {
            std::ostringstream oss;
            oss << as_float();
            return oss.str();
        }
        case Type::String:   return as_string();
        case Type::Vec2: {
            auto v = as_vec2();
            std::ostringstream oss;
            oss << "vec2(" << v.x << ", " << v.y << ")";
            return oss.str();
        }
        case Type::Vec3: {
            auto v = as_vec3();
            std::ostringstream oss;
            oss << "vec3(" << v.x << ", " << v.y << ", " << v.z << ")";
            return oss.str();
        }
        case Type::Vec4: {
            auto v = as_vec4();
            std::ostringstream oss;
            oss << "vec4(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")";
            return oss.str();
        }
        case Type::Entity:   return "entity(" + std::to_string(as_entity()) + ")";
        case Type::Function: return "<function>";
        case Type::Table:    return "<table>";
    }
    return "?";
}

// ── ScriptContext ───────────────────────────────────────────────────────────

void ScriptContext::set(const std::string& name, ScriptValue value) {
    vars_[name] = std::move(value);
}

ScriptValue ScriptContext::get(const std::string& name) const {
    auto it = vars_.find(name);
    if (it != vars_.end()) return it->second;
    if (parent_) return parent_->get(name);
    return ScriptValue::nil();
}

bool ScriptContext::has(const std::string& name) const {
    if (vars_.count(name)) return true;
    if (parent_) return parent_->has(name);
    return false;
}

void ScriptContext::set_upvalue(const std::string& name, ScriptValue value) {
    // Walk up the scope chain to find where the var lives
    ScriptContext* ctx = this;
    while (ctx) {
        if (ctx->vars_.count(name)) {
            ctx->vars_[name] = std::move(value);
            return;
        }
        ctx = ctx->parent_;
    }
    // Not found anywhere; set in local scope
    vars_[name] = std::move(value);
}

} // namespace nexus::scripting
