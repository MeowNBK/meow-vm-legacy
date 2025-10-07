#include "meow_vm.h"
#include "mark_sweep_gc.h"
#include "meow_object.h"

MeowVM::MeowVM(const Str& entryPointDir_) : entryPointDir(entryPointDir_) {
    memoryManager = std::make_unique<MemoryManager>(std::make_unique<MarkSweepGC>());
    memoryManager->setVM(this);
    defineNativeFunctions();
    initializeJumpTable();
}

MeowVM::MeowVM(const Str& entryPointDir_, int argc, char* argv[]) : entryPointDir(entryPointDir_) {
    memoryManager = std::make_unique<MemoryManager>(std::make_unique<MarkSweepGC>());
    memoryManager->setVM(this);
    defineNativeFunctions();
    initializeJumpTable();

    commandLineArgs.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        this->commandLineArgs.push_back(argv[i]);
    }
}

void MeowVM::interpret(const Str& entryPath, Bool isBinary) {
    callStack.clear();
    stackSlots.clear();
    openUpvalues.clear();
    moduleCache.clear();
    exceptionHandlers.clear();
    defineNativeFunctions();

    try {
        auto entryMod = _getOrLoadModule(entryPath, entryPointDir, isBinary);

        if (!entryMod->isExecuted) {
            if (!entryMod->hasMain) throw VMError("Entry module thiếu @main.");
            entryMod->isExecuting = true;

            auto closure = memoryManager->newObject<ObjClosure>(entryMod->mainProto);
            Int base = static_cast<Int>(stackSlots.size());
            stackSlots.resize(base + entryMod->mainProto->numRegisters, Value(Null{}));
            CallFrame frame(closure, base, entryMod, 0, -1);
            callStack.push_back(frame);
        }
        run();
        entryMod->isExecuted = true;

    } catch (const VMError& e) {
        std::cerr << "💥 Lỗi nghiêm trọng trong MeowScript VM: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "🤯 Lỗi C++ không lường trước: " << e.what() << std::endl;
    }
}

void MeowVM::traceRoots(GCVisitor& visitor) {
    for (Value& val : stackSlots) {
        visitor.visitValue(val);
    }

    for (auto& pair : moduleCache) {
        visitor.visitObject(pair.second);
    }

    for (ObjUpvalue* upvalue : openUpvalues) {
        visitor.visitObject(upvalue);
    }

    for (CallFrame& frame : callStack) {
        visitor.visitObject(frame.closure);
        visitor.visitObject(frame.module);
    }

    for (auto& type_pair : builtinMethods) {
        for (auto& method_pair : type_pair.second) {
            visitor.visitValue(method_pair.second);
        }
    }
    for (auto& type_pair : builtinGetters) {
        for (auto& getter_pair : type_pair.second) {
            visitor.visitValue(getter_pair.second);
        }
    }
}

std::vector<Value*> MeowVM::findRoots() {
    std::vector<Value*> roots;

    for (Value& val : stackSlots) {
        roots.push_back(&val);
    }

    for (auto& pair : moduleCache) {
        ObjModule* module = pair.second;
        for (auto& global_pair : module->globals) {
            roots.push_back(&global_pair.second);
        }
        for (auto& export_pair : module->exports) {
            roots.push_back(&export_pair.second);
        }
    }

    for (ObjUpvalue* upvalue : openUpvalues) {
        if (upvalue->state == ObjUpvalue::State::CLOSED) {
            roots.push_back(&upvalue->closed);
        }
    }

    return roots;
}

