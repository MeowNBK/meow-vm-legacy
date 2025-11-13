#include "meow_vm.h"

void MeowVM::opNewArray() {
    Int dst = currentInst->args[0], startIdx = currentInst->args[1], count = currentInst->args[2];
    if (count < 0 || startIdx < 0) throwVMError("NEW_ARRAY: invalid range");
    if (currentBase + startIdx + count > static_cast<Int>(stackSlots.size()))
        throwVMError("NEW_ARRAY: register range OOB");

    Array arr = new ObjArray();
    arr->elements.reserve(count);
    for (Int i = 0; i < count; ++i) {
        arr->elements.push_back(stackSlots[currentBase + startIdx + i]);
    }
    stackSlots[currentBase + dst] = Value(arr);
}

void MeowVM::opNewHash() {
    Int dst = currentInst->args[0], startIdx = currentInst->args[1], count = currentInst->args[2];
    if (count < 0 || startIdx < 0) throwVMError("NEW_HASH: invalid range");
    if (currentBase + startIdx + count*2 > static_cast<Int>(stackSlots.size()))
        throwVMError("NEW_HASH: register range OOB");

    Object hm = new ObjObject();
    for (Int i = 0; i < count; ++i) {
        Value& key = stackSlots[currentBase + startIdx + i * 2];
        Value& val = stackSlots[currentBase + startIdx + i * 2 + 1];
        hm->fields[_toString(key)] = val;
    }
    stackSlots[currentBase + dst] = Value(hm);
}

void MeowVM::opGetIndex() {
    Int dst = currentInst->args[0];
    Int srcReg = currentInst->args[1];
    Int keyReg = currentInst->args[2];

    if (currentBase + srcReg >= static_cast<Int>(stackSlots.size()) ||
        currentBase + keyReg >= static_cast<Int>(stackSlots.size()) ||
        currentBase + dst >= static_cast<Int>(stackSlots.size()))
        throwVMError("GET_INDEX register OOB");

    Value& src = stackSlots[currentBase + srcReg];
    Value& key = stackSlots[currentBase + keyReg];


    if (auto mm = getMagicMethod(src, "__getindex__")) {
        Value res = call(*mm, { key });
        stackSlots[currentBase + dst] = res;
        return;
    }


    if (isInt(key)) {
        Int idx = _toInt(key);
        if (isVector(src)) {
            Array arr = src.get<Array>(); if (!arr) throwVMError("Array null in GET_INDEX");
            auto &elems = arr->elements;
            if (idx < 0 || idx >= static_cast<Int>(elems.size())) {
                std::ostringstream os;
                os << "  -  Chỉ số vượt quá phạm vi: '" << idx << "'. ";
                os << "  -  Được truy cập trên mảng: `\n" << _toString(arr) << "\n`";
                throwVMError(os.str());

            }
            stackSlots[currentBase + dst] = elems[idx];
            return;
        }
        if (isString(src)) {
            Str s = src.get<Str>();
            if (idx < 0 || idx >= static_cast<Int>(s.size())) {
                std::ostringstream os;
                auto proto = currentFrame->closure->proto;
                os << "  -  Chỉ số vượt quá phạm vi: '" << idx << "'. ";
                os << "  -  Được truy cập trên string: `\n" << _toString(s) << "\n`\n";
                throwVMError(os.str());

            }
            stackSlots[currentBase + dst] = Value(Str(1, s[idx]));
            return;
        }
        if (isMap(src)) {
            Object m = src.get<Object>();
            Str k = _toString(key);
            auto it = m->fields.find(k);
            stackSlots[currentBase + dst] = (it != m->fields.end()) ? it->second : Value(Null{});
            return;
        }
        throwVMError("Numeric index not supported on type '" + _toString(src) + "'");
    }


    Str keyName = isString(key) ? key.get<Str>() : _toString(key);

    if (auto mm = getMagicMethod(src, "__getprop__")) {
        Value res = call(*mm, { Value(keyName) });
        stackSlots[currentBase + dst] = res;
        return;
    }


    if (auto mm2 = getMagicMethod(src, keyName)) {
        stackSlots[currentBase + dst] = *mm2;
        return;
    }


    stackSlots[currentBase + dst] = Value(Null{});
}


