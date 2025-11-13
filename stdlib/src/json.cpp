#include "meow_script.h" // Giả định file này bao gồm value.h, definitions.h, etc.
#include <sstream>
#include <string_view>
#include <iomanip> // Cần cho std::hex, std::setw, std::setfill
#include <vector>

namespace Detail {

class JsonParser {
private:
    std::string_view json;
    size_t pos = 0;
    MeowEngine* engine;
    bool hasError = false;

    // Helper để báo lỗi mà không throw exception
    Value reportError() {
        hasError = true;
        return Value(Null{});
    }

    char peek() const {
        return pos < json.length() ? json[pos] : '\0';
    }

    void advance() {
        if (pos < json.length()) {
            pos++;
        }
    }

    void skipWhitespace() {
        while (pos < json.length() && isspace(static_cast<unsigned char>(json[pos]))) {
            pos++;
        }
    }

    Value parseValue();
    Value parseObject();
    Value parseArray();
    Value parseString();
    Value parseNumber();
    Value parseTrue();
    Value parseFalse();
    Value parseNull();

public:
    explicit JsonParser(MeowEngine* e) : engine(e) {}

    Value parse(const Str& str) {
        json = str;
        pos = 0;
        hasError = false;
        skipWhitespace();
        Value result = parseValue();
        if (hasError) return result; // Trả về Null nếu có lỗi
        
        skipWhitespace();
        if (pos < json.length()) {
            // Có ký tự thừa sau khi parse xong
            return reportError();
        }
        return result;
    }
};

Value JsonParser::parseValue() {
    skipWhitespace();
    if (pos >= json.length()) return reportError();
    
    char c = peek();
    switch (c) {
        case '{': return parseObject();
        case '[': return parseArray();
        case '"': return parseString();
        case 't': return parseTrue();
        case 'f': return parseFalse();
        case 'n': return parseNull();
        default:
            if (isdigit(c) || c == '-') {
                return parseNumber();
            }
            return reportError();
    }
}

Value JsonParser::parseObject() {
    advance(); // Bỏ qua '{'
    auto obj = engine->getMemoryManager()->newObject<ObjObject>();
    
    skipWhitespace();
    if (peek() == '}') {
        advance();
        return Value(obj);
    }

    while (true) {
        skipWhitespace();
        if (peek() != '"') return reportError(); // Key phải là string
        
        Value key_val = parseString();
        if (hasError) return key_val;

        // Trong MeowScript, key của object luôn là string.
        const Str& key_str = key_val.get<Str>();

        skipWhitespace();
        if (peek() != ':') return reportError();
        advance();
        
        Value val = parseValue();
        if (hasError) return val;
        
        obj->fields[key_str] = val;

        skipWhitespace();
        char nextChar = peek();
        if (nextChar == '}') {
            advance();
            break;
        }
        if (nextChar != ',') return reportError();
        advance();
    }
    return Value(obj);
}

Value JsonParser::parseArray() {
    advance(); // Bỏ qua '['
    auto arr = engine->getMemoryManager()->newObject<ObjArray>();
    
    skipWhitespace();
    if (peek() == ']') {
        advance();
        return Value(arr);
    }
    while (true) {
        Value elem = parseValue();
        if (hasError) return elem;
        arr->elements.push_back(elem);
        
        skipWhitespace();
        char nextChar = peek();
        if (nextChar == ']') {
            advance();
            break;
        }
        if (nextChar != ',') return reportError();
        advance();
    }
    return Value(arr);
}

// NÂNG CẤP: Hỗ trợ đầy đủ Unicode (\uXXXX)
Value JsonParser::parseString() {
    advance(); // Bỏ qua '"'
    Str s;
    while (pos < json.length() && peek() != '"') {
        if (peek() == '\\') {
            advance();
            if (pos >= json.length()) return reportError();
            char escapedChar = peek();
            switch(escapedChar) {
                case '"':  s += '"'; break;
                case '\\': s += '\\'; break;
                case '/':  s += '/'; break;
                case 'b':  s += '\b'; break;
                case 'f':  s += '\f'; break;
                case 'n':  s += '\n'; break;
                case 'r':  s += '\r'; break;
                case 't':  s += '\t'; break;
                case 'u': {
                    advance(); // Bỏ qua 'u'
                    if (pos + 4 > json.length()) return reportError();
                    
                    unsigned int codepoint = 0;
                    for (int i = 0; i < 4; ++i) {
                        char h = peek();
                        advance();
                        codepoint <<= 4;
                        if (h >= '0' && h <= '9') codepoint |= (h - '0');
                        else if (h >= 'a' && h <= 'f') codepoint |= (10 + h - 'a');
                        else if (h >= 'A' && h <= 'F') codepoint |= (10 + h - 'A');
                        else return reportError();
                    }

                    // Chuyển codepoint thành UTF-8
                    if (codepoint <= 0x7F) {
                        s += static_cast<char>(codepoint);
                    } else if (codepoint <= 0x7FF) {
                        s += static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F));
                        s += static_cast<char>(0x80 | (codepoint & 0x3F));
                    } else { // Surrogate pairs không được hỗ trợ trong logic này, nhưng đủ cho BMP
                        s += static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F));
                        s += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                        s += static_cast<char>(0x80 | (codepoint & 0x3F));
                    }
                    continue; // advance() đã được gọi bên trong, ta cần continue để tránh gọi lần nữa
                }
                default:
                    // JSON không cho phép các escape sequence khác, nhưng ta có thể chấp nhận
                    s += escapedChar;
            }
        } else {
            s += peek();
        }
        advance();
    }
    if (pos >= json.length() || peek() != '"') {
        return reportError();
    }
    advance(); // Bỏ qua '"' cuối
    return Value(s);
}


