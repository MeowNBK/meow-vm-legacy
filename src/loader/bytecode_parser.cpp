#include "bytecode_parser.h"
#include "memory_manager.h"
#include "pch.h"

static Str trim(const Str& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == Str::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static Str toUpper(const Str& s) {
    Str res = s;
    std::transform(res.begin(), res.end(), res.begin(), ::toupper);
    return res;
}

static inline void skipComment(Str& line) {
    bool in_string = false;
    size_t comment_pos = Str::npos;

    for (size_t i = 0; i < line.length(); ++i) {
        if (line[i] == '"') {
            if (i == 0 || line[i-1] != '\\') {
                in_string = !in_string;
            }
        }
        
        if (line[i] == '#' && !in_string) {
            comment_pos = i;
            break;
        }
    }

    if (comment_pos != Str::npos) {
        line = line.substr(0, comment_pos);
    }
    line = trim(line);
}


Bool BytecodeParser::parseFile(const Str& filepath, MemoryManager& mm) {
    this->memoryManager = &mm;
    std::ifstream ifs(filepath);
    if (!ifs) {
        std::cerr << "Error: Cannot open file: " << filepath << std::endl;
        return false;
    }
    std::ostringstream ss;
    ss << ifs.rdbuf();
    Bool res = parseSource(ss.str(), filepath);
    this->memoryManager = nullptr;
    return res;
}

std::vector<Str> BytecodeParser::split(const Str& s) {
    std::istringstream iss(s);
    std::vector<Str> out;
    Str tok;
    while (std::getline(iss, tok)) {
        std::istringstream iss2(tok);
        Str sub_tok;
        while (iss2 >> sub_tok) {
            out.push_back(sub_tok);
        }
    }
    return out;
}

Bool BytecodeParser::parseSource(const Str& source, const Str& sourceName) {
    protos.clear();
    currentProto = nullptr;
    std::istringstream iss(source);
    Str line;
    Int lineno = 0;
    while (std::getline(iss, line)) {
        ++lineno;
        skipComment(line);
        if (line.empty()) continue;
        try {
            if (!parseLine(line, sourceName, lineno)) return false;
        } catch (const std::exception& e) {
            std::cerr << "Semantic error in '" << sourceName << "' at line " << lineno << ": " << e.what() << std::endl;
            return false;
        }
    }
    if (currentProto) {
        std::cerr << "Error in '" << sourceName << "': file ended but missed '.endfunc'" << std::endl;
        return false;
    }
    try {
        resolveAllLabels();
        linkProtos();
    } catch (const std::exception& e) {
        std::cerr << "Lỗi liên kết/nhãn: " << e.what() << std::endl;
        return false;
    }
    return true;
}

Bool BytecodeParser::parseLine(const Str& line, const Str& sourceName, Int lineNumber) {
    if (!line.empty() && line.back() == ':') {
        if (!currentProto) throw std::runtime_error("Nhãn phải nằm trong một khối .func.");
        Str label = line.substr(0, line.size() - 1);
        if (currentProto->labels.count(label)) throw std::runtime_error("Nhãn '" + label + "' đã được định nghĩa.");
        currentProto->labels[label] = currentProto->code.size();
        return true;
    }

    auto parts = split(line);
    if (parts.empty()) return true;

    if (parts[0].size() > 0 && parts[0][0] == '.') {
        return parseDirective(parts, sourceName, lineNumber);
    }
    if (!currentProto) throw std::runtime_error("Lệnh phải nằm trong một khối .func.");

    static const std::unordered_map<Str, OpCode> OPC = {
        {"LOAD_CONST", OpCode::LOAD_CONST}, {"LOAD_NULL", OpCode::LOAD_NULL}, {"LOAD_TRUE", OpCode::LOAD_TRUE},
        {"LOAD_FALSE", OpCode::LOAD_FALSE}, {"LOAD_INT", OpCode::LOAD_INT}, {"MOVE", OpCode::MOVE},
        {"ADD", OpCode::ADD}, {"SUB", OpCode::SUB}, {"MUL", OpCode::MUL}, {"DIV", OpCode::DIV},
        {"MOD", OpCode::MOD}, {"POW", OpCode::POW}, {"EQ", OpCode::EQ}, {"NEQ", OpCode::NEQ},
        {"GT", OpCode::GT}, {"GE", OpCode::GE}, {"LT", OpCode::LT}, {"LE", OpCode::LE},
        {"NEG", OpCode::NEG}, {"NOT", OpCode::NOT}, {"GET_GLOBAL", OpCode::GET_GLOBAL},
        {"SET_GLOBAL", OpCode::SET_GLOBAL}, {"GET_UPVALUE", OpCode::GET_UPVALUE}, {"SET_UPVALUE", OpCode::SET_UPVALUE},
        {"CLOSURE", OpCode::CLOSURE}, {"CLOSE_UPVALUES", OpCode::CLOSE_UPVALUES}, {"JUMP", OpCode::JUMP},
        {"JUMP_IF_FALSE", OpCode::JUMP_IF_FALSE}, {"JUMP_IF_TRUE", OpCode::JUMP_IF_TRUE}, {"CALL", OpCode::CALL}, {"RETURN", OpCode::RETURN},
        {"HALT", OpCode::HALT}, {"NEW_ARRAY", OpCode::NEW_ARRAY}, {"NEW_HASH", OpCode::NEW_HASH},
        {"GET_INDEX", OpCode::GET_INDEX}, {"SET_INDEX", OpCode::SET_INDEX}, {"GET_KEYS", OpCode::GET_KEYS}, {"GET_VALUES", OpCode::GET_VALUES}, {"NEW_CLASS", OpCode::NEW_CLASS},
        {"NEW_INSTANCE", OpCode::NEW_INSTANCE}, {"GET_PROP", OpCode::GET_PROP}, {"SET_PROP", OpCode::SET_PROP},
        {"SET_METHOD", OpCode::SET_METHOD}, {"INHERIT", OpCode::INHERIT}, {"GET_SUPER", OpCode::GET_SUPER}, {"BIT_AND", OpCode::BIT_AND},
        {"BIT_OR", OpCode::BIT_OR}, {"BIT_XOR", OpCode::BIT_XOR}, {"BIT_NOT", OpCode::BIT_NOT},
        {"LSHIFT", OpCode::LSHIFT}, {"RSHIFT", OpCode::RSHIFT}, {"THROW", OpCode::THROW},
        {"SETUP_TRY", OpCode::SETUP_TRY}, {"POP_TRY", OpCode::POP_TRY}, {"IMPORT_MODULE", OpCode::IMPORT_MODULE},
        {"EXPORT", OpCode::EXPORT}, {"GET_EXPORT", OpCode::GET_EXPORT}, {"GET_MODULE_EXPORT", OpCode::GET_MODULE_EXPORT}, {"IMPORT_ALL", OpCode::IMPORT_ALL}
    };
    Str upper_cmd = toUpper(parts[0]);
    auto it = OPC.find(upper_cmd);
    if (it == OPC.end()) throw std::runtime_error("Invalid opcode: '" + parts[0] + "'");
    OpCode op = it->second;
    std::vector<Int> args;
    Int instIndex = currentProto->code.size();
    if (op == OpCode::JUMP || op == OpCode::SETUP_TRY) {
        if (parts.size() < 2) throw std::runtime_error("'" + parts[0] + "' command needs a label or IP index.");
        try {
            args.push_back(std::stoi(parts[1]));
        } catch (...) {
            currentProto->pendingJumps.emplace_back(instIndex, 0, parts[1]);
            args.push_back(0);
        }
    } else if (op == OpCode::JUMP_IF_FALSE || op == OpCode::JUMP_IF_TRUE) {
        if (parts.size() < 3) throw std::runtime_error("Lệnh '" + parts[0] + "' need 2 arguments: register and label/IP.");
        
        args.push_back(std::stoi(parts[1]));
        
        try {
            args.push_back(std::stoi(parts[2]));
        } catch (...) {
            currentProto->pendingJumps.emplace_back(instIndex, 1, parts[2]);
            args.push_back(0);
        }
    } else {
        for (size_t i = 1; i < parts.size(); ++i) {
            try {
                args.push_back(std::stoi(parts[i]));
            } catch (...) {
                throw std::runtime_error("Invalid argument for '" + parts[0] + "' command. Make sure all arguments are integers.");
            }
        }
    }
    currentProto->code.emplace_back(op, args);
    return true;
}

Bool BytecodeParser::parseDirective(const std::vector<Str>& parts, [[maybe_unused]] const Str& sourceName, [[maybe_unused]] Int lineNumber) {
    const Str& cmd = parts[0];
    if (cmd == ".func") {
        if (currentProto) throw std::runtime_error("Không thể bắt đầu .func mới trong một .func khác.");
        if (parts.size() < 2) throw std::runtime_error(".func yêu cầu tên hàm.");

        currentProto = memoryManager->newObject<ObjFunctionProto>(0, 0, parts[1]);
        protos[parts[1]] = currentProto;
    } else if (cmd == ".endfunc") {
        if (!currentProto) throw std::runtime_error("Cannot find any .endfunc corresponding to .func.");
        currentProto = nullptr;
    } else {
        if (!currentProto) throw std::runtime_error("'" + cmd + "' directive must be inside a .func block.");
        if (cmd == ".registers") {
            if (parts.size() < 2) throw std::runtime_error(".registers cần 1 tham số.");
            currentProto->numRegisters = std::stoi(parts[1]);
        } else if (cmd == ".upvalues") {
            if (parts.size() < 2) throw std::runtime_error(".upvalues needs 1 parameter.");
            currentProto->numUpvalues = std::stoi(parts[1]);
        } 
        else if (cmd == ".const") {
            if (parts.size() < 2) throw std::runtime_error(".const is missing parameter.");
            Str rest;
            for (size_t i = 1; i < parts.size(); ++i) {
                if (i > 1) rest += " ";
                rest += parts[i];
            }
            currentProto->constantPool.push_back(parseConstValue(rest));
        } else if (cmd == ".upvalue") {
            if (parts.size() < 4) throw std::runtime_error(".upvalue yêu cầu 3 đối số.");
            Int uvIndex = std::stoi(parts[1]);
            Str uvType = parts[2];
            Int slot = std::stoi(parts[3]);
            Bool isLocal = (uvType == "local");
            if (uvType != "local" && uvType != "parent_upvalue") throw std::runtime_error("Loại upvalue không hợp lệ: '" + uvType + "'.");
            if (currentProto->upvalueDescs.size() <= static_cast<size_t>(uvIndex)) {
                currentProto->upvalueDescs.resize(uvIndex + 1);
            }
            currentProto->upvalueDescs[uvIndex] = UpvalueDesc(isLocal, slot);
        } else {
            throw std::runtime_error("Chỉ thị không nhận dạng được: '" + cmd + "'");
        }
    }
    return true;
}

Str unescapeString(const Str& s) {
    Str result;
    result.reserve(s.length());
    bool escaping = false;

    for (char c : s) {
        if (escaping) {
            switch (c) {
                case 'n':  result += '\n'; break;
                case 't':  result += '\t'; break;
                case 'r':  result += '\r'; break;
                case '"':  result += '\"'; break;
                case '\\': result += '\\'; break;
                default:
                    result += '\\';
                    result += c;
                    break;
            }
            escaping = false;
        } else if (c == '\\') {
            escaping = true;
        } else {
            result += c;
        }
    }
    return result;
}

Value BytecodeParser::parseConstValue(const Str& token) {
    Str s = trim(token);
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        Str inner = s.substr(1, s.size() - 2);
        return Value(unescapeString(inner));
    }
    if (!s.empty() && s.front() == '@') {
        return Value("::function_proto::" + s);
    }
    if (s.find('.') != Str::npos) {
        try { return Value(std::stod(s)); } catch (...) {}
    }
    try { return Value(std::stoi(s)); } catch (...) {}
    if (s == "true") return Value(true);
    if (s == "false") return Value(false);
    if (s == "null") return Value(Null{});
    throw std::runtime_error("Hằng số không hợp lệ: '" + s + "'");
}

void BytecodeParser::resolveAllLabels() {
    for (auto& pair : protos) {
        auto proto = pair.second;
        for (const auto& jump : proto->pendingJumps) {
            Int instIdx = std::get<0>(jump);
            Int argIdx = std::get<1>(jump);
            Str labelName = std::get<2>(jump);
            auto it = proto->labels.find(labelName);
            if (it == proto->labels.end()) {
                throw std::runtime_error("Không tìm thấy nhãn '" + labelName + "' trong hàm '" + proto->sourceName + "'");
            }
            proto->code[instIdx].args[argIdx] = it->second;
        }
        proto->pendingJumps.clear();
    }
}

void BytecodeParser::linkProtos() {
    const Str& prefix = "::function_proto::";
    for (auto& pair : protos) {
        auto proto = pair.second;
        for (size_t i = 0; i < proto->constantPool.size(); ++i) {
            if (proto->constantPool[i].is<Str>()) {
                auto s = proto->constantPool[i].get<Str>();
                if (s.rfind(prefix, 0) == 0) {
                    Str protoName = s.substr(prefix.length());
                    auto it = protos.find(protoName);
                    
                    if (it != protos.end()) {
                        proto->constantPool[i] = Value(it->second);
                    }
                }
            }
        }
    }
}