#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <variant>
#include <any>
#include <memory>
#include <optional>

namespace nexus::scripting {

// ─────────────────────────────────────────────────────────────────────────────
// ScriptValue — dynamically typed value for script variables
// ─────────────────────────────────────────────────────────────────────────────

class ScriptValue {
public:
    enum class Type : u8 {
        Nil,
        Bool,
        Int,
        Float,
        String,
        Vec2,
        Vec3,
        Vec4,
        Entity,
        Function,
        Table,
    };

    using FunctionType = std::function<ScriptValue(const std::vector<ScriptValue>&)>;
    using TableType = std::shared_ptr<std::unordered_map<std::string, ScriptValue>>;

    ScriptValue() : type_(Type::Nil) {}
    explicit ScriptValue(bool v)                : type_(Type::Bool),     data_(v) {}
    explicit ScriptValue(i32 v)                 : type_(Type::Int),      data_(v) {}
    explicit ScriptValue(i64 v)                 : type_(Type::Int),      data_(static_cast<i32>(v)) {}
    explicit ScriptValue(float v)               : type_(Type::Float),    data_(v) {}
    explicit ScriptValue(double v)              : type_(Type::Float),    data_(static_cast<float>(v)) {}
    explicit ScriptValue(const std::string& v)  : type_(Type::String),   data_(v) {}
    explicit ScriptValue(const char* v)         : type_(Type::String),   data_(std::string(v)) {}
    explicit ScriptValue(nexus::Vec2 v)         : type_(Type::Vec2),     data_(v) {}
    explicit ScriptValue(nexus::Vec3 v)         : type_(Type::Vec3),     data_(v) {}
    explicit ScriptValue(nexus::Vec4 v)         : type_(Type::Vec4),     data_(v) {}
    explicit ScriptValue(FunctionType fn)       : type_(Type::Function), data_(std::move(fn)) {}

    static ScriptValue nil()                 { return {}; }
    static ScriptValue entity(u32 id)        { ScriptValue v; v.type_ = Type::Entity; v.data_ = static_cast<i32>(id); return v; }
    static ScriptValue table()               { ScriptValue v; v.type_ = Type::Table; v.data_ = std::make_shared<std::unordered_map<std::string, ScriptValue>>(); return v; }

    Type type() const { return type_; }
    bool is_nil() const     { return type_ == Type::Nil; }
    bool is_bool() const    { return type_ == Type::Bool; }
    bool is_int() const     { return type_ == Type::Int; }
    bool is_float() const   { return type_ == Type::Float; }
    bool is_number() const  { return type_ == Type::Int || type_ == Type::Float; }
    bool is_string() const  { return type_ == Type::String; }
    bool is_vec2() const    { return type_ == Type::Vec2; }
    bool is_vec3() const    { return type_ == Type::Vec3; }
    bool is_vec4() const    { return type_ == Type::Vec4; }
    bool is_entity() const  { return type_ == Type::Entity; }
    bool is_function() const { return type_ == Type::Function; }
    bool is_table() const   { return type_ == Type::Table; }

    bool          as_bool() const   { return std::get<bool>(data_); }
    i32           as_int() const    { return std::get<i32>(data_); }
    float         as_float() const;
    std::string   as_string() const { return std::get<std::string>(data_); }
    nexus::Vec2   as_vec2() const   { return std::get<nexus::Vec2>(data_); }
    nexus::Vec3   as_vec3() const   { return std::get<nexus::Vec3>(data_); }
    nexus::Vec4   as_vec4() const   { return std::get<nexus::Vec4>(data_); }
    u32           as_entity() const { return static_cast<u32>(std::get<i32>(data_)); }
    const FunctionType& as_function() const { return std::get<FunctionType>(data_); }
    TableType     as_table() const  { return std::get<TableType>(data_); }

    /// Truthiness: nil and false are falsy, everything else truthy.
    bool truthy() const;

    /// Invoke if function, otherwise return nil.
    ScriptValue call(const std::vector<ScriptValue>& args = {}) const;

    /// Table field access.
    ScriptValue get_field(const std::string& key) const;
    void set_field(const std::string& key, ScriptValue val);

    /// Equality.
    bool operator==(const ScriptValue& other) const;
    bool operator!=(const ScriptValue& other) const { return !(*this == other); }

    /// Convert to display string.
    std::string to_string() const;

private:
    Type type_;
    std::variant<
        std::monostate,
        bool,
        i32,
        float,
        std::string,
        nexus::Vec2,
        nexus::Vec3,
        nexus::Vec4,
        FunctionType,
        TableType
    > data_;
};

// ─────────────────────────────────────────────────────────────────────────────
// ScriptContext — variable scope with parent chain
// ─────────────────────────────────────────────────────────────────────────────

class ScriptContext {
public:
    explicit ScriptContext(ScriptContext* parent = nullptr) : parent_(parent) {}

    void set(const std::string& name, ScriptValue value);
    ScriptValue get(const std::string& name) const;
    bool has(const std::string& name) const;

    /// Set in nearest scope that owns the variable, or local if new.
    void set_upvalue(const std::string& name, ScriptValue value);

    ScriptContext* parent() const { return parent_; }
    const std::unordered_map<std::string, ScriptValue>& locals() const { return vars_; }

private:
    ScriptContext* parent_{nullptr};
    std::unordered_map<std::string, ScriptValue> vars_;
};

// ─────────────────────────────────────────────────────────────────────────────
// NativeFunction — registered C++ function callable from scripts
// ─────────────────────────────────────────────────────────────────────────────

struct NativeFunction {
    std::string name;
    std::string module;    // e.g., "Entity", "Input", "Audio"
    ScriptValue::FunctionType func;
    u32 min_args{0};
    u32 max_args{255};
    std::string description;
};

// ─────────────────────────────────────────────────────────────────────────────
// ScriptError — error info from script execution
// ─────────────────────────────────────────────────────────────────────────────

struct ScriptError {
    std::string message;
    std::string source_file;
    u32 line{0};
    std::string stack_trace;
};

} // namespace nexus::scripting
