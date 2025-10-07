#include "meow_vm.h"
#include "pch.h"

template<class... Ts> 
struct overloaded : Ts... {
    using Ts::operator()...;
};

template<class... Ts> 
overloaded(Ts...) -> overloaded<Ts...>;

void MeowVM::defineNativeFunctions() {
    auto nativePrint = [this](Arguments args) -> Value {
        Str outputString;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) outputString += " ";
            outputString += _toString(args[i]);
        }

        std::cout << outputString << std::endl;
        return Value(Null{});
    };

    auto typeOf = [this](Arguments args) {
        return Value(visit([](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, Null>) return "null";
            if constexpr (std::is_same_v<T, Int>) return "int";
            if constexpr (std::is_same_v<T, Real>) {
                Real r = arg;
                if (std::isinf(r)) return "real";
                if (std::isnan(r)) return "real";
                return "real";
            }
            if constexpr (std::is_same_v<T, Bool>) return "bool";
            if constexpr (std::is_same_v<T, Str>) return "string";
            if constexpr (std::is_same_v<T, Array>) return "array";
            if constexpr (std::is_same_v<T, Object>) return "object";
            if constexpr (std::is_same_v<T, Function>) return "function";
            if constexpr (std::is_same_v<T, NativeFn>) return "native";
            if constexpr (std::is_same_v<T, Upvalue>) return "upvalue";
            if constexpr (std::is_same_v<T, Module>) return "module";
            if constexpr (std::is_same_v<T, Proto>) return "proto";
            
        
            if constexpr (std::is_same_v<T, Class>) return "class";
            if constexpr (std::is_same_v<T, Instance>) return "instance";
            if constexpr (std::is_same_v<T, BoundMethod>) return "bound_method";
            return "unknown";
        }, args[0]));
    };

    auto toInt = [this](Arguments args) {
        return Value(this->_toInt(args[0]));
    };

    auto toReal = [this](Arguments args) {
        return Value(this->_toDouble(args[0]));
    };

    auto toBool = [this](Arguments args) {
        return Value(this->_isTruthy(args[0]));
    };

    auto toStr = [this](Arguments args) {
        return Value(this->_toString(args[0]));
    };

    auto nativeLen = [this](Arguments args) {
        const auto& value = args[0];
        return visit(overloaded{
            [](const Str& s) { return Value((Int)s.length()); },
            [](const Array& a)  { return Value((Int)a->elements.size()); },
            [](const Object& o) { return Value((Int)o->fields.size()); },
            [](const auto&) -> Value { 
                return Int(-1);
            }
        }, value);
    };


    auto nativeAssert = [this](Arguments args) {
        if (!this->_isTruthy(args[0])) {
            Str message = "Assertion failed.";
            if (args.size() > 1 && this->isString(args[1])) {
                message = args[1].get<Str>();
            }
            throwVMError(message);
        }
        return Value(Null{});
    };


    auto nativeOrd = [this](Arguments args) {
        const auto& str = args[0].get<Str>();
        if (str.length() != 1) {
            throwVMError("Hàm ord() chỉ chấp nhận chuỗi có đúng 1 ký tự.");
        }
        return Value((Int)static_cast<unsigned char>(str[0]));
    };


    auto nativeChar = [this](Arguments args) {
        Int code = args[0].get<Int>();
        if (code < 0 || code > 255) {
            throwVMError("Mã ASCII của hàm chr() phải nằm trong khoảng [0, 255].");

        }
        return Value(Str(1, static_cast<char>(code)));
    };

    auto nativeRange = [this](Arguments args) {
        Int start = 0;
        Int stop = 0;
        Int step = 1;
        size_t argCount = args.size();

        if (argCount == 1) {
            stop = args[0].get<Int>();
        } else if (argCount == 2) {
            start = args[0].get<Int>();
            stop = args[1].get<Int>();
        } else {
            start = args[0].get<Int>();
            stop = args[1].get<Int>();
            step = args[2].get<Int>();
        }

        if (step == 0) {
            throwVMError("Tham số 'step' của hàm range() không thể bằng 0.");
        }

        auto resultArrayData = this->memoryManager->newObject<ObjArray>();
        
        if (step > 0) {
            for (Int i = start; i < stop; i += step) {
                resultArrayData->elements.push_back(Value(i));
            }
        } else {
            for (Int i = start; i > stop; i += step) {
                resultArrayData->elements.push_back(Value(i));
            }
        }

        return Value(Array(resultArrayData));
    };


    Value printFunc = nativePrint;
    std::unordered_map<Str, Value> natives;
    natives["print"]  = Value(nativePrint);
    natives["typeof"] = Value(typeOf);
    natives["len"]    = Value(nativeLen);
    natives["assert"] = Value(nativeAssert);
    natives["int"]  = Value(toInt);
    natives["real"] = Value(toReal);
    natives["bool"] = Value(toBool);
    natives["str"]  = Value(toStr);
    natives["ord"]    = Value(nativeOrd);
    natives["char"]   = Value(nativeChar);
    natives["range"]  = Value(nativeRange);


    auto nativeModule = memoryManager->newObject<ObjModule>("native", "native");
    nativeModule->globals = natives;
    moduleCache["native"] = nativeModule;

    std::vector<Str> list = {"array", "object", "string"};

    for (const auto& moduleName : list) {
        try {
            _getOrLoadModule(moduleName, "native", true); 
        } catch (const VMError& e) {
            // std::cerr << "Warning: Could not preload standard module '" 
                    //   << moduleName << "'. " << e.what() << std::endl;
        }
    }
}