Value JsonParser::parseNumber() {
    size_t start = pos;
    if (peek() == '-') advance();
    
    // Phần nguyên
    if (peek() == '0') {
        advance();
    } else if (peek() >= '1' && peek() <= '9') {
        while (pos < json.length() && isdigit(peek())) advance();
    } else {
        return reportError();
    }

    bool isReal = false;
    // Phần thập phân
    if (pos < json.length() && peek() == '.') {
        isReal = true;
        advance();
        if (!isdigit(peek())) return reportError();
        while (pos < json.length() && isdigit(peek())) advance();
    }

    // Phần mũ
    if (pos < json.length() && (peek() == 'e' || peek() == 'E')) {
        isReal = true;
        advance();
        if (pos < json.length() && (peek() == '+' || peek() == '-')) advance();
        if (!isdigit(peek())) return reportError();
        while (pos < json.length() && isdigit(peek())) advance();
    }

    std::string numStr(json.substr(start, pos - start));
    try {
        if (isReal) {
            return Value(std::stod(numStr));
        } else {
            return Value(static_cast<Int>(std::stoll(numStr)));
        }
    } catch (const std::out_of_range&) {
        // Số quá lớn hoặc quá nhỏ, trả về Real
        return Value(std::stod(numStr));
    } catch (...) {
        return reportError();
    }
}

Value JsonParser::parseTrue() {
    if (json.substr(pos, 4) == "true") {
        pos += 4;
        return Value(true);
    }
    return reportError();
}

Value JsonParser::parseFalse() {
    if (json.substr(pos, 5) == "false") {
        pos += 5;
        return Value(false);
    }
    return reportError();
}

Value JsonParser::parseNull() {
    if (json.substr(pos, 4) == "null") {
        pos += 4;
        return Value(Null{});
    }
    return reportError();
}


// NÂNG CẤP: Escape ký tự điều khiển (0x00 - 0x1F)
Str escapeJsonString(const Str& s) {
    std::ostringstream o;
    o << '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"':  o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\b': o << "\\b"; break;
            case '\f': o << "\\f"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:
                if (c <= '\x1f') {
                    o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    o << c;
                }
        }
    }
    o << '"';
    return o.str();
}

Str toJsonRecursive(const Value& value, int indentLevel, int tabSize) {
    std::ostringstream ss;
    Str indent(indentLevel * tabSize, ' ');
    Str innerIndent((indentLevel + 1) * tabSize, ' ');

    std::visit([&](const auto& val) {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, Null>) {
            ss << "null";
        } else if constexpr (std::is_same_v<T, Bool>) {
            ss << (val ? "true" : "false");
        } else if constexpr (std::is_same_v<T, Int>) {
            ss << val;
        } else if constexpr (std::is_same_v<T, Real>) {
            ss << val;
        } else if constexpr (std::is_same_v<T, Str>) {
            ss << escapeJsonString(val);
        } else if constexpr (std::is_same_v<T, Array>) { // Sử dụng ObjArray*
            if (val->elements.empty()) {
                ss << "[]";
            } else {
                ss << "[\n";
                for (size_t i = 0; i < val->elements.size(); ++i) {
                    ss << innerIndent << toJsonRecursive(val->elements[i], indentLevel + 1, tabSize);
                    if (i + 1 < val->elements.size()) ss << ",";
                    ss << "\n";
                }
                ss << indent << "]";
            }
        } else if constexpr (std::is_same_v<T, Object>) { // Sử dụng ObjObject*
            if (val->fields.empty()) {
                ss << "{}";
            } else {
                ss << "{\n";
                size_t i = 0;
                for (const auto& pair : val->fields) {
                    ss << innerIndent << escapeJsonString(pair.first) << ": " << toJsonRecursive(pair.second, indentLevel + 1, tabSize);
                    if (i + 1 < val->fields.size()) ss << ",";
                    ss << "\n";
                    i++;
                }
                ss << indent << "}";
            }
        } else {
            ss << "\"<unsupported_type>\"";
        }
    }, static_cast<const BaseValue&>(value));
    return ss.str();
}

} // namespace Detail

Value parse(MeowEngine* engine, Arguments args) {
    if (args.empty() || !args[0].is<Str>()) {
        return Value(Null{});
    }
    const Str& jsonString = args[0].get<Str>();

    Detail::JsonParser parser(engine);
    return parser.parse(jsonString);
}

Value stringify(Arguments args) {
    if (args.empty()) {
        return Value(Null{});
    }
    const Value& valueToConvert = args[0];
    int tabSize = 2;

    if (args.size() > 1 && args[1].is<Int>()) {
        tabSize = static_cast<int>(args[1].get<Int>());
        if (tabSize < 0) tabSize = 0;
    }
    
    Str result = Detail::toJsonRecursive(valueToConvert, 0, tabSize);
    return Value(result);
}

Module CreateMeowModule(MeowEngine* engine) {
    MemoryManager* mm = engine->getMemoryManager();
    auto jsonModule = mm->newObject<ObjModule>("json", "native:json");

    // Thay đổi để phù hợp với NativeFnAdvanced
    jsonModule->exports["stringify"] = Value(stringify);
    jsonModule->exports["parse"] = Value(parse);

    return jsonModule;
}