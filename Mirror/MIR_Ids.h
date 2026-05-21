#pragma once

#include "MIR_Structs.h"

// MIR_NULL_ID or Invalid value is MIR_TYPE_ID_MAX
// MIR_TYPE_ID(MIR_TYPE_ID_MAX - 0, void)

// Void
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 1, void)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 2, void*)

// Mutable primitives
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 3, signed char)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 4, signed short)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 5, signed int)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 6, signed long long)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 7, unsigned char)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 8, unsigned short)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 9, unsigned int)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 10, unsigned long long)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 11, float)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 12, double)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 13, long double)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 14, char)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 15, bool)

// Const primitives
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 16, const signed char)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 17, const signed short)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 18, const signed int)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 19, const signed long long)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 20, const unsigned char)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 21, const unsigned short)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 22, const unsigned int)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 23, const unsigned long long)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 24, const float)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 25, const double)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 26, const long double)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 27, const char)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 28, const bool)

// Mutable pointers
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 29, signed char*)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 30, signed short*)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 31, signed int*)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 32, signed long long*)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 33, unsigned char*)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 34, unsigned short*)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 35, unsigned int*)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 36, unsigned long long*)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 37, float*)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 38, double*)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 39, long double*)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 40, char*)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 41, bool*)

// Const pointers
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 42, const signed char*)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 43, const signed short*)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 44, const signed int*)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 45, const signed long long*)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 46, const unsigned char*)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 47, const unsigned short*)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 48, const unsigned int*)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 49, const unsigned long long*)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 50, const float*)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 51, const double*)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 52, const long double*)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 53, const char*)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 54, const bool*)

// std::string
#include <string>
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 55, std::string)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 56, std::string*)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 57, const std::string)
MIR_TYPE_ID(MIR_TYPE_ID_MAX - 58, const std::string*)

#define MIRROR_USER_TYPE_ID_MAX MIR_TYPE_ID_MAX - 59
