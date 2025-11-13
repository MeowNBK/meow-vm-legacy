#include "operator_dispatcher.h"
#include "meow_vm.h"
#include "value.h"
#include "pch.h"

ValueType getValueType(const Value& value) {
    if (value.is<Null>()) return ValueType::Null;
    if (value.is<Int>()) return ValueType::Int;
    if (value.is<Real>()) return ValueType::Real;
    if (value.is<Bool>()) return ValueType::Bool;
    if (value.is<Str>()) return ValueType::Str;
    if (value.is<Array>()) return ValueType::Array;
    if (value.is<Object>()) return ValueType::Object;
    if (value.is<Upvalue>()) return ValueType::Upvalue;
    if (value.is<Function>()) return ValueType::Function;
    if (value.is<Class>()) return ValueType::Class;
    if (value.is<Instance>()) return ValueType::Instance;
    if (value.is<BoundMethod>()) return ValueType::BoundMethod;
    if (value.is<Proto>()) return ValueType::Proto;
    if (value.is<NativeFn>()) return ValueType::NativeFn;

    throw std::runtime_error("Kiểu dữ liệu không xác định: " + std::to_string(value.index()));
}

Str valueTypeName(ValueType t) {
    switch (t) {
        case ValueType::Null: return "Null";
        case ValueType::Int: return "Int";
        case ValueType::Real: return "Real";
        case ValueType::Bool: return "Bool";
        case ValueType::Str: return "String";
        case ValueType::Array: return "Array";
        case ValueType::Object: return "Object";
        case ValueType::Upvalue: return "Upvalue";
        case ValueType::Function: return "Function";
        case ValueType::Class: return "Class";
        case ValueType::Instance: return "Instance";
        case ValueType::BoundMethod: return "BoundMethod";
        case ValueType::Proto: return "Proto";
        case ValueType::NativeFn: return "NativeFn";
        default: return "Unknown";
    }
}

