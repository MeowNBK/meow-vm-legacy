#include "meow_script.h"
#include "value.h"
#include <algorithm>
#include <iostream>

void log(const std::string& msg) {
    std::cout << msg << "\n";
}

static inline Bool isTruthy(const Value& v) {
    if (v.is<Null>()) return false;
    if (v.is<Bool>()) return v.get<Bool>();
    if (v.is<Int>()) return v.get<Int>() != 0;
    if (v.is<Real>()) return v.get<Real>() != 0.0;
    if (v.is<Str>()) return !v.get<Str>().empty();
    if (v.is<Array>()) return !v.get<Array>()->elements.empty();
    if (v.is<Object>()) return !v.get<Object>()->fields.empty();
    return true;
}

static inline bool isNumber(const Value& v) {
    return v.is<Int>() || v.is<Real>();
}
static inline double asNumber(const Value& v) {
    return v.is<Int>() ? static_cast<double>(v.get<Int>()) : v.get<Real>();
}


Value arrayPush(Arguments args) {
    if (args.empty() || !args[0].is<Array>()) return Value(Null{});
    ObjArray* arr = args[0].get<Array>();
    for (size_t i = 1; i < args.size(); i++) {
        arr->elements.push_back(args[i]);
    }

    return Value(static_cast<Int>(arr->elements.size()));
}


Value arrayPop(Arguments args) {
    if (args.empty() || !args[0].is<Array>()) return Value(Null{});
    ObjArray* arr = args[0].get<Array>();
    if (arr->elements.empty()) return Value(Null{});
    Value last = arr->elements.back();
    arr->elements.pop_back();
    return last;
}


Value arrayGetIndex(Arguments args) {
    if (args.size() < 2 || !args[0].is<Array>() || !args[1].is<Int>())
        return Value(Null{});
    ObjArray* arr = args[0].get<Array>();
    Int idx = args[1].get<Int>();
    Int n = static_cast<Int>(arr->elements.size());
    if (idx < 0) idx += n;
    if (idx < 0 || idx >= n) return Value(Null{});
    return arr->elements[idx];
}


Value arraySlice(MeowEngine* engine, Arguments args) {
    if (args.empty() || !args[0].is<Array>()) return Value(Null{});
    ObjArray* src = args[0].get<Array>();
    Int n = static_cast<Int>(src->elements.size());
    Int start = 0;
    Int end = n;
    if (args.size() > 1 && args[1].is<Int>()) start = args[1].get<Int>();
    if (args.size() > 2 && args[2].is<Int>()) end = args[2].get<Int>();
    if (start < 0) start += n;
    if (end < 0) end += n;
    if (start < 0) start = 0;
    if (end > n) end = n;

    ObjArray* dst = engine->getMemoryManager()->newObject<ObjArray>();
    if (start < end) {
        dst->elements.reserve(end - start);
        for (Int i = start; i < end; i++) {
            dst->elements.push_back(src->elements[i]);
        }
    }
    return Value(dst);
}


Value arrayMap(MeowEngine* engine, Arguments args) {
    if (args.size() < 2 || !args[0].is<Array>() || !args[1].is<Function>()) return Value(Null{});
    ObjArray* arr = args[0].get<Array>();
    Function cb = args[1].get<Function>();

    ObjArray* dst = engine->getMemoryManager()->newObject<ObjArray>();
    dst->elements.reserve(arr->elements.size());
    for (const auto& el : arr->elements) {
        Value r = engine->call(cb, { el });
        dst->elements.push_back(r);
    }
    return Value(dst);
}


Value arrayFilter(MeowEngine* engine, Arguments args) {
    if (args.size() < 2 || !args[0].is<Array>() || !args[1].is<Function>()) return Value(Null{});
    ObjArray* arr = args[0].get<Array>();
    Function cb = args[1].get<Function>();

    ObjArray* dst = engine->getMemoryManager()->newObject<ObjArray>();
    for (const auto& el : arr->elements) {
        Value r = engine->call(cb, { el });
        if (isTruthy(r)) dst->elements.push_back(el);
    }
    return Value(dst);
}


Value arrayReduce(MeowEngine* engine, Arguments args) {
    if (args.size() < 3 || !args[0].is<Array>() || !args[1].is<Function>()) return Value(Null{});
    ObjArray* arr = args[0].get<Array>();
    Function cb = args[1].get<Function>();
    Value acc = args[2];
    for (const auto& el : arr->elements) {
        acc = engine->call(cb, { acc, el });
    }
    return acc;
}


Value arrayForEach(MeowEngine* engine, Arguments args) {
    if (args.size() < 2 || !args[0].is<Array>() || !args[1].is<Function>()) return Value(Null{});
    ObjArray* arr = args[0].get<Array>();
    Function cb = args[1].get<Function>();
    for (size_t i = 0; i < arr->elements.size(); i++) {
        engine->call(cb, { arr->elements[i], Value(static_cast<Int>(i)) });
    }
    return Value(Null{});
}