void MeowVM::run() {
    while (!callStack.empty()) {
        currentFrame = &callStack.back();
        auto proto = currentFrame->closure->proto;
        currentBase = currentFrame->slotStart;

        if (currentFrame->ip >= static_cast<Int>(proto->code.size())) {
            if (currentFrame->retReg != -1) {
                if (callStack.size() > 1) { 
                    CallFrame& parentFrame = callStack[callStack.size() - 2];
                    stackSlots[parentFrame.slotStart + currentFrame->retReg] = Value(Null{});
                }
            }
            callStack.pop_back();
            continue;
        }
        try {
            currentInst = &proto->code[currentFrame->ip++];
            Int32 opcode = static_cast<Int32>(currentInst->op);

            if (opcode < 0 || opcode >= static_cast<Int32>(OpCode::TOTAL_OPCODES)) {
                std::ostringstream os;
                os << "Opcode không hợp lệ với giá trị opcode là: " << opcode << std::endl;

                throwVMError(os.str());
                callStack.clear();
                return;
            }
            {
                GCScopeGuard gcGuard(memoryManager.get());
                (this->*jumpTable[opcode])();
            }

        } catch (const VMError& e) {
            _handleRuntimeException(e);
        } catch (const std::exception& e) {
            std::cerr << "🤯 Lỗi C++ không lường trước trong VM.run: " << e.what() << std::endl;
            callStack.clear();
        }
    }
}

void MeowVM::_handleRuntimeException(const VMError& e) {
    if (exceptionHandlers.empty()) {
        throw e;
    }
    ExceptionHandler handler = exceptionHandlers.back();
    exceptionHandlers.pop_back();

    while (static_cast<Int>(callStack.size() - 1) > handler.frameDepth) {
        CallFrame currentFrame = callStack.back();
        callStack.pop_back();
        closeUpvalues(currentFrame.slotStart);
    }
    stackSlots.resize(handler.stackDepth);
    CallFrame& currentFrame = callStack.back();
    currentFrame.ip = handler.catchIp;
    if (currentFrame.closure->proto->numRegisters > 0) {
        stackSlots[currentFrame.slotStart] = Value(Str(e.what()));
    }
}

