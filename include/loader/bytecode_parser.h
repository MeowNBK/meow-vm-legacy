#pragma once
#include "definitions.h"

class MemoryManager;

class BytecodeParser {
public:
    std::unordered_map<Str, Proto> protos;
    BytecodeParser() = default;
    Bool parseFile(const Str& filepath, MemoryManager& mm);
    Bool parseSource(const Str& source, const Str& sourceName = "<string>");
private:
    MemoryManager* memoryManager = nullptr;
    Proto currentProto = nullptr;
    Bool parseLine(const Str& line, const Str& sourceName, Int lineNumber);
    Bool parseDirective(const std::vector<Str>& parts, const Str& sourceName, Int lineNumber);
    Value parseConstValue(const Str& token);
    std::vector<Str> split(const Str& s);
    void resolveAllLabels();
    void linkProtos();
};