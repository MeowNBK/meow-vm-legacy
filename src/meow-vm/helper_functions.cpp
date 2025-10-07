#include "meow_vm.h"
#include "pch.h"


Bool MeowVM::isNull(const Value& v) const            { return v.is<Null>(); }
Bool MeowVM::isBool(const Value& v) const            { return v.is<Bool>(); }
Bool MeowVM::isDouble(const Value& v) const          { return v.is<Real>(); }
Bool MeowVM::isInt(const Value& v) const             { return v.is<Int>(); }
Bool MeowVM::isString(const Value& v) const          { return v.is<Str>(); }
Bool MeowVM::isVector(const Value& v) const          { return v.is<Array>(); }
Bool MeowVM::isMap(const Value& v) const             { return v.is<Object>(); }
Bool MeowVM::isInstance(const Value& v) const        { return v.is<Instance>(); }
Bool MeowVM::isClass(const Value& v) const           { return v.is<Class>(); }
Bool MeowVM::isClosure(const Value& v) const         { return v.is<Function>(); }
Bool MeowVM::isFunctionProto(const Value& v) const   { return v.is<Proto>(); }
Bool MeowVM::isModule(const Value& v) const          { return v.is<Module>(); }
Bool MeowVM::isBound(const Value& v) const           { return v.is<BoundMethod>(); }
Bool MeowVM::isNative(const Value& v) const          { return v.is<NativeFn>(); }


static Str trimTrailingZeros(const Str& s) {

    auto pos = s.find('.');
    if (pos == Str::npos) return s;
    size_t end = s.size();
    while (end > pos + 1 && s[end - 1] == '0') --end;
    if (end == pos + 1) --pos;
    return s.substr(0, end);
}

