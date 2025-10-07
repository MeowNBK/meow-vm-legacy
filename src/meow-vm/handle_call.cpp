#include "meow_vm.h"

Upvalue MeowVM::captureUpvalue(Int slotIndex) {
    if (slotIndex < 0 || slotIndex >= static_cast<Int>(stackSlots.size()))
        throwVMError("captureUpvalue: slotIndex OOB: " + std::to_string(slotIndex) + " stackSize=" + std::to_string(stackSlots.size()));

    for (auto it = openUpvalues.rbegin(); it != openUpvalues.rend(); ++it) {
        if ((*it)->slotIndex == slotIndex) {
            return *it;
        }

        if ((*it)->slotIndex < slotIndex) {
            break; 
        }
    }

    auto newUv = memoryManager->newObject<ObjUpvalue>(slotIndex);

    auto it = openUpvalues.begin();
    while (it != openUpvalues.end() && (*it)->slotIndex < slotIndex) {
        ++it;
    }
    openUpvalues.insert(it, newUv);
    
    return newUv;
}

void MeowVM::closeUpvalues(Int slotIndex) {
    while (!openUpvalues.empty() && openUpvalues.back()->slotIndex >= slotIndex) {
        auto up = openUpvalues.back();
        up->close(stackSlots[up->slotIndex]);
        openUpvalues.pop_back();
    }
}

void MeowVM::_executeCall(const Value& callee, Int dst, Int argStart, Int argc, Int base) {

    std::vector<Value> args;
    args.reserve(argc);
    for (Int i = 0; i < argc; ++i) {
        args.push_back(stackSlots[base + argStart + i]);
    }

    if (isClosure(callee)) {
        auto closure = callee.get<Function>();
        Int newStart = static_cast<Int>(stackSlots.size());
        Int needed = newStart + closure->proto->numRegisters;
        stackSlots.resize(needed, Value(Null{}));

        if (static_cast<Int>(stackSlots.size()) < needed) {
            throwVMError("stackSlots quá nhỏ khi resize trong _executeCall. "
                        "cần=" + std::to_string(needed) +
                        " đã có=" + std::to_string(stackSlots.size()));
        }

        CallFrame newFrame(closure, newStart, callStack.back().module, 0, dst);
        callStack.push_back(newFrame);

        for (Int i = 0; i < std::min(argc, static_cast<Int>(closure->proto->numRegisters)); ++i) {
            stackSlots[newStart + i] = args[i];
        }
    } else if (isBound(callee)) {
        auto boundMethod = callee.get<BoundMethod>();
        if (!isClosure(boundMethod->callable)) throwVMError("Bound method không chứa một closure có thể gọi được.");
        auto methodClosure = boundMethod->callable;
        Int newStart = static_cast<Int>(stackSlots.size());
        stackSlots.resize(newStart + methodClosure->proto->numRegisters);
        CallFrame newFrame(methodClosure, newStart, callStack.back().module, 0, dst);
        callStack.push_back(newFrame);
        stackSlots[newStart + 0] = Value(boundMethod->receiver);
        for (Int i = 0; i < std::min(argc, static_cast<Int>(methodClosure->proto->numRegisters - 1)); ++i) {
            stackSlots[newStart + 1 + i] = args[i];
        }
    } else if (isClass(callee)) {
        auto klass = callee.get<Class>();
        auto instance = memoryManager->newObject<ObjInstance>(klass);
        if (dst != -1) stackSlots[base + dst] = Value(instance);
        auto it = klass->methods.find("init");
        if (it != klass->methods.end() && isClosure(it->second)) {
            auto boundInit = memoryManager->newObject<ObjBoundMethod>(instance, it->second.get<Function>());
            _executeCall(Value(boundInit), -1, argStart, argc, base);
        }
    } else if (isNative(callee)) {
        auto func = callee.get<NativeFn>();
        Value result = std::visit(
            [&](auto&& func) -> Value {
                using T = std::decay_t<decltype(func)>;
                if constexpr (std::is_same_v<T, NativeFnSimple>) {
                    return func(args);
                } else if constexpr (std::is_same_v<T, NativeFnAdvanced>) {
                    return func(this, args);
                }
                return Value(Null{});
            },
            func
        );
        if (dst != -1) stackSlots[base + dst] = result;
    } else {
        std::ostringstream os;
        os << "Giá trị kiểu '" << _toString(callee) << "' không thể gọi được: '" + _toString(callee) + "' ";
        os << "với các tham số là: ";
        for (const auto& arg : args) {
            os << _toString(arg) << " ";
        }
        os << "\n";
        throwVMError(os.str());
    }
}

Value MeowVM::call(const Value& callee, Arguments args) {
    size_t startCallDepth = callStack.size();

    Int argStartAbs = static_cast<Int>(stackSlots.size());
    stackSlots.resize(argStartAbs + static_cast<Int>(args.size()));
    for (size_t i = 0; i < args.size(); ++i) {
        stackSlots[argStartAbs + static_cast<Int>(i)] = args[i];
    }

    Int dstAbs = static_cast<Int>(stackSlots.size());
    stackSlots.resize(dstAbs + 1);

    Int argStartRel = argStartAbs - currentBase;
    Int dstRel      = dstAbs - currentBase;
    if (argStartRel < 0 || dstRel < -1) {
        throwVMError("Internal error: invalid relative arg/dst in VM::call");
    }

    _executeCall(callee, dstRel, argStartRel, static_cast<Int>(args.size()), currentBase);

    while (callStack.size() > startCallDepth) {

        currentFrame = &callStack.back();
        auto proto = currentFrame->closure->proto;
        currentBase = currentFrame->slotStart;

        if (currentFrame->ip >= static_cast<Int>(proto->code.size())) {

            if (currentFrame->retReg != -1 && callStack.size() > 1) {
                CallFrame& parent = callStack[callStack.size() - 2];
                size_t idx = static_cast<size_t>(parent.slotStart + currentFrame->retReg);
                if (idx < stackSlots.size()) stackSlots[idx] = Value(Null{});
            }
            callStack.pop_back();
            continue;
        }

        try {
            currentInst = &proto->code[currentFrame->ip++];
            (this->*jumpTable[static_cast<Int32>(currentInst->op)])();
        } catch (const VMError& e) {
            _handleRuntimeException(e);
        } catch (const std::exception& e) {
            std::cerr << "🤯 Lỗi C++ không lường trước trong VM::call loop: " << e.what() << std::endl;
            callStack.clear();
        }
    }

    Value result = stackSlots[dstAbs];

    stackSlots.resize(argStartAbs);
    return result;
}