#include "mark_sweep_gc.h"
#include "meow_vm.h"
#include "value.h"

MarkSweepGC::~MarkSweepGC() {
    for (auto const& [obj, data] : metadata) {

        delete obj;
    }
}

void MarkSweepGC::registerObject(MeowObject* obj) {

    metadata.emplace(obj, GCMetadata{});
}

void MarkSweepGC::collect(MeowVM& vmInstance) {
    this->vm = &vmInstance;

    // auto roots = vm->findRoots();
    // for (Value* root : roots) {
    //     visitValue(*root);
    // }

    vm->traceRoots(*this);

    for (auto it = metadata.begin(); it != metadata.end();) {
        MeowObject* obj = it->first;
        GCMetadata& data = it->second;

        if (data.isMarked) {
            data.isMarked = false;
            ++it;
        } else {

            delete obj;
            it = metadata.erase(it);
        }
    }
    
    this->vm = nullptr;
}

void MarkSweepGC::visitValue(Value& value) {
    if (value.is<Instance>())      
        mark(value.get<Instance>());
    else if (value.is<Function>()) 
        mark(value.get<Function>());
    else if (value.is<Class>())    
        mark(value.get<Class>());
    else if (value.is<Module>())   
        mark(value.get<Module>());
    else if (value.is<BoundMethod>())    
        mark(value.get<BoundMethod>());
    else if (value.is<Proto>())    
        mark(value.get<Proto>());
    else if (value.is<Upvalue>()) 
        mark(value.get<Upvalue>());
    else if (value.is<Array>()) {
        mark(value.get<Array>());
    } else if (value.is<Object>()) {
        mark(value.get<Object>());
    }
}

void MarkSweepGC::visitObject(MeowObject* obj) {
    mark(obj);
}

void MarkSweepGC::mark(MeowObject* obj) {
    if (obj == nullptr) {
        return;
    }

    auto it = metadata.find(obj);
    if (it == metadata.end()) {
        return;
    }

    if (it->second.isMarked) {
        return;
    }

    it->second.isMarked = true;

    obj->trace(*this);
}