#include "meow_vm.h"

void MeowVM::opUnsupported() {
    throwVMError("Unsupported OpCode!");
}