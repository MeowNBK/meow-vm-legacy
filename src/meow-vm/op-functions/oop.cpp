#include "meow_vm.h"

void MeowVM::opNewClass() {
    auto proto = currentFrame->closure->proto;
    Int dst = currentInst->args[0], nameIdx = currentInst->args[1];
    if (nameIdx < 0 || nameIdx >= static_cast<Int>(proto->constantPool.size()) || !isString(proto->constantPool[nameIdx])) {
        throwVMError("NEW_CLASS name must be a string");
    }
    Str name = proto->constantPool[nameIdx].get<Str>();
    auto klass = memoryManager->newObject<ObjClass>(name);
    stackSlots[currentBase + dst] = Value(klass);
}

void MeowVM::opNewInstance() {
    Int dst = currentInst->args[0], classReg = currentInst->args[1];
    Value& clsVal = stackSlots[currentBase + classReg];
    if (!isClass(clsVal)) throwVMError("NEW_INSTANCE trên giá trị không phải class");
    auto klass = clsVal.get<Class>();
    auto instObj = memoryManager->newObject<ObjInstance>(klass);
    stackSlots[currentBase + dst] = Value(instObj);
}

void MeowVM::opGetProp() {
    auto proto = currentFrame->closure->proto;
    Int dst = currentInst->args[0], objReg = currentInst->args[1], nameIdx = currentInst->args[2];

    if (currentBase + objReg >= static_cast<Int>(stackSlots.size()) ||
        currentBase + dst >= static_cast<Int>(stackSlots.size()))
        throwVMError("GET_PROP register OOB");
    if (nameIdx < 0 || nameIdx >= static_cast<Int>(proto->constantPool.size()) || !isString(proto->constantPool[nameIdx]))
        throwVMError("Property name must be a string");

    Str name = proto->constantPool[nameIdx].get<Str>();
    Value& obj = stackSlots[currentBase + objReg];

    if (isInstance(obj)) {
        Instance inst = obj.get<Instance>();
        auto it = inst->fields.find(name);
        if (it != inst->fields.end()) {
            stackSlots[currentBase + dst] = it->second;
            return;
        }
    }

    if (auto prop = getMagicMethod(obj, name)) {
        stackSlots[currentBase + dst] = *prop;
        return;
    }

    stackSlots[currentBase + dst] = Value(Null{});
}


void MeowVM::opSetProp() {
    auto proto = currentFrame->closure->proto;
    Int objReg = currentInst->args[0], nameIdx = currentInst->args[1], valReg = currentInst->args[2];

    if (currentBase + objReg >= static_cast<Int>(stackSlots.size()) ||
        currentBase + valReg >= static_cast<Int>(stackSlots.size()))
        throwVMError("SET_PROP register OOB");
    if (nameIdx < 0 || nameIdx >= static_cast<Int>(proto->constantPool.size()) || !isString(proto->constantPool[nameIdx]))
        throwVMError("Property name must be a string");

    Str name = proto->constantPool[nameIdx].get<Str>();
    Value& obj = stackSlots[currentBase + objReg];
    Value& val = stackSlots[currentBase + valReg];


    if (auto mm = getMagicMethod(obj, "__setprop__")) {
        (void) call(*mm, { Value(name), val });
        return;
    }

    if (isInstance(obj)) {
        Instance inst = obj.get<Instance>();
        inst->fields[name] = val;
        return;
    }
    if (isMap(obj)) {
        Object m = obj.get<Object>();
        m->fields[name] = val;
        return;
    }
    if (isClass(obj)) {
        Class cls = obj.get<Class>();
        if (!isClosure(val) && !val.is<BoundMethod>()) throwVMError("Method must be closure");
        cls->methods[name] = val;
        return;
    }

    throwVMError("SET_PROP not supported on type '" + _toString(obj) + "'");
}


void MeowVM::opSetMethod() {
    auto proto = currentFrame->closure->proto;
    Int classReg = currentInst->args[0], 
        nameIdx = currentInst->args[1], 
        methodReg = currentInst->args[2];
    Value& klassVal = stackSlots[currentBase + classReg];
    if(!isClass(klassVal)) throwVMError("SET_METHOD chỉ cho class");
    if (nameIdx < 0 || nameIdx >= static_cast<Int>(proto->constantPool.size()) || !isString(proto->constantPool[nameIdx])) {
        throwVMError("Method name must be a string");
    }
    Str name = proto->constantPool[nameIdx].get<Str>();
    if(!isClosure(stackSlots[currentBase + methodReg])) 
        throwVMError("Method value must be a closure");
    klassVal.get<Class>()->methods[name] = stackSlots[currentBase + methodReg];
}

void MeowVM::opInherit() {
    Int subClassReg = currentInst->args[0], superClassReg = currentInst->args[1];
    Value& subClassVal = stackSlots[currentBase + subClassReg];
    Value& superClassVal = stackSlots[currentBase + superClassReg];
    if(!isClass(subClassVal) || !isClass(superClassVal)) throwVMError("Cả hai toán hạng cho kế thừa phải là class.");
    subClassVal.get<Class>()->superclass = superClassVal.get<Class>();
    auto& subMethods = subClassVal.get<Class>()->methods;
    auto& superMethods = superClassVal.get<Class>()->methods;
    for(const auto& pair : superMethods) {
        if(subMethods.find(pair.first) == subMethods.end()) {
            subMethods[pair.first] = pair.second;
        }
    }
}

void MeowVM::opGetSuper() {
    Int dst = currentInst->args[0];
    Int nameIdx = currentInst->args[1];

    auto proto = currentFrame->closure->proto;
    if (nameIdx < 0 || nameIdx >= static_cast<Int>(proto->constantPool.size()) || !isString(proto->constantPool[nameIdx])) {
        throwVMError("GET_SUPER name must be a string");
    }
    Str methodName = proto->constantPool[nameIdx].get<Str>();

    Value& receiverVal = stackSlots[currentBase + 0];
    if (!isInstance(receiverVal)) {
        throwVMError("`super` can only be used inside a method.");
    }
    Instance receiver = receiverVal.get<Instance>();

    if (!receiver->klass->superclass) {
        throwVMError("Class '" + receiver->klass->name + "' has no superclass.");
    }
    Class superclass = *receiver->klass->superclass;

    auto it = superclass->methods.find(methodName);
    if (it == superclass->methods.end()) {
        throwVMError("Superclass '" + superclass->name + "' has no method named '" + methodName + "'.");
    }
    Value& method = it->second;
    if (!isClosure(method)) {
        throwVMError("Superclass method is not a callable closure.");
    }
    
    auto bound = memoryManager->newObject<ObjBoundMethod>(receiver, method.get<Function>());
    stackSlots[currentBase + dst] = Value(bound);
}
