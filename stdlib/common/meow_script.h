#pragma once

#include "definitions.h"
#include "meow_engine.h"

#if defined(_WIN32)
    #define MeowAPI __declspec(dllexport)
#else
    #define MeowAPI __attribute__((visibility("default")))
#endif

extern "C" MeowAPI Module CreateMeowModule(MeowEngine* engine);