Str MeowVM::_toString(const Value& v) {
    if (v.is<Null>()) return "null";
    if (v.is<Bool>()) return v.get<Bool>() ? "true" : "false";
    if (v.is<Int>()) return std::to_string(v.get<Int>());
    if (v.is<Real>()) {
        Real val = v.get<Real>();
        if (std::isnan(val)) return "NaN";
        if (std::isinf(val)) return (val > 0) ? "Infinity" : "-Infinity";

        if (val == 0.0 && std::signbit(val)) return "-0";
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(15) << val;
        return trimTrailingZeros(ss.str());
    }
    if (v.is<Str>()) return v.get<Str>();
    if (v.is<Instance>()) {
        const auto& inst = v.get<Instance>();
        auto it = inst->fields.find("__str__");
        if (it != inst->fields.end()) {

            try {
                Function func = it->second.get<Function>();
                BoundMethod bound = memoryManager->newObject<ObjBoundMethod>(inst, func);
                Value str = this->call(Value(bound), {});
                if (str.is<Str>()) return str.get<Str>();

            } catch (...) {

            }
        } else {

            auto currentClass = inst->klass;
            while (currentClass) {
                auto mIt = currentClass->methods.find("__str__");
                if (mIt != currentClass->methods.end()) {

                    try {
                        Function func = mIt->second.get<Function>();
                        BoundMethod bound = memoryManager->newObject<ObjBoundMethod>(inst, func);
                        Value str = this->call(Value(bound), {});
                        if (str.is<Str>()) return str.get<Str>();
                    } catch (...) {

                    }
                    break;
                }
                if (currentClass->superclass) {
                    currentClass = *currentClass->superclass;
                } else {
                    break;
                }
            }
        }
        return "<" + inst->klass->name + " object>";
    }
    if (v.is<Class>()) return "<class '" + v.get<Class>()->name + "'>";
    if (v.is<Function>()) return "<fn '" + v.get<Function>()->proto->sourceName + "'>";
    if (v.is<BoundMethod>()) return "<bound method>";
    if (v.is<Array>()) {
        const auto& vec = v.get<Array>()->elements;
        Str out = "[";
        for (size_t i = 0; i < vec.size(); ++i) {
            if (i > 0) out += ", ";
            out += _toString(vec[i]);
        }
        out += "]";
        return out;
    }
    if (v.is<Object>()) {
        const auto& m = v.get<Object>()->fields;
        Str out = "{";
        Bool first = true;
        for (const auto& pair : m) {
            if (!first) out += ", ";
            out += pair.first + ": " + _toString(pair.second);
            first = false;
        }
        out += "}";
        return out;
    }
    if (v.is<Module>()) return "<module '" + v.get<Module>()->name + "'>";
    if (v.is<NativeFn>()) return "<native fn>";
        if (v.is<Proto>()) {
        Proto proto = v.get<Proto>();
        if (!proto) return "<null proto>";
        std::ostringstream os;

        os << "<function proto '" << proto->sourceName << "'>\n";
        os << "  - code size: " << proto->code.size() << "\n";

        if (!proto->code.empty()) {
            // compute op name width for alignment (similar to throwVMError)
            int maxOpLen = 0;
            for (size_t i = 0; i < proto->code.size(); ++i) {
                int len = static_cast<int>(opToString(proto->code[i].op).size());
                if (len > maxOpLen) maxOpLen = len;
            }
            int opField = std::max(10, maxOpLen + 2);

            os << "  - Bytecode:\n";
            std::ios::fmtflags savedFlags = os.flags();
            for (size_t i = 0; i < proto->code.size(); ++i) {
                const Instruction& inst = proto->code[i];
                os << "     " << std::right << std::setw(4) << static_cast<Int>(i) << ": ";
                os << std::left << std::setw(opField) << opToString(inst.op);
                if (!inst.args.empty()) {
                    os << "  args=[";
                    for (size_t a = 0; a < inst.args.size(); ++a) {
                        if (a) os << ", ";
                        os << inst.args[a];
                    }
                    os << "]";
                } else {
                    os << "  args=[]";
                }
                os << "\n";
                os.flags(savedFlags);
            }
        } else {
            os << "  - (Bytecode rỗng)\n";
        }

        // valueToString safe (giống throwVMError): không đệ quy in proto/closure/instance...
        auto valueToString = [&](const Value& val) -> Str {
            if (val.is<Null>()) return "<null>";
            if (val.is<Int>()) return std::to_string(val.get<Int>());
            if (val.is<Real>()) {
                std::ostringstream t;
                Real r = val.get<Real>();
                if (std::isnan(r)) return "NaN";
                if (std::isinf(r)) return (r > 0) ? "Infinity" : "-Infinity";
                t << r;
                return t.str();
            }
            if (val.is<Bool>()) return val.get<Bool>() ? "true" : "false";
            if (val.is<Str>()) return Str("\"") + val.get<Str>() + Str("\"");
            if (val.is<Proto>()) return "<function proto>";
            if (val.is<Function>()) return "<closure>";
            if (val.is<Instance>()) return "<instance>";
            if (val.is<Class>()) return "<class>";
            if (val.is<Array>()) return "<array>";
            if (val.is<Object>()) return "<object>";
            if (val.is<Upvalue>()) return "<upvalue>";
            if (val.is<Module>()) return "<module>";
            if (val.is<BoundMethod>()) return "<bound method>";
            if (val.is<NativeFn>()) return "<native fn>";
            return "<unknown value>";
        };

        if (!proto->constantPool.empty()) {
            os << "\n  - Constant pool (preview up to 10):\n";
            size_t maxShow = std::min<size_t>(proto->constantPool.size(), 10);
            for (size_t ci = 0; ci < maxShow; ++ci) {
                os << "     [" << ci << "]: " << valueToString(proto->constantPool[ci]) << "\n";
            }
        }

        return os.str();
    }
    if (v.is<Upvalue>()) return "upvalue";
    return "<unknown_type>";
}

