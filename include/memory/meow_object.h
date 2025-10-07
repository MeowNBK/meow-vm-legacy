#pragma once

class Value;
class MeowObject;

class GCVisitor {
public:
    virtual ~GCVisitor() = default;
    virtual void visitValue(Value& value) = 0;
    virtual void visitObject(MeowObject* obj) = 0;
};

class MeowObject {
public:
    virtual ~MeowObject() = default;
    
    virtual void trace(GCVisitor& visitor) = 0;
};