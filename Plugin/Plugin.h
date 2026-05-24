#pragma once

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#error Define EXPORT
#endif

extern "C" EXPORT void OnFirstLoaded();
extern "C" EXPORT void OnPreUnload();
extern "C" EXPORT void OnReloaded();

extern "C" EXPORT void runAlternate();
extern "C" EXPORT void runAlternate2();

// #TODO How to forward declare?
// Mirror needs to un-nest structs, but also need to check include and compile order and dependency flow
// struct Mirror
// {
//     struct TypeInfo;
// };

#include "../Mirror/MIR_Mirror.h"
#include <vector>
extern "C" EXPORT std::vector<const Mirror::TypeInfo*> StructTypeInfos();

struct MyStructAlternate
{
    char value = 0;
    int value2 = 0;
    float value4[7] = { 0 };
};

struct MyStruct
{
    char value = 0;
    int value2 = 0;
    bool value3 = 0;
    float value4[7] = { 0 };
};

