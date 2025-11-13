#pragma once

#include "pch.h"

struct ObjString;
struct ObjArray;
struct ObjObject;

struct ObjClass;
struct ObjInstance;
struct ObjClosure;
struct ObjFunctionProto;
struct ObjModule;
struct ObjUpvalue;
struct ObjBoundMethod;

class MeowEngine;
class Value;

using Arguments = const std::vector<Value>&;

using Int8 = int8_t;
using Int16 = int16_t;
using Int32 = int32_t;
using Int64 = int64_t;

using Uint8 = uint8_t;
using Uint16 = uint16_t;
using Uint32 = uint32_t;
using Uint64 = uint64_t;

using Float32 = float;
using Float64 = double;
using Float128 = long double;

using Null = std::monostate;
using Int = Int64;
using Real = Float64;
using Bool = bool;
using Str = std::string;
using Array = ObjArray*;

using Object = ObjObject*;

using Instance = ObjInstance*;
using Class = ObjClass*;
using Upvalue = ObjUpvalue*;
using Function = ObjClosure*;
using Module = ObjModule*;
using BoundMethod = ObjBoundMethod*;
using Proto = ObjFunctionProto*;

using NativeFnSimple = std::function<Value(Arguments)>;
using NativeFnAdvanced = std::function<Value(MeowEngine*, Arguments)>;
using NativeFn = std::variant<NativeFnSimple, NativeFnAdvanced>;

using BaseValue = std::variant<
    Null,
    Int,
    Real,
    Bool,
    Str,
    Array,
    Object,
    Instance,
    Class,
    Upvalue,
    Function,
    Module,
    BoundMethod,
    Proto,
    NativeFn
>;

class Value : public BaseValue {
public:
    Value() : BaseValue(Null{}) {}
    template<typename T>
    Value(T&& t) : BaseValue(std::forward<T>(t)) {}

    template<typename T>
    const T& get() const { return std::get<T>(*this); }

    template<typename T>
    T& get() { return std::get<T>(*this); }

    template<typename T>
    const T* get_if() const { return std::get_if<T>(this); }

    template<typename T>
    T* get_if() { return std::get_if<T>(this); }
    
    template<typename T>
    bool is() const { return std::holds_alternative<T>(*this); }
};