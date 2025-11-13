#include "memory_manager.h"

MemoryManager::MemoryManager(std::unique_ptr<GarbageCollector> gcImplement)
    : gc(std::move(gcImplement)), gcThreshold(1024), objectAllocated(0) {}