OperatorDispatcher::OperatorDispatcher() {
    using enum OpCode;
    using VT = ValueType;


    auto boolToInt  = [](const Bool& b) { return static_cast<Int>(b);  };
    auto boolToReal = [](const Bool& b) { return static_cast<Real>(b); };


    const Real EPS = static_cast<Real>(1e-12);
    auto realEq = [&](Real a, Real b) {
        Real diff = std::fabs(a - b);
        if (diff <= EPS) return true;
        Real maxab = std::max(std::fabs(a), std::fabs(b));
        return diff <= EPS * std::max(static_cast<Real>(1), maxab);
    };


    const Real INF = std::numeric_limits<Real>::infinity();
    const Real NANV = std::numeric_limits<Real>::quiet_NaN();


    auto isFalsy = [&](const Value& v)->Bool {
        switch (getValueType(v)) {
            case VT::Null:   return true;
            case VT::Bool:   return !v.get<Bool>();
            case VT::Int:    return v.get<Int>() == 0;
            case VT::Real:   return v.get<Real>() == static_cast<Real>(0.0);
            case VT::Str:    return v.get<Str>().empty();
            case VT::Array:  {
                auto arr = v.get<Array>();
                return !(arr && !arr->elements.empty());
            }
            case VT::Object: {
                auto obj = v.get<Object>();
                return !(obj && !obj->fields.empty());
            }
            case VT::Upvalue:
            case VT::Function:
            case VT::Class:
            case VT::Instance:
            case VT::BoundMethod:
                return false;
            default:
                return false;
        }
    };


    auto toBool = [&](const Value& v)->Bool {
        return !isFalsy(v);
    };



    binaryOps[{ADD, VT::Int,  VT::Int }] = [](const Value& l, const Value& r){ return Value(l.get<Int>()  + r.get<Int>()); };
    binaryOps[{ADD, VT::Real, VT::Real}] = [](const Value& l, const Value& r){ return Value(l.get<Real>() + r.get<Real>()); };
    binaryOps[{ADD, VT::Int,  VT::Real}] = [](const Value& l, const Value& r){ return Value(static_cast<Real>(l.get<Int>()) + r.get<Real>()); };
    binaryOps[{ADD, VT::Real, VT::Int }] = [](const Value& l, const Value& r){ return Value(l.get<Real>() + static_cast<Real>(r.get<Int>())); };

    binaryOps[{ADD, VT::Int,  VT::Bool}] = [=](const Value& l, const Value& r){ return Value(l.get<Int>()  + boolToInt(r.get<Bool>())); };
    binaryOps[{ADD, VT::Bool, VT::Int }] = [=](const Value& l, const Value& r){ return Value(boolToInt(l.get<Bool>()) + r.get<Int>()); };
    binaryOps[{ADD, VT::Real, VT::Bool}] = [=](const Value& l, const Value& r){ return Value(l.get<Real>() + boolToReal(r.get<Bool>())); };
    binaryOps[{ADD, VT::Bool, VT::Real}] = [=](const Value& l, const Value& r){ return Value(boolToReal(l.get<Bool>()) + r.get<Real>()); };

    binaryOps[{ADD, VT::Str,  VT::Str }] = [](const Value& l, const Value& r){ return Value(l.get<Str>() + r.get<Str>()); };
    binaryOps[{ADD, VT::Str,  VT::Int }] = [](const Value& l, const Value& r){ return Value(l.get<Str>() + std::to_string(r.get<Int>())); };
    binaryOps[{ADD, VT::Int,  VT::Str }] = [](const Value& l, const Value& r){ return Value(std::to_string(l.get<Int>()) + r.get<Str>()); };
    binaryOps[{ADD, VT::Str,  VT::Real}] = [](const Value& l, const Value& r){ return Value(l.get<Str>() + std::to_string(r.get<Real>())); };
    binaryOps[{ADD, VT::Real, VT::Str }] = [](const Value& l, const Value& r){ return Value(std::to_string(l.get<Real>()) + r.get<Str>()); };
    binaryOps[{ADD, VT::Str,  VT::Bool}] = [](const Value& l, const Value& r){ return Value(l.get<Str>() + (r.get<Bool>() ? "true" : "false")); };
    binaryOps[{ADD, VT::Bool, VT::Str }] = [](const Value& l, const Value& r){ return Value((l.get<Bool>() ? "true" : "false") + r.get<Str>()); };


    binaryOps[{SUB, VT::Int,  VT::Int }] = [](const Value& l, const Value& r){ return Value(l.get<Int>()  - r.get<Int>()); };
    binaryOps[{SUB, VT::Real, VT::Real}] = [](const Value& l, const Value& r){ return Value(l.get<Real>() - r.get<Real>()); };
    binaryOps[{SUB, VT::Int,  VT::Real}] = [](const Value& l, const Value& r){ return Value(static_cast<Real>(l.get<Int>()) - r.get<Real>()); };
    binaryOps[{SUB, VT::Real, VT::Int }] = [](const Value& l, const Value& r){ return Value(l.get<Real>() - static_cast<Real>(r.get<Int>())); };
    binaryOps[{SUB, VT::Int,  VT::Bool}] = [=](const Value& l, const Value& r){ return Value(l.get<Int>()  - boolToInt(r.get<Bool>())); };
    binaryOps[{SUB, VT::Bool, VT::Int }] = [=](const Value& l, const Value& r){ return Value(boolToInt(l.get<Bool>()) - r.get<Int>()); };
    binaryOps[{SUB, VT::Real, VT::Bool}] = [=](const Value& l, const Value& r){ return Value(l.get<Real>() - boolToReal(r.get<Bool>())); };
    binaryOps[{SUB, VT::Bool, VT::Real}] = [=](const Value& l, const Value& r){ return Value(boolToReal(l.get<Bool>()) - r.get<Real>()); };


    binaryOps[{MUL, VT::Int,  VT::Int }] = [](const Value& l, const Value& r){ return Value(l.get<Int>()  * r.get<Int>()); };
    binaryOps[{MUL, VT::Real, VT::Real}] = [](const Value& l, const Value& r){ return Value(l.get<Real>() * r.get<Real>()); };
    binaryOps[{MUL, VT::Int,  VT::Real}] = [](const Value& l, const Value& r){ return Value(static_cast<Real>(l.get<Int>()) * r.get<Real>()); };
    binaryOps[{MUL, VT::Real, VT::Int }] = [](const Value& l, const Value& r){ return Value(l.get<Real>() * static_cast<Real>(r.get<Int>())); };
    binaryOps[{MUL, VT::Int,  VT::Bool}] = [=](const Value& l, const Value& r){ return Value(l.get<Int>()  * boolToInt(r.get<Bool>())); };
    binaryOps[{MUL, VT::Bool, VT::Int }] = [=](const Value& l, const Value& r){ return Value(boolToInt(l.get<Bool>()) * r.get<Int>()); };
    binaryOps[{MUL, VT::Real, VT::Bool}] = [=](const Value& l, const Value& r){ return Value(l.get<Real>() * boolToReal(r.get<Bool>())); };
    binaryOps[{MUL, VT::Bool, VT::Real}] = [=](const Value& l, const Value& r){ return Value(boolToReal(l.get<Bool>()) * r.get<Real>()); };


    binaryOps[{MUL, VT::Str,  VT::Int }] = [](const Value& l, const Value& r){
        const Str& s = l.get<Str>(); Int times = r.get<Int>();
        if (times <= 0) return Value(Str{});
        Str out; out.reserve(s.size()*static_cast<size_t>(times));
        for (Int i = 0; i < times; ++i) out += s;
        return Value(out);
    };


    binaryOps[{MUL, VT::Str, VT::Real}] = [=](const Value& l, const Value& r){
        Real rv = r.get<Real>();
        Real iv; if (std::modf(rv, &iv) == 0.0 && iv >= static_cast<Real>(0) && iv <= static_cast<Real>(std::numeric_limits<Int>::max())) {
            Int times = static_cast<Int>(iv);
            const Str& s = l.get<Str>();
            if (times <= 0) return Value(Str{});
            Str out; out.reserve(s.size()*static_cast<size_t>(times));
            for (Int i = 0; i < times; ++i) out += s;
            return Value(out);
        }

        return Value(NANV);
    };


    binaryOps[{MUL, VT::Str, VT::Bool}] = [](const Value& l, const Value& r){
        const Str& s = l.get<Str>(); Int times = static_cast<Int>(r.get<Bool>());
        if (times <= 0) return Value(Str{});
        Str out; out.reserve(s.size()*static_cast<size_t>(times));
        for (Int i = 0; i < times; ++i) out += s;
        return Value(out);
    };


    binaryOps[{DIV, VT::Int,  VT::Int }] = [=](const Value& l, const Value& r){
        auto rv = r.get<Int>();
        if (rv == 0) {
            Int lv = l.get<Int>();
            if (lv > 0) return Value(INF);
            if (lv < 0) return Value(-INF);
            return Value(NANV);
        }
        return Value(static_cast<Real>(l.get<Int>()) / static_cast<Real>(rv));
    };
    binaryOps[{DIV, VT::Real, VT::Real}] = [=](const Value& l, const Value& r){
        auto rv = r.get<Real>();
        if (rv == static_cast<Real>(0.0)) {
            Real lv = l.get<Real>();
            if (lv > static_cast<Real>(0.0)) return Value(INF);
            if (lv < static_cast<Real>(0.0)) return Value(-INF);
            return Value(NANV);
        }
        return Value(l.get<Real>() / rv);
    };
    binaryOps[{DIV, VT::Int,  VT::Real}] = [=](const Value& l, const Value& r){
        auto rv = r.get<Real>();
        if (rv == static_cast<Real>(0.0)) {
            Int lv = l.get<Int>();
            if (lv > 0) return Value(INF);
            if (lv < 0) return Value(-INF);
            return Value(NANV);
        }
        return Value(static_cast<Real>(l.get<Int>()) / rv);
    };
    binaryOps[{DIV, VT::Real, VT::Int }] = [=](const Value& l, const Value& r){
        auto rv = r.get<Int>();
        if (rv == 0) {
            Real lv = l.get<Real>();
            if (lv > static_cast<Real>(0.0)) return Value(INF);
            if (lv < static_cast<Real>(0.0)) return Value(-INF);
            return Value(NANV);
        }
        return Value(l.get<Real>() / static_cast<Real>(rv));
    };

    binaryOps[{DIV, VT::Int,  VT::Bool}] = [=](const Value& l, const Value& r){
        Int d = boolToInt(r.get<Bool>());
        if (d == 0) {
            Int lv = l.get<Int>();
            if (lv > 0) return Value(INF);
            if (lv < 0) return Value(-INF);
            return Value(NANV);
        }
        return Value(static_cast<Real>(l.get<Int>()) / static_cast<Real>(d));
    };
    binaryOps[{DIV, VT::Real, VT::Bool}] = [=](const Value& l, const Value& r){
        Int d = boolToInt(r.get<Bool>());
        if (d == 0) {
            Real lv = l.get<Real>();
            if (lv > static_cast<Real>(0.0)) return Value(INF);
            if (lv < static_cast<Real>(0.0)) return Value(-INF);
            return Value(NANV);
        }
        return Value(l.get<Real>() / static_cast<Real>(d));
    };
    binaryOps[{DIV, VT::Bool, VT::Int }] = [=](const Value& l, const Value& r){
        Int num = boolToInt(l.get<Bool>()); Int d = r.get<Int>();
        if (d == 0) {
            if (num > 0) return Value(INF);
            if (num < 0) return Value(-INF);
            return Value(NANV);
        }
        return Value(static_cast<Real>(num) / static_cast<Real>(d));
    };
    binaryOps[{DIV, VT::Bool, VT::Real}] = [=](const Value& l, const Value& r){
        Real d = r.get<Real>();
        Int num = boolToInt(l.get<Bool>());
        if (d == static_cast<Real>(0.0)) {
            if (num > 0) return Value(INF);
            if (num < 0) return Value(-INF);
            return Value(NANV);
        }
        return Value(static_cast<Real>(num) / d);
    };


    binaryOps[{MOD, VT::Int,  VT::Int }] = [=](const Value& l, const Value& r){
        auto rv = r.get<Int>();
        if (rv == 0) {

            return Value(NANV);
        }
        return Value(l.get<Int>() % rv);
    };
    binaryOps[{MOD, VT::Int,  VT::Bool}] = [=](const Value& l, const Value& r){
        Int d = boolToInt(r.get<Bool>()); if (d == 0) return Value(NANV);
        return Value(l.get<Int>() % d);
    };
    binaryOps[{MOD, VT::Bool, VT::Int }] = [=](const Value& l, const Value& r){
        Int n = boolToInt(l.get<Bool>()); Int d = r.get<Int>(); if (d == 0) return Value(NANV);
        return Value(n % d);
    };
    binaryOps[{MOD, VT::Bool, VT::Bool}] = [=](const Value& l, const Value& r){
        Int n = boolToInt(l.get<Bool>()); Int d = boolToInt(r.get<Bool>()); if (d == 0) return Value(NANV);
        return Value(n % d);
    };


    binaryOps[{POW, VT::Int,  VT::Int }] = [](const Value& l, const Value& r){ return Value(std::pow(static_cast<Real>(l.get<Int>()),  static_cast<Real>(r.get<Int>()))); };
    binaryOps[{POW, VT::Real, VT::Real}] = [](const Value& l, const Value& r){ return Value(std::pow(l.get<Real>(), r.get<Real>())); };
    binaryOps[{POW, VT::Int,  VT::Real}] = [](const Value& l, const Value& r){ return Value(std::pow(static_cast<Real>(l.get<Int>()),  r.get<Real>())); };
    binaryOps[{POW, VT::Real, VT::Int }] = [](const Value& l, const Value& r){ return Value(std::pow(l.get<Real>(), static_cast<Real>(r.get<Int>()))); };
    binaryOps[{POW, VT::Bool, VT::Int }]  = [=](const Value& l, const Value& r){ return Value(std::pow(static_cast<Real>(boolToInt(l.get<Bool>())), static_cast<Real>(r.get<Int>()))); };
    binaryOps[{POW, VT::Bool, VT::Real}]  = [=](const Value& l, const Value& r){ return Value(std::pow(static_cast<Real>(boolToInt(l.get<Bool>())), r.get<Real>())); };
    binaryOps[{POW, VT::Int,  VT::Bool}]  = [=](const Value& l, const Value& r){ return Value(std::pow(static_cast<Real>(l.get<Int>()), static_cast<Real>(boolToInt(r.get<Bool>())))); };
    binaryOps[{POW, VT::Real, VT::Bool}]  = [=](const Value& l, const Value& r){ return Value(std::pow(l.get<Real>(), static_cast<Real>(boolToInt(r.get<Bool>())))); };


    binaryOps[{BIT_AND, VT::Int,  VT::Int }] = [](const Value& l, const Value& r){ return Value(l.get<Int>()  & r.get<Int>()); };
    binaryOps[{BIT_OR,  VT::Int,  VT::Int }] = [](const Value& l, const Value& r){ return Value(l.get<Int>()  | r.get<Int>()); };
    binaryOps[{BIT_XOR, VT::Int,  VT::Int }] = [](const Value& l, const Value& r){ return Value(l.get<Int>()  ^ r.get<Int>()); };
    binaryOps[{LSHIFT,  VT::Int,  VT::Int }] = [](const Value& l, const Value& r){ return Value(l.get<Int>()  << r.get<Int>()); };
    binaryOps[{RSHIFT,  VT::Int,  VT::Int }] = [](const Value& l, const Value& r){ return Value(l.get<Int>()  >> r.get<Int>()); };

    binaryOps[{BIT_AND, VT::Bool, VT::Bool}] = [](const Value& l, const Value& r){ return Value(l.get<Bool>() & r.get<Bool>()); };
    binaryOps[{BIT_OR,  VT::Bool, VT::Bool}] = [](const Value& l, const Value& r){ return Value(l.get<Bool>() | r.get<Bool>()); };


    binaryOps[{BIT_AND, VT::Int,  VT::Bool}] = [=](const Value& l, const Value& r){ return Value(l.get<Int>() & boolToInt(r.get<Bool>())); };
    binaryOps[{BIT_AND, VT::Bool, VT::Int }] = [=](const Value& l, const Value& r){ return Value(boolToInt(l.get<Bool>()) & r.get<Int>()); };
    binaryOps[{BIT_OR,  VT::Int,  VT::Bool}] = [=](const Value& l, const Value& r){ return Value(l.get<Int>() | boolToInt(r.get<Bool>())); };
    binaryOps[{BIT_OR,  VT::Bool, VT::Int }] = [=](const Value& l, const Value& r){ return Value(boolToInt(l.get<Bool>()) | r.get<Int>()); };
    binaryOps[{BIT_XOR, VT::Int,  VT::Bool}] = [=](const Value& l, const Value& r){ return Value(l.get<Int>() ^ boolToInt(r.get<Bool>())); };
    binaryOps[{BIT_XOR, VT::Bool, VT::Int }] = [=](const Value& l, const Value& r){ return Value(boolToInt(l.get<Bool>()) ^ r.get<Int>()); };

    binaryOps[{LSHIFT,  VT::Int,  VT::Bool}] = [=](const Value& l, const Value& r){ return Value(l.get<Int>() << boolToInt(r.get<Bool>())); };
    binaryOps[{RSHIFT,  VT::Int,  VT::Bool}] = [=](const Value& l, const Value& r){ return Value(l.get<Int>() >> boolToInt(r.get<Bool>())); };



    binaryOps[{EQ,  VT::Int,  VT::Int }] = [](const Value& l, const Value& r){ return Value(l.get<Int>()  == r.get<Int>()); };
    binaryOps[{EQ,  VT::Real, VT::Real}] = [=](const Value& l, const Value& r){ return Value(realEq(l.get<Real>(), r.get<Real>())); };
    binaryOps[{EQ,  VT::Int,  VT::Real}] = [=](const Value& l, const Value& r){ return Value(realEq(static_cast<Real>(l.get<Int>()), r.get<Real>())); };
    binaryOps[{EQ,  VT::Real, VT::Int }] = [=](const Value& l, const Value& r){ return Value(realEq(l.get<Real>(), static_cast<Real>(r.get<Int>()))); };
    binaryOps[{EQ,  VT::Bool, VT::Bool}] = [](const Value& l, const Value& r){ return Value(l.get<Bool>() == r.get<Bool>()); };

    binaryOps[{EQ,  VT::Int,  VT::Bool}] = [](const Value& l, const Value& r){ return Value(l.get<Int>()  == static_cast<Int>(r.get<Bool>())); };
    binaryOps[{EQ,  VT::Bool, VT::Int }] = [](const Value& l, const Value& r){ return Value(static_cast<Int>(l.get<Bool>()) == r.get<Int>()); };
    binaryOps[{EQ,  VT::Real, VT::Bool}] = [=](const Value& l, const Value& r){ return Value(realEq(l.get<Real>(), static_cast<Real>(static_cast<Int>(r.get<Bool>())))); };
    binaryOps[{EQ,  VT::Bool, VT::Real}] = [=](const Value& l, const Value& r){ return Value(realEq(static_cast<Real>(static_cast<Int>(l.get<Bool>())), r.get<Real>())); };

    binaryOps[{EQ,  VT::Str,  VT::Str }] = [](const Value& l, const Value& r){ return Value(l.get<Str>() == r.get<Str>()); };

    binaryOps[{EQ,  VT::Null, VT::Null}] = [](const Value&, const Value&){ return Value(true); };


    std::vector<VT> allTypes = {
        VT::Null, VT::Bool, VT::Int, VT::Real, VT::Str,
        VT::Array, VT::Object, VT::Upvalue, VT::Function,
        VT::Class, VT::Instance, VT::BoundMethod, VT::Proto, VT::NativeFn
    };
    for (VT t : allTypes) {
        if (t == VT::Null) continue;
        binaryOps[{EQ, VT::Null, t}] = [](const Value&, const Value&){ return Value(false); };
        binaryOps[{EQ, t, VT::Null}] = [](const Value&, const Value&){ return Value(false); };
        binaryOps[{NEQ, VT::Null, t}] = [](const Value&, const Value&){ return Value(true); };
        binaryOps[{NEQ, t, VT::Null}] = [](const Value&, const Value&){ return Value(true); };
    }

    for (VT t : allTypes) {
        if (t == VT::Bool) continue;
        binaryOps[{EQ, VT::Bool, t}] = [=](const Value& l, const Value& r){ return Value(static_cast<Bool>(toBool(l) == toBool(r))); };
        binaryOps[{EQ, t, VT::Bool}] = [=](const Value& l, const Value& r){ return Value(static_cast<Bool>(toBool(l) == toBool(r))); };
        binaryOps[{NEQ, VT::Bool, t}] = [=](const Value& l, const Value& r){ return Value(static_cast<Bool>(toBool(l) != toBool(r))); };
        binaryOps[{NEQ, t, VT::Bool}] = [=](const Value& l, const Value& r){ return Value(static_cast<Bool>(toBool(l) != toBool(r))); };
    }

    binaryOps[{NEQ, VT::Int,  VT::Int }] = [](const Value& l, const Value& r){ return Value(l.get<Int>()  != r.get<Int>()); };
    binaryOps[{NEQ, VT::Real, VT::Real}] = [=](const Value& l, const Value& r){ return Value(!realEq(l.get<Real>(), r.get<Real>())); };
    binaryOps[{NEQ, VT::Int,  VT::Real}] = [=](const Value& l, const Value& r){ return Value(!realEq(static_cast<Real>(l.get<Int>()), r.get<Real>())); };
    binaryOps[{NEQ, VT::Real, VT::Int }] = [=](const Value& l, const Value& r){ return Value(!realEq(l.get<Real>(), static_cast<Real>(r.get<Int>()))); };
    binaryOps[{NEQ, VT::Bool, VT::Bool}] = [](const Value& l, const Value& r){ return Value(l.get<Bool>() != r.get<Bool>()); };
    binaryOps[{NEQ, VT::Str,  VT::Str }] = [](const Value& l, const Value& r){ return Value(l.get<Str>() != r.get<Str>()); };
    binaryOps[{NEQ, VT::Null, VT::Null}] = [](const Value&, const Value&){ return Value(false); };


    binaryOps[{LT, VT::Int, VT::Int}] = [](const Value& l, const Value& r){ return Value(l.get<Int>()  <  r.get<Int>()); };
    binaryOps[{LE, VT::Int, VT::Int}] = [](const Value& l, const Value& r){ return Value(l.get<Int>()  <= r.get<Int>()); };
    binaryOps[{GT, VT::Int, VT::Int}] = [](const Value& l, const Value& r){ return Value(l.get<Int>()  >  r.get<Int>()); };
    binaryOps[{GE, VT::Int, VT::Int}] = [](const Value& l, const Value& r){ return Value(l.get<Int>()  >= r.get<Int>()); };
    binaryOps[{LT, VT::Real, VT::Real}] = [](const Value& l, const Value& r){ return Value(l.get<Real>() <  r.get<Real>()); };
    binaryOps[{LE, VT::Real, VT::Real}] = [](const Value& l, const Value& r){ return Value(l.get<Real>() <= r.get<Real>()); };
    binaryOps[{GT, VT::Real, VT::Real}] = [](const Value& l, const Value& r){ return Value(l.get<Real>() >  r.get<Real>()); };
    binaryOps[{GE, VT::Real, VT::Real}] = [](const Value& l, const Value& r){ return Value(l.get<Real>() >= r.get<Real>()); };

    binaryOps[{LT, VT::Int,  VT::Real}] = [](const Value& l, const Value& r){ return Value(static_cast<Real>(l.get<Int>()) <  r.get<Real>()); };
    binaryOps[{LE, VT::Int,  VT::Real}] = [](const Value& l, const Value& r){ return Value(static_cast<Real>(l.get<Int>()) <= r.get<Real>()); };
    binaryOps[{GT, VT::Int,  VT::Real}] = [](const Value& l, const Value& r){ return Value(static_cast<Real>(l.get<Int>()) >  r.get<Real>()); };
    binaryOps[{GE, VT::Int,  VT::Real}] = [](const Value& l, const Value& r){ return Value(static_cast<Real>(l.get<Int>()) >= r.get<Real>()); };
    binaryOps[{LT, VT::Real, VT::Int }] = [](const Value& l, const Value& r){ return Value(l.get<Real>() <  static_cast<Real>(r.get<Int>())); };
    binaryOps[{LE, VT::Real, VT::Int }] = [](const Value& l, const Value& r){ return Value(l.get<Real>() <= static_cast<Real>(r.get<Int>())); };
    binaryOps[{GT, VT::Real, VT::Int }] = [](const Value& l, const Value& r){ return Value(l.get<Real>() >  static_cast<Real>(r.get<Int>())); };
    binaryOps[{GE, VT::Real, VT::Int }] = [](const Value& l, const Value& r){ return Value(l.get<Real>() >= static_cast<Real>(r.get<Int>())); };

    binaryOps[{LT, VT::Bool, VT::Bool}] = [](const Value& l, const Value& r){ return Value(static_cast<Int>(l.get<Bool>()) <  static_cast<Int>(r.get<Bool>())); };
    binaryOps[{LE, VT::Bool, VT::Bool}] = [](const Value& l, const Value& r){ return Value(static_cast<Int>(l.get<Bool>()) <= static_cast<Int>(r.get<Bool>())); };
    binaryOps[{GT, VT::Bool, VT::Bool}] = [](const Value& l, const Value& r){ return Value(static_cast<Int>(l.get<Bool>()) >  static_cast<Int>(r.get<Bool>())); };
    binaryOps[{GE, VT::Bool, VT::Bool}] = [](const Value& l, const Value& r){ return Value(static_cast<Int>(l.get<Bool>()) >= static_cast<Int>(r.get<Bool>())); };

    binaryOps[{LT, VT::Int,  VT::Bool}] = [](const Value& l, const Value& r){ return Value(l.get<Int>()  <  static_cast<Int>(r.get<Bool>())); };
    binaryOps[{LE, VT::Int,  VT::Bool}] = [](const Value& l, const Value& r){ return Value(l.get<Int>()  <= static_cast<Int>(r.get<Bool>())); };
    binaryOps[{GT, VT::Int,  VT::Bool}] = [](const Value& l, const Value& r){ return Value(l.get<Int>()  >  static_cast<Int>(r.get<Bool>())); };
    binaryOps[{GE, VT::Int,  VT::Bool}] = [](const Value& l, const Value& r){ return Value(l.get<Int>()  >= static_cast<Int>(r.get<Bool>())); };
    binaryOps[{LT, VT::Bool, VT::Int }] = [](const Value& l, const Value& r){ return Value(static_cast<Int>(l.get<Bool>()) <  r.get<Int>()); };
    binaryOps[{LE, VT::Bool, VT::Int }] = [](const Value& l, const Value& r){ return Value(static_cast<Int>(l.get<Bool>()) <= r.get<Int>()); };
    binaryOps[{GT, VT::Bool, VT::Int }] = [](const Value& l, const Value& r){ return Value(static_cast<Int>(l.get<Bool>()) >  r.get<Int>()); };
    binaryOps[{GE, VT::Bool, VT::Int }] = [](const Value& l, const Value& r){ return Value(static_cast<Int>(l.get<Bool>()) >= r.get<Int>()); };
    binaryOps[{LT, VT::Real, VT::Bool}] = [](const Value& l, const Value& r){ return Value(l.get<Real>() <  static_cast<Real>(static_cast<Int>(r.get<Bool>()))); };
    binaryOps[{LE, VT::Real, VT::Bool}] = [](const Value& l, const Value& r){ return Value(l.get<Real>() <= static_cast<Real>(static_cast<Int>(r.get<Bool>()))); };
    binaryOps[{GT, VT::Real, VT::Bool}] = [](const Value& l, const Value& r){ return Value(l.get<Real>() >  static_cast<Real>(static_cast<Int>(r.get<Bool>()))); };
    binaryOps[{GE, VT::Real, VT::Bool}] = [](const Value& l, const Value& r){ return Value(l.get<Real>() >= static_cast<Real>(static_cast<Int>(r.get<Bool>()))); };
    binaryOps[{LT, VT::Bool, VT::Real}] = [](const Value& l, const Value& r){ return Value(static_cast<Real>(static_cast<Int>(l.get<Bool>())) <  r.get<Real>()); };
    binaryOps[{LE, VT::Bool, VT::Real}] = [](const Value& l, const Value& r){ return Value(static_cast<Real>(static_cast<Int>(l.get<Bool>())) <= r.get<Real>()); };
    binaryOps[{GT, VT::Bool, VT::Real}] = [](const Value& l, const Value& r){ return Value(static_cast<Real>(static_cast<Int>(l.get<Bool>())) >  r.get<Real>()); };
    binaryOps[{GE, VT::Bool, VT::Real}] = [](const Value& l, const Value& r){ return Value(static_cast<Real>(static_cast<Int>(l.get<Bool>())) >= r.get<Real>()); };

    binaryOps[{LT, VT::Str, VT::Str}] = [](const Value& l, const Value& r){ return Value(l.get<Str>() <  r.get<Str>()); };
    binaryOps[{LE, VT::Str, VT::Str}] = [](const Value& l, const Value& r){ return Value(l.get<Str>() <= r.get<Str>()); };
    binaryOps[{GT, VT::Str, VT::Str}] = [](const Value& l, const Value& r){ return Value(l.get<Str>() >  r.get<Str>()); };
    binaryOps[{GE, VT::Str, VT::Str}] = [](const Value& l, const Value& r){ return Value(l.get<Str>() >= r.get<Str>()); };


    unaryOps[{NEG,     VT::Int }]  = [](const Value& r){ return Value(-r.get<Int>()); };
    unaryOps[{NEG,     VT::Real}]  = [](const Value& r){ return Value(-r.get<Real>()); };
    unaryOps[{BIT_NOT, VT::Int }]  = [](const Value& r){ return Value(~r.get<Int>()); };


    for (VT t : allTypes) {
        unaryOps[{NOT, t}] = [=](const Value& r) -> Value {
            switch (t) {
                case VT::Null:   return Value(true);
                case VT::Bool:   return Value(!r.get<Bool>());
                case VT::Int:    return Value(r.get<Int>() == 0);
                case VT::Real:   return Value(r.get<Real>() == static_cast<Real>(0.0));
                case VT::Str:    return Value(r.get<Str>().empty());
                case VT::Array:  {
                    auto arr = r.get<Array>();
                    return Value(!(arr && !arr->elements.empty()));
                }
                case VT::Object: {
                    auto obj = r.get<Object>();
                    return Value(!(obj && !obj->fields.empty()));
                }
                case VT::Upvalue:
                case VT::Function:
                case VT::Class:
                case VT::Instance:
                case VT::BoundMethod:
                    return Value(false);
                default:
                    return Value(false);
            }
        };
    }
}


BinaryOpFunc* OperatorDispatcher::find(OpCode op, const Value& left, const Value& right) {
    auto key = BinaryOperationKey{op, getValueType(left), getValueType(right)};
    auto it = binaryOps.find(key);
    return (it != binaryOps.end()) ? &it->second : nullptr;
}

UnaryOpFunc* OperatorDispatcher::find(OpCode op, const Value& right) {
    auto key = UnaryOperationKey{op, getValueType(right)};
    auto it = unaryOps.find(key);
    return (it != unaryOps.end()) ? &it->second : nullptr;
}
