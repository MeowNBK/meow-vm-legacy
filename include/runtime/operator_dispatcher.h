#pragma once

#include "op_codes.h"
#include "value.h"
#include "pch.h"

class Value;

enum class ValueType {
    Null, Int, Real, Bool, Str, Array, Object, Upvalue, Function, Class, Instance, BoundMethod, Proto, NativeFn
};

ValueType getValueType(const Value& value);

Str valueTypeName(ValueType t);

struct BinaryOperationKey {
    OpCode op;
    ValueType left;
    ValueType right;
    Bool operator==(const BinaryOperationKey&) const = default;
};

struct UnaryOperationKey {
    OpCode op;
    ValueType right;
    Bool operator==(const UnaryOperationKey&) const = default;
};

namespace std{
    template<> 
    struct hash<BinaryOperationKey> {
        size_t operator()(const BinaryOperationKey& k) const {
            size_t h1 = std::hash<int>()(static_cast<int>(k.op));
            size_t h2 = std::hash<int>()(static_cast<int>(k.left));
            size_t h3 = std::hash<int>()(static_cast<int>(k.right));
            return ((h1 ^ (h2 << 1)) >> 1) ^ (h3 << 1);
        }
    };

    template<> 
    struct hash<UnaryOperationKey> {
        size_t operator()(const UnaryOperationKey& k) const {
            size_t h1 = std::hash<int>()(static_cast<int>(k.op));
            size_t h2 = std::hash<int>()(static_cast<int>(k.right));
            return ((h1 ^ (h2 << 1)) >> 1);
        }
    };
}

using BinaryOpFunc = std::function<Value(const Value&, const Value&)>;
using UnaryOpFunc = std::function<Value(const Value&)>;

class OperatorDispatcher {
public:
    std::unordered_map<BinaryOperationKey, BinaryOpFunc> binaryOps;
    std::unordered_map<UnaryOperationKey, UnaryOpFunc> unaryOps;

    OperatorDispatcher();

    BinaryOpFunc* find(OpCode op, const Value& left, const Value& right);
    UnaryOpFunc* find(OpCode op, const Value& right);
};