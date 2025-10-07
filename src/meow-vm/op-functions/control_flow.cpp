#include "meow_vm.h"

void MeowVM::opClosure() {
    auto proto = currentFrame->closure->proto;
    Int dst = currentInst->args[0], 
        protoIdx = currentInst->args[1];
    if (protoIdx < 0 || protoIdx >= static_cast<Int>(proto->constantPool.size()) || !isFunctionProto(proto->constantPool[protoIdx])) {
        throwVMError("CLOSURE constant must be a FunctionProto.");
    }
    auto childProto = proto->constantPool[protoIdx].get<Proto>();
    auto closure = memoryManager->newObject<ObjClosure>(childProto);

    closure->upvalues.resize(childProto->upvalueDescs.size(), nullptr);

    for (size_t i = 0; i < childProto->upvalueDescs.size(); ++i) {
        auto& desc = childProto->upvalueDescs[i];
        if (desc.isLocal) {
            closure->upvalues[i] = captureUpvalue(currentBase + desc.index);
        } else {
            if (desc.index < 0 || desc.index >= static_cast<Int>(currentFrame->closure->upvalues.size())) {
                throwVMError("CLOSURE: parent upvalue index OOB");
            }
            closure->upvalues[i] = currentFrame->closure->upvalues[desc.index];
        }
    }
    stackSlots[currentBase + dst] = Value(closure);
}

void MeowVM::opCloseUpvalues() {
    closeUpvalues(currentBase + currentInst->args[0]);
}

void MeowVM::opJump() {
    Int target = currentInst->args[0];
    auto proto = currentFrame->closure->proto;
    if (target < 0 || target >= static_cast<Int>(proto->code.size())) 
        throwVMError("JUMP target OOB");
    currentFrame->ip = target;
}

void MeowVM::opJumpIfFalse() {
    Int reg = currentInst->args[0], target = currentInst->args[1];
    if (!_isTruthy(stackSlots[currentBase + reg])) {
        auto proto = currentFrame->closure->proto;
        if (target < 0 || target >= static_cast<Int>(proto->code.size())) 
            throwVMError("JUMP_IF_FALSE target OOB");
        currentFrame->ip = target;
    }
}

void MeowVM::opJumpIfTrue() {
    Int reg = currentInst->args[0];
    Int target = currentInst->args[1];

    if (_isTruthy(stackSlots[currentBase + reg])) {
        auto proto = currentFrame->closure->proto;
        if (target < 0 || target >= static_cast<Int>(proto->code.size())) {
            throwVMError("JUMP_IF_TRUE target OOB");
        }
        currentFrame->ip = target;
    }
}

void MeowVM::opCall() {
    Int dst = currentInst->args[0], fnReg = currentInst->args[1], argStart = currentInst->args[2], argc = currentInst->args[3];
    auto& callee = stackSlots[currentBase + fnReg];
    _executeCall(callee, dst, argStart, argc, currentBase);
}

void MeowVM::opReturn() {
    Value retVal = (currentInst->args.empty() || currentInst->args[0] < 0) ? Value(Null{}) : stackSlots[currentBase + currentInst->args[0]];
    closeUpvalues(currentBase);

    CallFrame poppedFrame = *currentFrame;
    callStack.pop_back();

    if (callStack.empty()) {
        stackSlots.clear();
        currentFrame = nullptr;
        currentInst = nullptr;
        return;
    }

    CallFrame& caller = callStack.back();
    Int callerBase = caller.slotStart;

    Int callerRegs = 0;
    if (caller.closure && caller.closure->proto) {
        callerRegs = caller.closure->proto->numRegisters;
    } 
    Int minSize = callerBase + std::max<Int>(callerRegs, 1);
    if (static_cast<Int>(stackSlots.size()) < minSize) {
        stackSlots.resize(minSize, Value(Null{}));
    } else {
        stackSlots.resize(minSize);
    }

    Int destReg = poppedFrame.retReg;
    if (destReg != -1) {
        Int need = callerBase + destReg + 1;
        if (static_cast<Int>(stackSlots.size()) < need) {
            stackSlots.resize(need, Value(Null{}));
        }
        stackSlots[callerBase + destReg] = retVal;
    }

    currentFrame = &callStack.back();
    currentBase = currentFrame->slotStart;

}

void MeowVM::opHalt() {
    callStack.clear();
}