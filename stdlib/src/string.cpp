
#include "meow_script.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iomanip>


static std::string _toString(const Value& v) {
    if (v.is<Null>()) return "null";
    if (v.is<Bool>()) return v.get<Bool>() ? "true" : "false";
    if (v.is<Int>()) return std::to_string(v.get<Int>());
    if (v.is<Real>()) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(15) << v.get<Real>();
        return ss.str();
    }
    if (v.is<Str>()) return v.get<Str>();
    if (v.is<Array>()) {
        const auto& vec = v.get<Array>()->elements;
        std::string out = "[";
        for (size_t i = 0; i < vec.size(); ++i) {
            if (i) out += ", ";
            out += _toString(vec[i]);
        }
        out += "]";
        return out;
    }
    if (v.is<Object>()) {
        const auto& m = v.get<Object>()->fields;
        std::string out = "{";
        bool first = true;
        for (const auto& p : m) {
            if (!first) out += ", ";
            out += p.first + ": " + _toString(p.second);
            first = false;
        }
        out += "}";
        return out;
    }
    if (v.is<Function>()) return "<fn>";
    if (v.is<BoundMethod>()) return "<bound method>";
    if (v.is<Module>()) return "<module>";
    return "<unknown_string>";
}


static inline const std::string& valToStdStr(const Value& v) {
    return v.get<Str>();
}


Value native_string_split(Arguments args) {
    if (args.empty() || !args[0].is<Str>()) return Value(Null{});
    const std::string& str = valToStdStr(args[0]);
    std::string delimiter = " ";
    if (args.size() > 1 && args[1].is<Str>()) delimiter = valToStdStr(args[1]);

    Array result = new ObjArray();
    size_t start = 0, end;
    if (delimiter.empty()) {

        for (char c : str) result->elements.emplace_back(Value(std::string(1, c)));
        return Value(result);
    }
    while ((end = str.find(delimiter, start)) != std::string::npos) {
        result->elements.emplace_back(Value(str.substr(start, end - start)));
        start = end + delimiter.length();
    }
    result->elements.emplace_back(Value(str.substr(start)));
    return Value(result);
}


Value native_string_join(Arguments args) {
    if (args.size() < 2 || !args[0].is<Str>() || !args[1].is<Array>()) return Value(Null{});
    const std::string& sep = valToStdStr(args[0]);
    Array arr = args[1].get<Array>();

    std::ostringstream os;
    for (size_t i = 0; i < arr->elements.size(); ++i) {
        os << _toString(arr->elements[i]);
        if (i + 1 < arr->elements.size()) os << sep;
    }
    return Value(os.str());
}


Value native_string_upper(Arguments args) {
    if (args.empty() || !args[0].is<Str>()) return Value(Null{});
    std::string s = valToStdStr(args[0]);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::toupper(c); });
    return Value(s);
}


Value native_string_lower(Arguments args) {
    if (args.empty() || !args[0].is<Str>()) return Value(Null{});
    std::string s = valToStdStr(args[0]);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return Value(s);
}


Value native_string_trim(Arguments args) {
    if (args.empty() || !args[0].is<Str>()) return Value(Null{});
    std::string s = valToStdStr(args[0]);

    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch){ return !std::isspace(ch); }).base(), s.end());

    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch){ return !std::isspace(ch); }));
    return Value(s);
}


Value native_string_startsWith(Arguments args) {
    if (args.size() < 2 || !args[0].is<Str>() || !args[1].is<Str>()) return Value(false);
    const std::string& s = valToStdStr(args[0]);
    const std::string& pref = valToStdStr(args[1]);
    return Value(s.rfind(pref, 0) == 0);
}


Value native_string_endsWith(Arguments args) {
    if (args.size() < 2 || !args[0].is<Str>() || !args[1].is<Str>()) return Value(false);
    const std::string& s = valToStdStr(args[0]);
    const std::string& suf = valToStdStr(args[1]);
    if (suf.size() > s.size()) return Value(false);
    return Value(std::equal(suf.rbegin(), suf.rend(), s.rbegin()));
}


