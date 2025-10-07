#include "meow_script.h"
#include <iostream>

Value systemArgv(MeowEngine* engine, [[maybe_unused]] Arguments args) {
    auto argv = engine->getArguments();
    Array arr = new ObjArray();
    for (auto &i : argv) {
        arr->elements.push_back(Value(i));
    }

    return Value(arr);
}

Value systemExit(MeowEngine* engine, Arguments args) {
    Int exitCode = 0;
    if (!args.empty()) {
        auto a = args[0];
        if (a.is<Int>()) {
            exitCode = a.get<Int>();
        }
    }
    std::exit(exitCode);
    return Value(Null{});
}

Value systemExec(Arguments args) {
    const auto& command = args[0].get<Str>();

    int exit_code = std::system(command.c_str());
    return Value((Int)exit_code);
}

Module CreateMeowModule(MeowEngine* engine) {
    MemoryManager* memoryManager = engine->getMemoryManager();
    
    auto sysModule = memoryManager->newObject<ObjModule>("io", "native:system");
    sysModule->exports["argv"] = Value(systemArgv);
    sysModule->exports["exit"] = Value(systemExit);
    sysModule->exports["exec"] = Value(systemExec);

    return sysModule;
}