Int MeowVM::_toInt(const Value& v) const {
    if (v.is<Int>()) return v.get<Int>();
    if (v.is<Real>()) {
        Real r = v.get<Real>();
        if (std::isinf(r)) return (r > 0) ? std::numeric_limits<Int>::max() : std::numeric_limits<Int>::min();
        if (std::isnan(r)) return 0;
        return static_cast<Int>(r);
    }
    if (v.is<Bool>()) return v.get<Bool>() ? 1 : 0;
    if (v.is<Str>()) {
        const Str sfull = v.get<Str>();

        size_t left = 0;
        while (left < sfull.size() && std::isspace(static_cast<unsigned char>(sfull[left]))) ++left;
        size_t right = sfull.size();
        while (right > left && std::isspace(static_cast<unsigned char>(sfull[right - 1]))) --right;
        if (left >= right) return 0;


        Str token = sfull.substr(left, right - left);

        bool neg = false;
        size_t pos = 0;
        if (token[pos] == '+' || token[pos] == '-') {
            neg = (token[pos] == '-');
            ++pos;
            if (pos >= token.size()) return 0;
        }


        if (token.size() - pos >= 2 && token[pos] == '0' && (token[pos+1] == 'b' || token[pos+1] == 'B')) {

            unsigned long long acc = 0;
            const unsigned long long limit = static_cast<unsigned long long>(std::numeric_limits<Int>::max());
            for (size_t i = pos + 2; i < token.size(); ++i) {
                char c = token[i];
                if (c == '0' || c == '1') {
                    int d = c - '0';
                    if (acc > (limit - d) / 2) {
                        return neg ? std::numeric_limits<Int>::min() : std::numeric_limits<Int>::max();
                    }
                    acc = (acc << 1) | static_cast<unsigned long long>(d);
                } else break;
            }
            Int result = static_cast<Int>(acc);
            return neg ? -result : result;
        }


        int base = 10;
        if (token.size() - pos >= 2 && token[pos] == '0' && (token[pos+1] == 'x' || token[pos+1] == 'X')) {
            base = 16;
        } else if (token.size() - pos >= 2 && token[pos] == '0' && (token[pos+1] == 'o' || token[pos+1] == 'O')) {
            base = 8;
        } else if (token.size() - pos >= 2 && token[pos] == '0' && std::isdigit(static_cast<unsigned char>(token[pos+1]))) {

            base = 8;
        }


        errno = 0;
        char* endptr = nullptr;
        const std::string tokstd(token.begin(), token.end());
        long long val = std::strtoll(tokstd.c_str(), &endptr, base);
        if (endptr == tokstd.c_str()) return 0;
        if (errno == ERANGE) {
            return (val > 0) ? std::numeric_limits<Int>::max() : std::numeric_limits<Int>::min();
        }

        if (val > static_cast<long long>(std::numeric_limits<Int>::max())) return std::numeric_limits<Int>::max();
        if (val < static_cast<long long>(std::numeric_limits<Int>::min())) return std::numeric_limits<Int>::min();
        return static_cast<Int>(val);
    }
    return 0;
}


Real MeowVM::_toDouble(const Value& v) const {
    if (v.is<Real>()) return v.get<Real>();
    if (v.is<Int>()) return static_cast<Real>(v.get<Int>());
    if (v.is<Bool>()) return v.get<Bool>() ? 1.0 : 0.0;
    if (v.is<Str>()) {
        Str s = v.get<Str>();

        for (auto &c : s) c = static_cast<char>(std::tolower((unsigned char)c));
        if (s == "nan") return std::numeric_limits<Real>::quiet_NaN();
        if (s == "infinity" || s == "+infinity" || s == "inf" || s == "+inf") return std::numeric_limits<Real>::infinity();
        if (s == "-infinity" || s == "-inf") return -std::numeric_limits<Real>::infinity();
        const char* cs = v.get<Str>().c_str();
        errno = 0;
        char* endptr = nullptr;
        double val = std::strtod(cs, &endptr);
        if (cs == endptr) return 0.0;
        if (errno == ERANGE) {
            return (val > 0) ? std::numeric_limits<Real>::infinity() : -std::numeric_limits<Real>::infinity();
        }
        return static_cast<Real>(val);
    }
    return 0.0;
}

Bool MeowVM::_isTruthy(const Value& v) const {
    if (v.is<Null>()) return false;
    if (v.is<Bool>()) return v.get<Bool>();
    if (v.is<Int>()) return v.get<Int>() != 0;
    if (v.is<Real>()) {
        Real r = v.get<Real>();
        return r != 0.0 && !std::isnan(r);
    }
    if (v.is<Str>()) return !v.get<Str>().empty();
    if (v.is<Array>()) return !v.get<Array>()->elements.empty();
    if (v.is<Object>()) return !v.get<Object>()->fields.empty();
    return true;
}