Value arrayFind(MeowEngine* engine, Arguments args) {
    if (args.size() < 2 || !args[0].is<Array>() || !args[1].is<Function>()) return Value(Null{});
    ObjArray* arr = args[0].get<Array>();
    Function cb = args[1].get<Function>();
    for (size_t i = 0; i < arr->elements.size(); i++) {
        Value r = engine->call(cb, { arr->elements[i], Value(static_cast<Int>(i)) });
        if (isTruthy(r)) return arr->elements[i];
    }
    return Value(Null{});
}


Value arrayFindIndex(MeowEngine* engine, Arguments args) {
    if (args.size() < 2 || !args[0].is<Array>() || !args[1].is<Function>()) return Value(static_cast<Int>(-1));
    ObjArray* arr = args[0].get<Array>();
    Function cb = args[1].get<Function>();
    for (size_t i = 0; i < arr->elements.size(); i++) {
        Value r = engine->call(cb, { arr->elements[i], Value(static_cast<Int>(i)) });
        if (isTruthy(r)) return Value(static_cast<Int>(i));
    }
    return Value(static_cast<Int>(-1));
}


Value arrayReverse(Arguments args) {
    if (args.empty() || !args[0].is<Array>()) return Value(Null{});
    ObjArray* arr = args[0].get<Array>();
    std::reverse(arr->elements.begin(), arr->elements.end());
    return args[0];
}


Value arraySort(MeowEngine* engine, Arguments args) {
    if (args.empty() || !args[0].is<Array>()) return Value(Null{});
    ObjArray* arr = args[0].get<Array>();
    bool hasCmp = args.size() > 1 && args[1].is<Function>();
    Function cmp;
    if (hasCmp) cmp = args[1].get<Function>();

    auto& elems = arr->elements;
    std::sort(elems.begin(), elems.end(), [&](const Value& a, const Value& b) {
        if (hasCmp) {
            Value r = engine->call(cmp, { a, b });
            if (r.is<Int>()) return r.get<Int>() < 0;
            if (r.is<Real>()) return r.get<Real>() < 0.0;
            return false;
        }
        if (isNumber(a) && isNumber(b)) return asNumber(a) < asNumber(b);
        if (a.is<Str>() && b.is<Str>()) return a.get<Str>() < b.get<Str>();
        return false;
    });
    return args[0];
}


Value arrayReserve(Arguments args) {
    if (args.size() < 2 || !args[0].is<Array>() || !args[1].is<Int>()) return Value(Null{});
    ObjArray* arr = args[0].get<Array>();
    Int cap = args[1].get<Int>();
    if (cap < 0) return Value(Null{});
    arr->elements.reserve(static_cast<size_t>(cap));
    return Value(Null{});
}


Value arrayResize(Arguments args) {
    if (args.size() < 2 || !args[0].is<Array>() || !args[1].is<Int>()) return Value(Null{});
    ObjArray* arr = args[0].get<Array>();
    Int n = args[1].get<Int>();
    if (n < 0) return Value(Null{});
    if (args.size() > 2) {
        arr->elements.resize(static_cast<size_t>(n), args[2]);
    } else {
        arr->elements.resize(static_cast<size_t>(n), Value(Null{}));
    }
    return Value(Null{});
}


Value arrayLength(Arguments args) {
    if (args.empty() || !args[0].is<Array>()) return Value(Null{});
    ObjArray* arr = args[0].get<Array>();
    return Value(static_cast<Int>(arr->elements.size()));
}


Module CreateMeowModule(MeowEngine* engine) {
    MemoryManager* mm = engine->getMemoryManager();
    auto arrayModule = mm->newObject<ObjModule>("array", "native:array");

    arrayModule->exports["push"]        = Value(arrayPush);
    arrayModule->exports["pop"]         = Value(arrayPop);
    arrayModule->exports["__getindex__"]= Value(arrayGetIndex);
    arrayModule->exports["slice"]       = Value(arraySlice);

    arrayModule->exports["map"]         = Value(arrayMap);
    arrayModule->exports["filter"]      = Value(arrayFilter);
    arrayModule->exports["reduce"]      = Value(arrayReduce);
    arrayModule->exports["forEach"]     = Value(arrayForEach);
    arrayModule->exports["find"]        = Value(arrayFind);
    arrayModule->exports["findIndex"]   = Value(arrayFindIndex);

    arrayModule->exports["reverse"]     = Value(arrayReverse);
    arrayModule->exports["sort"]        = Value(arraySort);

    arrayModule->exports["reserve"]     = Value(arrayReserve);
    arrayModule->exports["resize"]      = Value(arrayResize);

    arrayModule->exports["size"]      = Value(arrayLength);

    for (const auto& [name, fn] : arrayModule->exports) {
        engine->registerMethod("Array", name, fn);
    }

    engine->registerGetter("Array", "length", Value(arrayLength));
    return arrayModule;
}
