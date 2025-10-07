
#include "meow_script.h"
#include <vector>


Value native_object_keys(Arguments args) {
    if (args.empty() || !args[0].is<Object>()) return Value(Null{});
    Object obj = args[0].get<Object>();
    Array out = new ObjArray();
    out->elements.reserve(obj->fields.size());
    for (const auto& kv : obj->fields) {
        out->elements.emplace_back(Value(kv.first));
    }
    return Value(out);
}


Value native_object_values(Arguments args) {
    if (args.empty() || !args[0].is<Object>()) return Value(Null{});
    Object obj = args[0].get<Object>();
    Array out = new ObjArray();
    out->elements.reserve(obj->fields.size());
    for (const auto& kv : obj->fields) {
        out->elements.emplace_back(kv.second);
    }
    return Value(out);
}


Value native_object_entries(Arguments args) {
    if (args.empty() || !args[0].is<Object>()) return Value(Null{});
    Object obj = args[0].get<Object>();
    Array out = new ObjArray();
    out->elements.reserve(obj->fields.size());
    for (const auto& kv : obj->fields) {
        Array pair = new ObjArray();
        pair->elements.emplace_back(Value(kv.first));
        pair->elements.emplace_back(kv.second);
        out->elements.emplace_back(Value(pair));
    }
    return Value(out);
}


Value native_object_has(Arguments args) {
    if (args.size() < 2 || !args[0].is<Object>() || !args[1].is<Str>()) return Value(false);
    Object obj = args[0].get<Object>();
    const Str& key = args[1].get<Str>();
    return Value(obj->fields.find(key) != obj->fields.end());
}


Value native_object_merge(Arguments args) {
    if (args.empty()) return Value(Null{});
    Object result = new ObjObject();

    for (size_t i = 0; i < args.size(); ++i) {
        if (!args[i].is<Object>()) continue;
        Object src = args[i].get<Object>();
        for (const auto& kv : src->fields) {

            result->fields[kv.first] = kv.second;
        }
    }
    return Value(result);
}


Module CreateMeowModule(MeowEngine* engine) {
    MemoryManager* mm = engine->getMemoryManager();
    auto mod = mm->newObject<ObjModule>("object", "native:object");

    mod->exports["keys"]   = Value(native_object_keys);
    mod->exports["values"] = Value(native_object_values);
    mod->exports["entries"]= Value(native_object_entries);
    mod->exports["has"]    = Value(native_object_has);
    mod->exports["merge"]  = Value(native_object_merge);

    for (const auto& [name, fn] : mod->exports) {
        engine->registerMethod("Object", name, fn);
    }
    return mod;
}
