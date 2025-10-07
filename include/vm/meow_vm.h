#pragma once
#include "definitions.h"
#include "bytecode_parser.h"
#include "binary_parser.h"
#include "operator_dispatcher.h"
#include "memory_manager.h"
#include "meow_engine.h"
#include "pch.h"

class VMError : public std::runtime_error {
public:
    VMError(const Str& m) : std::runtime_error(m) {}
};

class GCScopeGuard {
private:
    MemoryManager* mm;
public:
    GCScopeGuard(MemoryManager* mem) : mm(mem) { if(mm) mm->disableGC(); }
    ~GCScopeGuard() { if(mm) mm->enableGC(); }
};

struct GCVisitor;

class MeowVM: public MeowEngine {
public:
    MeowVM(const Str& entryPointDir);
    MeowVM(const Str& entryPointDir, int argc, char* argv[]);
    void interpret(const Str& entryPath, Bool isBinary);
    std::vector<Value*> findRoots();
    void traceRoots(GCVisitor&);

private:
    std::vector<CallFrame> callStack;
    std::vector<Value> stackSlots;
    std::vector<Upvalue> openUpvalues;
    std::vector<Str> commandLineArgs;
    std::unordered_map<Str, Module> moduleCache;
    std::unordered_map<Module, std::unordered_map<Str, Value>> moduleGlobals;
    std::unordered_map<Str, std::unordered_map<Str, Value>> builtinMethods;
    std::unordered_map<Str, std::unordered_map<Str, Value>> builtinGetters;
    
    std::vector<ExceptionHandler> exceptionHandlers;
    BytecodeParser textParser;
    BinaryParser binaryParser;
    OperatorDispatcher opDispatcher;
    std::unique_ptr<MemoryManager> memoryManager;
    Str entryPointDir;

    using OpCodeHandler = void (MeowVM::*)();
    std::vector<OpCodeHandler> jumpTable;

    CallFrame* currentFrame = nullptr;
    const Instruction* currentInst = nullptr;
    Int currentBase = 0;

    void defineNativeFunctions();
    Module _getOrLoadModule(const Str& modulePath, const Str& importerPath, Bool isBinary);
    void run();
    void initializeJumpTable();
    void _handleRuntimeException(const VMError& e);
    void closeUpvalues(Int slotIndex);
    Upvalue captureUpvalue(Int slotIndex);
    void _executeCall(const Value& callee, Int dst, Int argStart, Int argc, Int base);

    MemoryManager* getMemoryManager() override { return this->memoryManager.get(); }
    Value call(const Value& callee, Arguments args) override;
    void registerMethod(const Str& typeName, const Str& methodName, const Value& method) override;
    void registerGetter(const Str& typeName, const Str& propName, const Value& getter) override;
    const std::vector<Str>& getArguments() const override { return commandLineArgs; }

    Function wrapClosure(const Value& maybeCallable);
    std::optional<Value> getMagicMethod(const Value& obj, const Str& name);
    
    void opMove();
    void opLoadConst();
    void opLoadInt();
    void opLoadNull();
    void opLoadTrue();
    void opLoadFalse();
    void opBinary();
    void opUnary();
    void opGetGlobal();
    void opSetGlobal();
    void opGetUpvalue();
    void opSetUpvalue();
    void opClosure();
    void opCloseUpvalues();
    void opJump();
    void opJumpIfFalse();
    void opJumpIfTrue();
    void opCall();
    void opReturn();
    void opHalt();
    void opNewArray();
    void opNewHash();
    void opGetIndex();
    void opSetIndex();
    void opGetKeys();
    void opGetValues();
    void opNewClass();
    void opNewInstance();
    void opGetProp();
    void opSetProp();
    void opSetMethod();
    void opInherit();
    void opGetSuper();
    void opImportModule();
    void opExport();
    void opGetExport();
    void opGetModuleExport();
    void opImportAll();
    void opSetupTry();
    void opPopTry();
    void opThrow();
    void opUnsupported();

    Bool isNull(const Value& v) const;
    Bool isBool(const Value& v) const;
    Bool isDouble(const Value& v) const;
    Bool isInt(const Value& v) const;
    Bool isString(const Value& v) const;
    Bool isVector(const Value& v) const;
    Bool isMap(const Value& v) const;
    Bool isInstance(const Value& v) const;
    Bool isClass(const Value& v) const;
    Bool isClosure(const Value& v) const;
    Bool isFunctionProto(const Value& v) const;
    Bool isModule(const Value& v) const;
    Bool isBound(const Value& v) const;
    Bool isNative(const Value& v) const;

    Str _toString(const Value& v);
    Int _toInt(const Value& v) const;
    Real _toDouble(const Value& v) const;
    Bool _isTruthy(const Value& v) const;
    Bool _areValuesEqual(const Value& a, const Value& b) const;
    
    Str opToString(OpCode op) const;

    [[noreturn]] void throwVMError(const Str& msg);
};