void MeowVM::initializeJumpTable() {
    jumpTable.assign(static_cast<size_t>(OpCode::TOTAL_OPCODES), &MeowVM::opUnsupported);


    jumpTable[static_cast<Int32>(OpCode::MOVE)] = &MeowVM::opMove;
    jumpTable[static_cast<Int32>(OpCode::LOAD_CONST)] = &MeowVM::opLoadConst;
    jumpTable[static_cast<Int32>(OpCode::LOAD_INT)] = &MeowVM::opLoadInt;
    jumpTable[static_cast<Int32>(OpCode::LOAD_NULL)] = &MeowVM::opLoadNull;
    jumpTable[static_cast<Int32>(OpCode::LOAD_TRUE)] = &MeowVM::opLoadTrue;
    jumpTable[static_cast<Int32>(OpCode::LOAD_FALSE)] = &MeowVM::opLoadFalse;


    jumpTable[static_cast<Int32>(OpCode::ADD)] = &MeowVM::opBinary;
    jumpTable[static_cast<Int32>(OpCode::SUB)] = &MeowVM::opBinary;
    jumpTable[static_cast<Int32>(OpCode::MUL)] = &MeowVM::opBinary;
    jumpTable[static_cast<Int32>(OpCode::DIV)] = &MeowVM::opBinary;
    jumpTable[static_cast<Int32>(OpCode::MOD)] = &MeowVM::opBinary;
    jumpTable[static_cast<Int32>(OpCode::POW)] = &MeowVM::opBinary;


    jumpTable[static_cast<Int32>(OpCode::EQ)] = &MeowVM::opBinary;
    jumpTable[static_cast<Int32>(OpCode::NEQ)] = &MeowVM::opBinary;
    jumpTable[static_cast<Int32>(OpCode::GT)] = &MeowVM::opBinary;
    jumpTable[static_cast<Int32>(OpCode::GE)] = &MeowVM::opBinary;
    jumpTable[static_cast<Int32>(OpCode::LT)] = &MeowVM::opBinary;
    jumpTable[static_cast<Int32>(OpCode::LE)] = &MeowVM::opBinary;


    jumpTable[static_cast<Int32>(OpCode::BIT_AND)] = &MeowVM::opBinary;
    jumpTable[static_cast<Int32>(OpCode::BIT_OR)] = &MeowVM::opBinary;
    jumpTable[static_cast<Int32>(OpCode::BIT_XOR)] = &MeowVM::opBinary;
    jumpTable[static_cast<Int32>(OpCode::LSHIFT)] = &MeowVM::opBinary;
    jumpTable[static_cast<Int32>(OpCode::RSHIFT)] = &MeowVM::opBinary;


    jumpTable[static_cast<Int32>(OpCode::NEG)] = &MeowVM::opUnary;
    jumpTable[static_cast<Int32>(OpCode::NOT)] = &MeowVM::opUnary;
    jumpTable[static_cast<Int32>(OpCode::BIT_NOT)] = &MeowVM::opUnary;


    jumpTable[static_cast<Int32>(OpCode::GET_GLOBAL)] = &MeowVM::opGetGlobal;
    jumpTable[static_cast<Int32>(OpCode::SET_GLOBAL)] = &MeowVM::opSetGlobal;
    jumpTable[static_cast<Int32>(OpCode::GET_UPVALUE)] = &MeowVM::opGetUpvalue;
    jumpTable[static_cast<Int32>(OpCode::SET_UPVALUE)] = &MeowVM::opSetUpvalue;


    jumpTable[static_cast<Int32>(OpCode::CLOSURE)] = &MeowVM::opClosure;
    jumpTable[static_cast<Int32>(OpCode::CLOSE_UPVALUES)] = &MeowVM::opCloseUpvalues;


    jumpTable[static_cast<Int32>(OpCode::JUMP)] = &MeowVM::opJump;
    jumpTable[static_cast<Int32>(OpCode::JUMP_IF_FALSE)] = &MeowVM::opJumpIfFalse;
    jumpTable[static_cast<Int32>(OpCode::JUMP_IF_TRUE)] = &MeowVM::opJumpIfTrue;
    jumpTable[static_cast<Int32>(OpCode::CALL)] = &MeowVM::opCall;
    jumpTable[static_cast<Int32>(OpCode::RETURN)] = &MeowVM::opReturn;
    jumpTable[static_cast<Int32>(OpCode::HALT)] = &MeowVM::opHalt;


    jumpTable[static_cast<Int32>(OpCode::NEW_ARRAY)] = &MeowVM::opNewArray;
    jumpTable[static_cast<Int32>(OpCode::NEW_HASH)] = &MeowVM::opNewHash;
    jumpTable[static_cast<Int32>(OpCode::GET_INDEX)] = &MeowVM::opGetIndex;
    jumpTable[static_cast<Int32>(OpCode::SET_INDEX)] = &MeowVM::opSetIndex;
    jumpTable[static_cast<Int32>(OpCode::GET_KEYS)] = &MeowVM::opGetKeys;
    jumpTable[static_cast<Int32>(OpCode::GET_VALUES)] = &MeowVM::opGetValues;


    jumpTable[static_cast<Int32>(OpCode::NEW_CLASS)] = &MeowVM::opNewClass;
    jumpTable[static_cast<Int32>(OpCode::NEW_INSTANCE)] = &MeowVM::opNewInstance;
    jumpTable[static_cast<Int32>(OpCode::GET_PROP)] = &MeowVM::opGetProp;
    jumpTable[static_cast<Int32>(OpCode::SET_PROP)] = &MeowVM::opSetProp;
    jumpTable[static_cast<Int32>(OpCode::SET_METHOD)] = &MeowVM::opSetMethod;
    jumpTable[static_cast<Int32>(OpCode::INHERIT)] = &MeowVM::opInherit;
    jumpTable[static_cast<Int32>(OpCode::GET_SUPER)] = &MeowVM::opGetSuper;


    jumpTable[static_cast<Int32>(OpCode::IMPORT_MODULE)] = &MeowVM::opImportModule;
    jumpTable[static_cast<Int32>(OpCode::EXPORT)] = &MeowVM::opExport;
    jumpTable[static_cast<Int32>(OpCode::GET_EXPORT)] = &MeowVM::opGetExport;
    jumpTable[static_cast<Int32>(OpCode::GET_MODULE_EXPORT)] = &MeowVM::opGetModuleExport;
    jumpTable[static_cast<Int32>(OpCode::IMPORT_ALL)] = &MeowVM::opImportAll;


    jumpTable[static_cast<Int32>(OpCode::SETUP_TRY)] = &MeowVM::opSetupTry;
    jumpTable[static_cast<Int32>(OpCode::POP_TRY)] = &MeowVM::opPopTry;
    jumpTable[static_cast<Int32>(OpCode::THROW)] = &MeowVM::opThrow;
}