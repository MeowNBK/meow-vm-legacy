#pragma once

#include "value.h"
#include "memory_manager.h"

class MeowEngine {
public:
    virtual ~MeowEngine() = default;

    virtual Value call(const Value& callee, Arguments args) = 0;

    virtual MemoryManager* getMemoryManager() = 0;

    virtual void registerMethod(const Str& typeName, const Str& methodName, const Value& method) = 0;
    
    virtual void registerGetter(const Str& typeName, const Str& propName, const Value& getter) = 0;

    virtual const std::vector<Str>& getArguments() const = 0;
};