#include "meow_vm.h"

void MeowVM::opSetupTry() {
    Int target = currentInst->args[0];
    Int errorReg = (currentInst->args.size() > 1) ? currentInst->args[1] : -1;
    
    ExceptionHandler h(target, static_cast<Int>(callStack.size() - 1), static_cast<Int>(stackSlots.size()), errorReg);
    exceptionHandlers.push_back(h);
}

void MeowVM::opPopTry() {
    if (!exceptionHandlers.empty()) {
        exceptionHandlers.pop_back();
    }
}

void MeowVM::opThrow() {
    Int reg = currentInst->args[0];
    throw VMError(_toString(stackSlots[currentBase + reg]));
}