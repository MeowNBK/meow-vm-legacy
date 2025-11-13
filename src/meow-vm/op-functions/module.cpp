#include "meow_vm.h"

void MeowVM::opImportModule() {
    auto proto = currentFrame->closure->proto;
    Int dst = currentInst->args[0];
    Int pathIdx = currentInst->args[1];

    if (pathIdx < 0 || pathIdx >= static_cast<Int>(proto->constantPool.size()))
        throwVMError("IMPORT_MODULE index OOB");
    if (!isString(proto->constantPool[pathIdx]))
        throwVMError("IMPORT_MODULE path must be a string");

    Str importPath = proto->constantPool[pathIdx].get<Str>();
    Bool importerBinary = currentFrame->module->isBinary;

    auto mod = _getOrLoadModule(importPath, currentFrame->module->path, importerBinary);

    stackSlots[currentBase + dst] = Value(mod);

    if (mod->hasMain && !mod->isExecuted) {
        if (!mod->isExecuting) {
            mod->isExecuting = true;

            auto moduleClosure = memoryManager->newObject<ObjClosure>(mod->mainProto);
            Int newStart = static_cast<Int>(stackSlots.size());
            stackSlots.resize(newStart + mod->mainProto->numRegisters, Value(Null{}));

            CallFrame newFrame(moduleClosure, newStart, mod, 0, -1);
            callStack.push_back(newFrame);

            mod->isExecuted = true; 
        }
    }
}


void MeowVM::opExport() {
    auto proto = currentFrame->closure->proto;
    Int nameIdx = currentInst->args[0], srcReg = currentInst->args[1];
    if (nameIdx < 0 || nameIdx >= static_cast<Int>(proto->constantPool.size())) 
        throwVMError("EXPORT index OOB");
    if (!isString(proto->constantPool[nameIdx])) 
        throwVMError("EXPORT name must be a string");
    Str exportName = proto->constantPool[nameIdx].get<Str>();
    currentFrame->module->exports[exportName] = stackSlots[currentBase + srcReg];
}

void MeowVM::opGetExport() {
    auto proto = currentFrame->closure->proto;
    Int dst = currentInst->args[0],
        moduleReg = currentInst->args[1], 
        nameIdx = currentInst->args[2];
    if (currentBase + moduleReg >= static_cast<Int>(stackSlots.size())) 
        throwVMError("GET_EXPORT module register OOB");
    Value& moduleVal = stackSlots[currentBase + moduleReg];
    if (!isModule(moduleVal)) 
        throwVMError("Chỉ có thể lấy export từ một đối tượng module: " + _toString(moduleVal));
    if (nameIdx < 0 || nameIdx >= static_cast<Int>(proto->constantPool.size())) 
        throwVMError("GET_EXPORT index OOB");
    if (!isString(proto->constantPool[nameIdx])) 
        throwVMError("Export name must be a string");
    Str exportName = proto->constantPool[nameIdx].get<Str>();
    auto mod = moduleVal.get<Module>();
    auto it = mod->exports.find(exportName);
    if (it == mod->exports.end()) 
        throwVMError("Module '" + mod->name + "' không có export nào tên là '" + exportName + "'.");
    stackSlots[currentBase + dst] = it->second;
}

void MeowVM::opGetModuleExport() {
    auto proto = currentFrame->closure->proto;
    Int dst = currentInst->args[0];
    Int moduleReg = currentInst->args[1];
    Int nameIdx = currentInst->args[2];

    if (currentBase + moduleReg >= static_cast<Int>(stackSlots.size()))
        throwVMError("GET_MODULE_EXPORT module register OOB");

    Value& moduleVal = stackSlots[currentBase + moduleReg];
    if (!isModule(moduleVal))
        throwVMError("GET_MODULE_EXPORT chỉ dùng với module.");

    if (nameIdx < 0 || nameIdx >= static_cast<Int>(proto->constantPool.size()) || !isString(proto->constantPool[nameIdx]))
        throwVMError("Export name phải là string hợp lệ");

    Str exportName = proto->constantPool[nameIdx].get<Str>();
    auto mod = moduleVal.get<Module>();

    auto it = mod->exports.find(exportName);
    if (it == mod->exports.end())
        throwVMError("Module '" + mod->name + "' không có export '" + exportName + "'.");

    stackSlots[currentBase + dst] = it->second;
}

void MeowVM::opImportAll() {
    Int moduleReg = currentInst->args[0]; 

    if (currentBase + moduleReg >= static_cast<Int>(stackSlots.size())) {
        throwVMError("IMPORT_ALL register OOB");
    }

    Value& moduleVal = stackSlots[currentBase + moduleReg];
    if (!isModule(moduleVal)) {
        throwVMError("IMPORT_ALL chỉ có thể dùng với một đối tượng module.");
    }

    auto importedModule = moduleVal.get<Module>();
    auto currentModule = currentFrame->module;

    for (const auto& pair : importedModule->exports) {
        currentModule->globals[pair.first] = pair.second;
    }
}