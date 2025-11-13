#include "binary_parser.h"
#include "memory_manager.h"

Bool BinaryParser::parseFile(const Str& filepath, MemoryManager& mm) {
    this->memoryManager = &mm;
    fileStream.open(filepath, std::ios::binary);
    if (!fileStream.is_open()) {
        std::cerr << "Lỗi: Không thể mở file nhị phân: " << filepath << std::endl;
        return false;
    }
    protos.clear();

    try {
        Int numProtos = read<Int>();
        for (Int i = 0; i < numProtos; ++i) {
            Str protoName = readString();
            parseProto(protoName);
        }
        linkProtos();
    } catch (const std::exception& e) {
        std::cerr << "Lỗi đọc file nhị phân: " << e.what() << std::endl;
        fileStream.close();
        return false;
    }

    fileStream.close();
    this->memoryManager = nullptr;
    return true;
}

template<typename T>
T BinaryParser::read() {
    T value;
    fileStream.read(reinterpret_cast<char*>(&value), sizeof(T));
    if (fileStream.gcount() != sizeof(T)) {
        throw std::runtime_error("Lỗi đọc file, file không đúng định dạng hoặc kết thúc đột ngột.");
    }
    return value;
}

Str BinaryParser::readString() {
    Int size = read<Int>();
    std::vector<char> buffer(size);
    fileStream.read(buffer.data(), size);
    if (fileStream.gcount() != size) {
        throw std::runtime_error("Lỗi đọc chuỗi, file không đúng định dạng hoặc kết thúc đột ngột.");
    }
    return Str(buffer.begin(), buffer.end());
}

void BinaryParser::parseProto(const Str& sourceName) {

    auto proto = memoryManager->newObject<ObjFunctionProto>();
    proto->sourceName = sourceName;
    proto->numRegisters = read<Int>();
    proto->numUpvalues = read<Int>();

    Int numConstants = read<Int>();
    proto->constantPool.reserve(numConstants);
    for (Int i = 0; i < numConstants; ++i) {
        Int type = read<Int>();
        if (type == 0) proto->constantPool.push_back(Value(Null{}));
        else if (type == 1) proto->constantPool.push_back(Value(read<Int>()));
        else if (type == 2) proto->constantPool.push_back(Value(read<Real>()));
        else if (type == 3) proto->constantPool.push_back(Value(read<Bool>()));
        else if (type == 4) proto->constantPool.push_back(Value(readString()));
        else if (type == 5) {
            Str protoName = readString();
            proto->constantPool.push_back(Value(protoName));
        } else {
            throw std::runtime_error("Kiểu hằng số không hợp lệ.");
        }
    }

    Int numUpvalues = read<Int>();
    proto->upvalueDescs.reserve(numUpvalues);
    for (Int i = 0; i < numUpvalues; ++i) {
        Bool isLocal = read<Bool>();
        Int index = read<Int>();
        proto->upvalueDescs.emplace_back(isLocal, index);
    }

    Int numInstructions = read<Int>();
    proto->code.reserve(numInstructions);
    for (Int i = 0; i < numInstructions; ++i) {
        Int opcode = read<Int>();
        Int numArgs = read<Int>();
        std::vector<Int> args(numArgs);
        for (Int j = 0; j < numArgs; ++j) {
            args[j] = read<Int>();
        }
        proto->code.emplace_back(static_cast<OpCode>(opcode), args);
    }
    protos[sourceName] = proto;
}

void BinaryParser::linkProtos() {
    for (auto const& [name, proto] : protos) {
        for (Value& v : proto->constantPool) {
            if (v.is<Str>() && v.get<Str>().rfind("@", 0) == 0) {
                auto linkedProto = protos.at(v.get<Str>());
                v = Value(linkedProto);
            }
        }
    }
}