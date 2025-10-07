#pragma once

#include "garbage_collector.h"
#include "pch.h"

class MeowVM;

struct GCMetadata {
    bool isMarked = false;
};

class MarkSweepGC : public GarbageCollector, public GCVisitor {
private:
    std::unordered_map<MeowObject*, GCMetadata> metadata;
    MeowVM* vm = nullptr;

public:
    ~MarkSweepGC() override;

    void registerObject(MeowObject* obj) override;

    void collect(MeowVM& vmInstance) override;

    void visitValue(Value& value) override;

    void visitObject(MeowObject* obj) override;

private:
    void mark(MeowObject* obj);
};