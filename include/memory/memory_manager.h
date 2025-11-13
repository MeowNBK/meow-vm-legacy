#pragma once
#include "garbage_collector.h"
#include "pch.h"

class MeowVM;
class MemoryManager {
private:
    std::unique_ptr<GarbageCollector> gc;
    MeowVM* vm;

    size_t gcThreshold;
    size_t objectAllocated;
    bool gcDisabled = false;
public:
    MemoryManager(std::unique_ptr<GarbageCollector> gcImplement);

    template<typename T, typename... Args>
    T* newObject(Args&&... args) {
        if (objectAllocated >= gcThreshold && !gcDisabled) collect();
        T* newObj = new T(std::forward<Args>(args)...);
        gc->registerObject(static_cast<MeowObject*>(newObj));
        ++objectAllocated;
        return newObj;
    }

    inline void enableGC() noexcept {
        gcDisabled = false;
    }

    inline void disableGC() noexcept {
        gcDisabled = true;
    }

    inline void collect() {
        if (!vm) return;
        gc->collect(*vm);
        objectAllocated = 0;
    }

    void setVM(MeowVM* _vm) {
        vm = _vm;
    }
};