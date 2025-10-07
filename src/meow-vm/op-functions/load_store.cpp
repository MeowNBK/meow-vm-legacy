#include "meow_vm.h"

void MeowVM::opMove() {
    Int dst = currentInst->args[0], src = currentInst->args[1];
    stackSlots[currentBase + dst] = stackSlots[currentBase + src];
}

void MeowVM::opLoadConst() {
    Int dst = currentInst->args[0], cidx = currentInst->args[1];
    auto proto = currentFrame->closure->proto;
    if (cidx < 0 || cidx >= static_cast<Int>(proto->constantPool.size())) {
        throwVMError("LOAD_CONST index OOB");
    }
    stackSlots[currentBase + dst] = proto->constantPool[cidx];
}

void MeowVM::opLoadInt() {
    Int dst = currentInst->args[0], val = currentInst->args[1];
    stackSlots[currentBase + dst] = Value(val);
}

void MeowVM::opLoadNull() {
    stackSlots[currentBase + currentInst->args[0]] = Value(Null{});
}

void MeowVM::opLoadTrue() {
    stackSlots[currentBase + currentInst->args[0]] = Value(true);
}

void MeowVM::opLoadFalse() {
    stackSlots[currentBase + currentInst->args[0]] = Value(false);
}

void MeowVM::opGetGlobal() {
    auto proto = currentFrame->closure->proto;
    Int dst = currentInst->args[0], constIdx = currentInst->args[1];
    if (constIdx < 0 || constIdx >= static_cast<Int>(proto->constantPool.size()))
        throwVMError("GET_GLOBAL index OOB");
    if (!proto->constantPool[constIdx].is<Str>())
        throwVMError("GET_GLOBAL name must be a string");
    auto name = proto->constantPool[constIdx].get<Str>();
    auto it = currentFrame->module->globals.find(name);
    if (it != currentFrame->module->globals.end()) {
        stackSlots[currentBase + dst] = it->second;
    } else {
        stackSlots[currentBase + dst] = Value(Null{});
    }
}

void MeowVM::opSetGlobal() {
    auto proto = currentFrame->closure->proto;
    Int constIdx = currentInst->args[0], src = currentInst->args[1];
    if (constIdx < 0 || constIdx >= static_cast<Int>(proto->constantPool.size())) 
        throwVMError("SET_GLOBAL index OOB với constIdx là: " + _toString(constIdx) + " vuợt quá giới hạn min = 0 và max = " + _toString(static_cast<Int>(proto->constantPool.size() - 1)));
    if (!proto->constantPool[constIdx].is<Str>()) {
        throwVMError("Global variable name must be a string");
    }
    auto name = proto->constantPool[constIdx].get<Str>();
    currentFrame->module->globals[name] = stackSlots[currentBase + src];
}

void MeowVM::opGetUpvalue() {
    Int dst = currentInst->args[0], uvIndex = currentInst->args[1];

    if (uvIndex < 0 || uvIndex >= static_cast<Int>(currentFrame->closure->upvalues.size()))
        throwVMError("GET_UPVALUE index OOB với uvIndex là: " + _toString(uvIndex) + " vuợt quá giới hạn min = 0 và max = " + _toString(static_cast<Int>(currentFrame->closure->upvalues.size() - 1)));

    auto uv = currentFrame->closure->upvalues[uvIndex];
    if (uv->state == ObjUpvalue::State::CLOSED) {
        stackSlots[currentBase + dst] = uv->closed;
    } else {
        stackSlots[currentBase + dst] = stackSlots[uv->slotIndex];
    }
}

void MeowVM::opSetUpvalue() {
    Int uvIndex = currentInst->args[0], src = currentInst->args[1];

    if (uvIndex < 0 || uvIndex >= static_cast<Int>(currentFrame->closure->upvalues.size()))
        throwVMError("SET_UPVALUE index OOB");

    auto uv = currentFrame->closure->upvalues[uvIndex];
    if (uv->state == ObjUpvalue::State::OPEN) {
        stackSlots[uv->slotIndex] = stackSlots[currentBase + src];
    } else {
        uv->closed = stackSlots[currentBase + src];
    }
}