Bool MeowVM::_areValuesEqual(const Value& a, const Value& b) const {

    if (a.index() == b.index()) {
        if (a.is<Null>()) return true;
        if (a.is<Bool>()) return a.get<Bool>() == b.get<Bool>();
        if (a.is<Int>()) return a.get<Int>() == b.get<Int>();
        if (a.is<Real>()) {
            Real ra = a.get<Real>(), rb = b.get<Real>();
            if (std::isnan(ra) && std::isnan(rb)) return false;
            return ra == rb;
        }
        if (a.is<Str>()) return a.get<Str>() == b.get<Str>();
        return false;
    }


    if ((isDouble(a) && isInt(b)) || (isInt(a) && isDouble(b))) {
        return _toDouble(a) == _toDouble(b);
    }


    if ((isString(a) && (isDouble(b) || isInt(b))) || (isString(b) && (isDouble(a) || isInt(a)))) {

        return _toDouble(a) == _toDouble(b);
    }

    return false;
}

Str MeowVM::opToString(OpCode op) const {
    switch (op) {
        case OpCode::LOAD_CONST: return "LOAD_CONST";
        case OpCode::LOAD_NULL: return "LOAD_NULL";
        case OpCode::LOAD_TRUE: return "LOAD_TRUE";
        case OpCode::LOAD_FALSE: return "LOAD_FALSE";
        case OpCode::LOAD_INT: return "LOAD_INT";
        case OpCode::MOVE: return "MOVE";
        case OpCode::ADD: return "ADD";
        case OpCode::SUB: return "SUB";
        case OpCode::MUL: return "MUL";
        case OpCode::DIV: return "DIV";
        case OpCode::MOD: return "MOD";
        case OpCode::POW: return "POW";
        case OpCode::EQ: return "EQ";
        case OpCode::NEQ: return "NEQ";
        case OpCode::GT: return "GT";
        case OpCode::GE: return "GE";
        case OpCode::LT: return "LT";
        case OpCode::LE: return "LE";
        case OpCode::NEG: return "NEG";
        case OpCode::NOT: return "NOT";
        case OpCode::GET_GLOBAL: return "GET_GLOBAL";
        case OpCode::SET_GLOBAL: return "SET_GLOBAL";
        case OpCode::GET_UPVALUE: return "GET_UPVALUE";
        case OpCode::SET_UPVALUE: return "SET_UPVALUE";
        case OpCode::CLOSURE: return "CLOSURE";
        case OpCode::CLOSE_UPVALUES: return "CLOSE_UPVALUES";
        case OpCode::JUMP: return "JUMP";
        case OpCode::JUMP_IF_FALSE: return "JUMP_IF_FALSE";
        case OpCode::JUMP_IF_TRUE: return "JUMP_IF_TRUE";
        case OpCode::CALL: return "CALL";
        case OpCode::RETURN: return "RETURN";
        case OpCode::HALT: return "HALT";
        case OpCode::NEW_ARRAY: return "NEW_ARRAY";
        case OpCode::NEW_HASH: return "NEW_HASH";
        case OpCode::GET_INDEX: return "GET_INDEX";
        case OpCode::SET_INDEX: return "SET_INDEX";
        case OpCode::GET_KEYS: return "GET_KEYS";
        case OpCode::GET_VALUES: return "GET_VALUES";
        case OpCode::NEW_CLASS: return "NEW_CLASS";
        case OpCode::NEW_INSTANCE: return "NEW_INSTANCE";
        case OpCode::GET_PROP: return "GET_PROP";
        case OpCode::SET_PROP: return "SET_PROP";
        case OpCode::SET_METHOD: return "SET_METHOD";
        case OpCode::INHERIT: return "INHERIT";
        case OpCode::GET_SUPER: return "GET_SUPER";
        case OpCode::BIT_AND: return "BIT_AND";
        case OpCode::BIT_OR: return "BIT_OR";
        case OpCode::BIT_XOR: return "BIT_XOR";
        case OpCode::BIT_NOT: return "BIT_NOT";
        case OpCode::LSHIFT: return "LSHIFT";
        case OpCode::RSHIFT: return "RSHIFT";
        case OpCode::THROW: return "THROW";
        case OpCode::SETUP_TRY: return "SETUP_TRY";
        case OpCode::POP_TRY: return "POP_TRY";
        case OpCode::IMPORT_MODULE: return "IMPORT_MODULE";
        case OpCode::EXPORT: return "EXPORT";
        case OpCode::GET_EXPORT: return "GET_EXPORT";
        case OpCode::GET_MODULE_EXPORT: return "GET_MODULE_EXPORT";
        case OpCode::IMPORT_ALL: return "IMPORT_ALL";
        case OpCode::TOTAL_OPCODES: return "TOTAL_OPCODES";
        default: return "UNKNOWN_OPCODE";
    }
}