void MeowVM::opSetIndex() {
    Int srcReg = currentInst->args[0];
    Int keyReg = currentInst->args[1];
    Int valReg = currentInst->args[2];

    if (currentBase + srcReg >= static_cast<Int>(stackSlots.size()) ||
        currentBase + keyReg >= static_cast<Int>(stackSlots.size()) ||
        currentBase + valReg >= static_cast<Int>(stackSlots.size()))
        throwVMError("SET_INDEX register OOB");

    Value& src = stackSlots[currentBase + srcReg];
    Value& key = stackSlots[currentBase + keyReg];
    Value& val = stackSlots[currentBase + valReg];


    if (auto mm = getMagicMethod(src, "__setindex__")) {
        (void) call(*mm, { key, val });
        return;
    }


    if (isInt(key)) {
        Int idx = _toInt(key);
        if (isVector(src)) {
            Array arr = src.get<Array>();
            if (idx < 0) throwVMError("Invalid index");
            if (idx >= static_cast<Int>(arr->elements.size())) {
                if (idx > 10000000) throwVMError("Index too large");
                arr->elements.resize(static_cast<size_t>(idx + 1));
            }
            arr->elements[static_cast<size_t>(idx)] = val;
            return;
        }
        if (isString(src)) {
            if (!isString(val) || val.get<Str>().empty()) throwVMError("String assign must be non-empty string");
            Str &s = src.get<Str>();
            if (idx < 0 || idx >= static_cast<Int>(s.size())) {
                std::ostringstream os;
                os << "Chỉ số vượt quá phạm vi: '" << idx << "'. ";
                os << "Được truy cập trên string: `\n" << _toString(s) << "\n`";
                throwVMError(os.str());

            }
            s[static_cast<size_t>(idx)] = val.get<Str>()[0];
            return;
        }
        if (isMap(src)) {
            Object m = src.get<Object>();
            Str k = _toString(key);
            m->fields[k] = val;
            return;
        }
        throwVMError("Numeric index not supported on type '" + _toString(src) + "'");
    }


    Str keyName = isString(key) ? key.get<Str>() : _toString(key);
    if (auto mm = getMagicMethod(src, "__setprop__")) {
        (void) call(*mm, { Value(keyName), val });
        return;
    }


    if (isInstance(src)) {
        Instance inst = src.get<Instance>();
        inst->fields[keyName] = val;
        return;
    }
    if (isMap(src)) {
        Object m = src.get<Object>();
        m->fields[keyName] = val;
        return;
    }
    if (isClass(src)) {
        Class cls = src.get<Class>();
        if (!isClosure(val) && !val.is<BoundMethod>()) throwVMError("Method must be closure");
        cls->methods[keyName] = val;
        return;
    }

    throwVMError("SET_INDEX not supported on type '" + _toString(src) + "'");
}

void MeowVM::opGetKeys() {
    Int dst = currentInst->args[0];
    Int srcReg = currentInst->args[1];

    if (currentBase + srcReg >= static_cast<Int>(stackSlots.size())) {
        throwVMError("GET_KEYS register OOB");
    }
    Value& src = stackSlots[currentBase + srcReg];


    Array keysArr = memoryManager->newObject<ObjArray>();

    if (isInstance(src)) {

        Instance inst = src.get<Instance>();
        keysArr->elements.reserve(inst->fields.size());
        for (const auto& pair : inst->fields) {
            keysArr->elements.push_back(Value(pair.first));
        }
    } else if (isMap(src)) {

        Object obj = src.get<Object>();
        keysArr->elements.reserve(obj->fields.size());
        for (const auto& pair : obj->fields) {
            keysArr->elements.push_back(Value(pair.first));
        }
    } else if (isVector(src)) {

        Array arr = src.get<Array>();
        Int size = static_cast<Int>(arr->elements.size());
        keysArr->elements.reserve(size);
        for (Int i = 0; i < size; ++i) {
            keysArr->elements.push_back(Value(i));
        }
    } else if (isString(src)) {

        Str s = src.get<Str>();
        Int size = static_cast<Int>(s.length());
        keysArr->elements.reserve(size);
        for (Int i = 0; i < size; ++i) {
            keysArr->elements.push_back(Value(i));
        }
    }


    stackSlots[currentBase + dst] = Value(keysArr);
}

void MeowVM::opGetValues() {
    Int dst = currentInst->args[0];
    Int srcReg = currentInst->args[1];

    if (currentBase + srcReg >= static_cast<Int>(stackSlots.size())) {
        throwVMError("GET_VALUES register OOB");
    }
    Value& src = stackSlots[currentBase + srcReg];


    Array valueArr = memoryManager->newObject<ObjArray>();

    if (isInstance(src)) {

        Instance inst = src.get<Instance>();
        valueArr->elements.reserve(inst->fields.size());
        for (const auto& pair : inst->fields) {
            valueArr->elements.push_back(pair.second);
        }
    } else if (isMap(src)) {

        Object obj = src.get<Object>();
        valueArr->elements.reserve(obj->fields.size());
        for (const auto& pair : obj->fields) {
            valueArr->elements.push_back(pair.second);
        }
    } else if (isVector(src)) {

        Array arr = src.get<Array>();
        Int size = static_cast<Int>(arr->elements.size());
        valueArr->elements.reserve(size);
        for (const auto& element : arr->elements) {
            valueArr->elements.push_back(element);
        }
    } else if (isString(src)) {

        Str s = src.get<Str>();
        Int size = static_cast<Int>(s.length());
        valueArr->elements.reserve(size);
        for (const auto& c : s) {
            valueArr->elements.push_back(Value(Str(1, c)));
        }
    }
    stackSlots[currentBase + dst] = Value(valueArr);
}