Value native_string_replace(Arguments args) {
    if (args.size() < 3 || !args[0].is<Str>() || !args[1].is<Str>() || !args[2].is<Str>()) return Value(Null{});
    std::string s = valToStdStr(args[0]);
    const std::string& from = valToStdStr(args[1]);
    const std::string& to   = valToStdStr(args[2]);
    size_t pos = s.find(from);
    if (pos != std::string::npos) s.replace(pos, from.size(), to);
    return Value(s);
}


Value native_string_contains(Arguments args) {
    if (args.size() < 2 || !args[0].is<Str>() || !args[1].is<Str>()) return Value(false);
    const std::string& s = valToStdStr(args[0]);
    const std::string& sub = valToStdStr(args[1]);
    return Value(s.find(sub) != std::string::npos);
}


Value native_string_indexOf(Arguments args) {
    if (args.size() < 2 || !args[0].is<Str>() || !args[1].is<Str>()) return Value(static_cast<Int>(-1));
    const std::string& s = valToStdStr(args[0]);
    const std::string& sub = valToStdStr(args[1]);
    size_t start = 0;
    if (args.size() > 2 && args[2].is<Int>()) start = static_cast<size_t>(args[2].get<Int>());
    size_t pos = s.find(sub, start);
    return Value((pos == std::string::npos) ? static_cast<Int>(-1) : static_cast<Int>(pos));
}


Value native_string_lastIndexOf(Arguments args) {
    if (args.size() < 2 || !args[0].is<Str>() || !args[1].is<Str>()) return Value(static_cast<Int>(-1));
    const std::string& s = valToStdStr(args[0]);
    const std::string& sub = valToStdStr(args[1]);
    size_t pos = s.rfind(sub);
    return Value((pos == std::string::npos) ? static_cast<Int>(-1) : static_cast<Int>(pos));
}


Value native_string_substring(Arguments args) {
    if (args.size() < 2 || !args[0].is<Str>() || !args[1].is<Int>()) return Value(Null{});
    const std::string& s = valToStdStr(args[0]);
    size_t start = static_cast<size_t>(args[1].get<Int>());
    if (start > s.size()) return Value(std::string(""));
    size_t len = s.size() - start;
    if (args.size() > 2 && args[2].is<Int>()) len = static_cast<size_t>(args[2].get<Int>());
    return Value(s.substr(start, len));
}


Value native_string_slice(Arguments args) {
    if (args.size() < 2 || !args[0].is<Str>() || !args[1].is<Int>()) return Value(Null{});
    const std::string& s = valToStdStr(args[0]);
    int start = static_cast<int>(args[1].get<Int>());
    int end = (args.size() > 2 && args[2].is<Int>()) ? static_cast<int>(args[2].get<Int>()) : static_cast<int>(s.size());
    if (start < 0) start += (int)s.size();
    if (end < 0) end += (int)s.size();
    if (start < 0) start = 0;
    if (end > (int)s.size()) end = (int)s.size();
    if (start >= end) return Value(std::string(""));
    return Value(s.substr(static_cast<size_t>(start), static_cast<size_t>(end - start)));
}


Value native_string_repeat(Arguments args) {
    if (args.size() < 2 || !args[0].is<Str>() || !args[1].is<Int>()) return Value(std::string(""));
    const std::string& s = valToStdStr(args[0]);
    int count = static_cast<int>(args[1].get<Int>());
    if (count <= 0) return Value(std::string(""));
    std::string out;
    out.reserve(s.size() * (size_t)count);
    for (int i = 0; i < count; ++i) out += s;
    return Value(out);
}


Value native_string_padLeft(Arguments args) {
    if (args.size() < 2 || !args[0].is<Str>() || !args[1].is<Int>()) return Value(Null{});
    std::string s = valToStdStr(args[0]);
    size_t length = static_cast<size_t>(args[1].get<Int>());
    char ch = ' ';
    if (args.size() > 2 && args[2].is<Str>() && !valToStdStr(args[2]).empty()) ch = valToStdStr(args[2])[0];
    if (s.size() < length) s.insert(s.begin(), length - s.size(), ch);
    return Value(s);
}


