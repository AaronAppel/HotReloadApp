#pragma once

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

extern "C" EXPORT void run();

#include "../Mirror/MIR_Mirror.h"
extern "C" EXPORT const Mirror::TypeInfo* myStructTypeInfo();
