#pragma once

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#error Define EXPORT
#endif

extern "C" EXPORT void run();

// #TODO How to forward declare?
// struct Mirror
// {
//     struct TypeInfo;
// };

#include "../Mirror/MIR_Mirror.h"
extern "C" EXPORT const Mirror::TypeInfo* MyStructTypeInfo();