Value native_string_padRight(Arguments args) {
    if (args.size() < 2 || !args[0].is<Str>() || !args[1].is<Int>()) return Value(Null{});
    std::string s = valToStdStr(args[0]);
    size_t length = static_cast<size_t>(args[1].get<Int>());
    char ch = ' ';
    if (args.size() > 2 && args[2].is<Str>() && !valToStdStr(args[2]).empty()) ch = valToStdStr(args[2])[0];
    if (s.size() < length) s.append(length - s.size(), ch);
    return Value(s);
}


Value native_string_equalsIgnoreCase(Arguments args) {
    if (args.size() < 2 || !args[0].is<Str>() || !args[1].is<Str>()) return Value(false);
    std::string a = valToStdStr(args[0]);
    std::string b = valToStdStr(args[1]);
    std::transform(a.begin(), a.end(), a.begin(), [](unsigned char c){ return std::tolower(c); });
    std::transform(b.begin(), b.end(), b.begin(), [](unsigned char c){ return std::tolower(c); });
    return Value(a == b);
}


Value native_string_charAt(Arguments args) {
    if (args.size() < 2 || !args[0].is<Str>() || !args[1].is<Int>()) return Value(std::string(""));
    const std::string& s = valToStdStr(args[0]);
    size_t idx = static_cast<size_t>(args[1].get<Int>());
    if (idx >= s.size()) return Value(std::string(""));
    return Value(std::string(1, s[idx]));
}


Value native_string_charCodeAt(Arguments args) {
    if (args.size() < 2 || !args[0].is<Str>() || !args[1].is<Int>()) return Value(static_cast<Int>(-1));
    const std::string& s = valToStdStr(args[0]);
    size_t idx = static_cast<size_t>(args[1].get<Int>());
    if (idx >= s.size()) return Value(static_cast<Int>(-1));
    return Value(static_cast<Int>(static_cast<unsigned char>(s[idx])));
}


Value native_string_fromCharCode(Arguments args) {
    if (args.empty() || !args[0].is<Int>()) return Value(std::string(""));
    int code = static_cast<int>(args[0].get<Int>());
    return Value(std::string(1, static_cast<char>(code)));
}

Value stringLength(Arguments args) {
    if (args.empty() || !args[0].is<Str>()) return Value(Null{});
    return Value(static_cast<Int>(args[0].get<Str>().size()));
}


Module CreateMeowModule(MeowEngine* engine) {
    MemoryManager* mm = engine->getMemoryManager();
    auto stringModule = mm->newObject<ObjModule>("string", "native:string");


    stringModule->exports["split"] = Value(native_string_split);
    stringModule->exports["join"]  = Value(native_string_join);
    stringModule->exports["upper"] = Value(native_string_upper);
    stringModule->exports["lower"] = Value(native_string_lower);
    stringModule->exports["trim"]  = Value(native_string_trim);

    stringModule->exports["startsWith"] = Value(native_string_startsWith);
    stringModule->exports["endsWith"]   = Value(native_string_endsWith);
    stringModule->exports["replace"]    = Value(native_string_replace);
    stringModule->exports["contains"]   = Value(native_string_contains);
    stringModule->exports["indexOf"]    = Value(native_string_indexOf);
    stringModule->exports["lastIndexOf"]= Value(native_string_lastIndexOf);

    stringModule->exports["substring"]  = Value(native_string_substring);
    stringModule->exports["slice"]      = Value(native_string_slice);
    stringModule->exports["repeat"]     = Value(native_string_repeat);

    stringModule->exports["padLeft"]    = Value(native_string_padLeft);
    stringModule->exports["padRight"]   = Value(native_string_padRight);
    stringModule->exports["equalsIgnoreCase"] = Value(native_string_equalsIgnoreCase);

    stringModule->exports["charAt"]     = Value(native_string_charAt);
    stringModule->exports["charCodeAt"] = Value(native_string_charCodeAt);
    stringModule->exports["fromCharCode"]= Value(native_string_fromCharCode);

    stringModule->exports["size"]= Value(stringLength);



    for (const auto& pair : stringModule->exports) {
        engine->registerMethod("String", pair.first, pair.second);
    }

    engine->registerGetter("String", "length", Value(stringLength));

    return stringModule;
}
