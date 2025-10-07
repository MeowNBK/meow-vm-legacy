#pragma once
#include "meow_object.h"

class MeowVM;

class GarbageCollector {
public:
    virtual ~GarbageCollector() = default;
    
    virtual void registerObject(MeowObject* obj) = 0;
    
    virtual void collect(MeowVM& vm) = 0;
};