void MeowVM::throwVMError(const Str& msg) {
    std::ostringstream os;


    auto valueToString = [&](const Value& v) -> Str {
        if (v.is<Null>()) return "<null>";
        if (v.is<Int>()) return std::to_string(v.get<Int>());
        if (v.is<Real>()) {
            std::ostringstream t;
            Real r = v.get<Real>();
            if (std::isnan(r)) return "NaN";
            if (std::isinf(r)) return (r > 0) ? "Infinity" : "-Infinity";
            t << r;
            return t.str();
        }
        if (v.is<Bool>()) return v.get<Bool>() ? "true" : "false";
        if (v.is<Str>()) return Str("\"") + v.get<Str>() + Str("\"");
        if (v.is<Proto>()) return "<function proto>";
        if (v.is<Function>()) return "<closure>";
        if (v.is<Instance>()) return "<instance>";
        if (v.is<Class>()) return "<class>";
        if (v.is<Array>()) return "<array>";
        if (v.is<Object>()) return "<object>";
        if (v.is<Upvalue>()) return "<upvalue>";
        if (v.is<Module>()) return "<module>";
        if (v.is<BoundMethod>()) return "<bound method>";
        if (v.is<NativeFn>()) return "<native fn>";
        return "<unknown value>";
    };


    os << "!!! 🐛 LỖI NGHIÊM TRỌNG: `" << msg << "` 🐛 !!!\n";
    os << "  - VM Base: " << currentBase << "\n";


    ObjFunctionProto* proto = nullptr;
    if (currentFrame && currentFrame->closure) proto = currentFrame->closure->proto;
    if (proto) {
        Int rawIp = currentFrame->ip;
        Int errorIndex = (rawIp > 0) ? (rawIp - 1) : 0;
        const Int codeSize = static_cast<Int>(proto->code.size());

        os << "  - Source: " << proto->sourceName << "\n";
        os << "  - Bytecode index (in-func): " << errorIndex << "\n";
        os << "  - Opcode at error: "
           << ((errorIndex >= 0 && errorIndex < codeSize) ? opToString(proto->code[static_cast<size_t>(errorIndex)].op) : Str("<out-of-range>"))
           << "\n\n";


        const Int range = 5;
        if (codeSize > 0) {
            const Int start = std::max<Int>(0, errorIndex - range);
            const Int end = std::min<Int>(codeSize - 1, errorIndex + range);


            int maxOpLen = 0;
            for (Int i = start; i <= end; ++i) {
                int len = static_cast<int>(opToString(proto->code[static_cast<size_t>(i)].op).size());
                if (len > maxOpLen) maxOpLen = len;
            }
            int opField = std::max(10, maxOpLen + 2);

            os << "  - Vùng bytecode (±" << range << "):\n";

            std::ios::fmtflags savedFlags = os.flags();
            for (Int i = start; i <= end; ++i) {
                const Instruction& inst = proto->code[static_cast<size_t>(i)];

                const char* prefix = (i == errorIndex) ? "  >> " : "     ";
                os << prefix;


                os << std::right << std::setw(4) << i << ": ";


                os << std::left << std::setw(opField) << opToString(inst.op);


                if (!inst.args.empty()) {
                    os << "  args=[";
                    for (size_t a = 0; a < inst.args.size(); ++a) {
                        if (a) os << ", ";
                        Int arg = inst.args[a];
                        os << arg;
                        // if (!proto->constantPool.empty() && arg >= 0 && static_cast<size_t>(arg) < proto->constantPool.size()) {
                        //     os << " -> " << valueToString(proto->constantPool[static_cast<size_t>(arg)]);
                        // }
                    }
                    os << "]";
                } else {
                    os << "  args=[]";
                }

                if (i == errorIndex) os << "    <-- lỗi\n"; else os << "\n";


                os.flags(savedFlags);
            }
        } else {
            os << "  - (Bytecode rỗng)\n";
        }

        if (!proto->constantPool.empty()) {
            os << "\n  - Constant pool (preview up to 10):\n";
            size_t maxShow = std::min<size_t>(proto->constantPool.size(), 10);
            for (size_t ci = 0; ci < maxShow; ++ci) {
                os << "     [" << ci << "]: " << valueToString(proto->constantPool[ci]) << "\n";
            }
        }
    } else {
        os << "  (Không có proto/closure để in chi tiết bytecode.)\n";
    }
    os << "\n";
    os << "  - Call stack (most recent first):\n";
    if (callStack.empty()) {
        os << "     <empty call stack>\n";
    } else {
        for (int i = static_cast<int>(callStack.size()) - 1, depth = 0; i >= 0; --i, ++depth) {
            const CallFrame& f = callStack[static_cast<size_t>(i)];
            Str src = "<native>";
            Int ip = f.ip;
            if (f.closure && f.closure->proto) {
                src = f.closure->proto->sourceName;
            }
            os << "     #" << depth << " " << src << "  ip=" << ip << "  slotStart=" << f.slotStart << "  retReg=" << f.retReg << "\n";
        }
    }

    os << "\n";


    os << "  - Stack snapshot (stackSlots size = " << stackSlots.size() << "):\n";
    if (stackSlots.empty()) {
        os << "     <empty stack>\n";
    } else {
        const size_t maxAround = 8;
        Int base = currentBase;
        size_t start = static_cast<size_t>(std::max<Int>(0, base));
        size_t end = std::min(stackSlots.size(), start + maxAround);

        std::ios::fmtflags savedFlags = os.flags();
        for (size_t i = start; i < end; ++i) {
            os << ((static_cast<Int>(i) == currentBase) ? "  >> " : "     ");
            os << std::right << std::setw(4) << i << ": " << valueToString(stackSlots[i]) << "\n";
            os.flags(savedFlags);
        }
        if (end < stackSlots.size()) {
            os << "     ...\n";
            size_t topCount = std::min<size_t>(3, stackSlots.size());
            for (size_t i = stackSlots.size() - topCount; i < stackSlots.size(); ++i) {
                os << "     (top) " << std::right << std::setw(4) << i << ": " << valueToString(stackSlots[i]) << "\n";
                os.flags(savedFlags);
            }
        }
    }

    os << "\n";


    os << "  - Open upvalues (" << openUpvalues.size() << "):\n";
    if (openUpvalues.empty()) {
        os << "     <none>\n";
    } else {
        for (size_t i = 0; i < openUpvalues.size(); ++i) {
            const Upvalue uv = openUpvalues[i];
            if (!uv) {
                os << "     [" << i << "]: <null upvalue>\n";
            } else {
                os << "     [" << i << "]: slotIndex=" << uv->slotIndex << " state="
                   << (uv->state == ObjUpvalue::State::OPEN ? "OPEN" : "CLOSED")
                   << " value=" << (uv->state == ObjUpvalue::State::CLOSED ? valueToString(uv->closed) : "<live slot>") << "\n";
            }
        }
    }

    os << "\n";

    os << "  - Exception handlers (" << exceptionHandlers.size() << "):\n";
    if (exceptionHandlers.empty()) {
        os << "     <none>\n";
    } else {
        for (size_t i = 0; i < exceptionHandlers.size(); ++i) {
            const auto& h = exceptionHandlers[i];
            os << "     [" << i << "] catchIp=" << h.catchIp << " frameDepth=" << h.frameDepth << " stackDepth=" << h.stackDepth << "\n";
        }
    }
    throw VMError(os.str());
}