#pragma once
#include "definitions.h"
#include "pch.h"

class MemoryManager;

class BinaryParser {
public:
    std::unordered_map<Str, Proto> protos;
    BinaryParser() = default;
    Bool parseFile(const Str& filepath, MemoryManager& mm);
private:
    MemoryManager* memoryManager;
    std::ifstream fileStream;

    template<typename T>
    T read();
    Str readString();

    void parseProto(const Str& sourceName);
    void linkProtos();
};