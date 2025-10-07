#pragma once

#include "value.h"
#include "op_codes.h"
#include "meow_object.h"
#include "pch.h"

struct Instruction {
    OpCode op;
    std::vector<Int> args;
    Instruction(OpCode op = OpCode::HALT, std::vector<Int> args = {}) : op(op), args(std::move(args)) {}
};

struct UpvalueDesc {
    Bool isLocal;
    Int index;
    UpvalueDesc(Bool local = false, Int idx = 0) : isLocal(local), index(idx) {}
};

struct ExceptionHandler {
    Int catchIp;
    Int frameDepth;
    Int stackDepth;
    ExceptionHandler(Int c = 0, Int f = 0, Int s = 0) : catchIp(c), frameDepth(f), stackDepth(s) {}
};

struct ObjFunctionProto : public MeowObject {
    Int numRegisters = 0;
    Int numUpvalues = 0;
    Str sourceName = "<anon>";
    std::vector<Instruction> code;
    std::vector<Value> constantPool;
    std::vector<UpvalueDesc> upvalueDescs;
    std::unordered_map<Str, Int> labels;
    std::vector<std::tuple<Int, Int, Str>> pendingJumps;

    ObjFunctionProto(Int regs = 0, Int ups = 0, Str name = "<anon>")
        : numRegisters(regs), numUpvalues(ups), sourceName(std::move(name)) {}

    void trace(GCVisitor& visitor) override {
        for (auto& constant : constantPool) {
            visitor.visitValue(constant);
        }
    }
};

struct ObjModule : public MeowObject {
    Str name;
    Str path;
    std::unordered_map<Str, Value> globals;
    std::unordered_map<Str, Value> exports;
    Bool isExecuted = false;
    Bool isBinary = false;

    Proto mainProto;
    Bool hasMain = false;

    Bool isExecuting = false;

    ObjModule(Str n = "", Str p = "", Bool b = false)
        : name(std::move(n)), path(std::move(p)), isBinary(b) {}

    void trace(GCVisitor& visitor) override {
        for (auto& kv : globals) visitor.visitValue(kv.second);
        for (auto& kv : exports) visitor.visitValue(kv.second);
        visitor.visitObject(mainProto);
    }
};

struct ObjUpvalue : public MeowObject {
    enum class State { OPEN, CLOSED };
    State state = State::OPEN;
    Int slotIndex = 0;
    Value closed = Null{};
    ObjUpvalue(Int idx = 0) : slotIndex(idx) {}
    void close(const Value& v) { 
        closed = v; 
        state = State::CLOSED; 
    }

    void trace(GCVisitor& visitor) override {
        visitor.visitValue(closed);
    }
};

struct ObjClosure : public MeowObject {
    Proto proto;
    std::vector<Upvalue> upvalues;
    ObjClosure(Proto p = nullptr) : proto(p), upvalues(p ? p->numUpvalues : 0) {}

    void trace(GCVisitor& visitor) override {
        visitor.visitObject(proto);
        for (auto& uv : upvalues) {
            visitor.visitObject(uv);
        }
    }
};

struct CallFrame {
    Function closure;
    Int slotStart;
    Module module;
    Int ip;
    Int retReg;
    CallFrame(Function c, Int start, Module m, Int ip_, Int ret)
        : closure(c), slotStart(start), module(m), ip(ip_), retReg(ret) {}
};

struct ObjClass : public MeowObject {
    Str name;
    std::optional<Class> superclass;
    std::unordered_map<Str, Value> methods;
    ObjClass(Str n = "") : name(std::move(n)) {}

    void trace(GCVisitor& visitor) override {
        if (superclass) {
            visitor.visitObject(*superclass);
        }
        for (auto& method : methods) {
            visitor.visitValue(method.second);
        }
    }
};

struct ObjInstance : public MeowObject {
    Class klass;
    std::unordered_map<Str, Value> fields;
    ObjInstance(Class k = nullptr) : klass(k) {}

    void trace(GCVisitor& visitor) override {
        visitor.visitObject(klass);
        for (auto& field : fields) {
            visitor.visitValue(field.second);
        }
    }
};

struct ObjBoundMethod : public MeowObject {
    Instance receiver;
    Function callable;
    ObjBoundMethod(Instance r = nullptr, Function c = nullptr) : receiver(r), callable(c) {}

    void trace(GCVisitor& visitor) override {
        visitor.visitObject(receiver);
        visitor.visitObject(callable);
    }
};

struct ObjArray : public MeowObject {
    std::vector<Value> elements;
    ObjArray() = default;
    ObjArray(std::vector<Value> v) : elements(std::move(v)) {}

    void trace(GCVisitor& visitor) override {
        for (auto& element : elements) {
            visitor.visitValue(element);
        }
    }
};

struct ObjObject : public MeowObject {
    std::unordered_map<Str, Value> fields;
    ObjObject() = default;
    ObjObject(std::unordered_map<Str, Value> f) : fields(std::move(f)) {}

    void trace(GCVisitor& visitor) override {
        for (auto& field : fields) {
            visitor.visitValue(field.second);
        }
    }
};