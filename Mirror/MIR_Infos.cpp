#include "MIR_Mirror.h"

// Void
MIR_TYPE_VOID(void)
MIR_TYPE(void*)

// Mutable
MIR_TYPE(signed char)
MIR_TYPE(signed short)
MIR_TYPE(signed int)
MIR_TYPE(signed long long)
MIR_TYPE(unsigned char)
MIR_TYPE(unsigned short)
MIR_TYPE(unsigned int)
MIR_TYPE(unsigned long long)
MIR_TYPE(float)
MIR_TYPE(double)
MIR_TYPE(long double)
MIR_TYPE(char)
MIR_TYPE(bool)

// Const
MIR_TYPE(const signed char)
MIR_TYPE(const signed short)
MIR_TYPE(const signed int)
MIR_TYPE(const signed long long)
MIR_TYPE(const unsigned char)
MIR_TYPE(const unsigned short)
MIR_TYPE(const unsigned int)
MIR_TYPE(const unsigned long long)
MIR_TYPE(const float)
MIR_TYPE(const double)
MIR_TYPE(const long double)
MIR_TYPE(const char)
MIR_TYPE(const bool)

// Mutable pointers
MIR_TYPE(signed char*)
MIR_TYPE(signed short*)
MIR_TYPE(signed int*)
MIR_TYPE(signed long long*)
MIR_TYPE(unsigned char*)
MIR_TYPE(unsigned short*)
MIR_TYPE(unsigned int*)
MIR_TYPE(unsigned long long*)
MIR_TYPE(float*)
MIR_TYPE(double*)
MIR_TYPE(long double*)
MIR_TYPE(char*)
MIR_TYPE(bool*)

// Const pointers
MIR_TYPE(const signed char*)
MIR_TYPE(const signed short*)
MIR_TYPE(const signed int*)
MIR_TYPE(const signed long long*)
MIR_TYPE(const unsigned char*)
MIR_TYPE(const unsigned short*)
MIR_TYPE(const unsigned int*)
MIR_TYPE(const unsigned long long*)
MIR_TYPE(const float*)
MIR_TYPE(const double*)
MIR_TYPE(const long double*)
MIR_TYPE(const char*)
MIR_TYPE(const bool*)

// String
#include <string>
MIR_TYPE(std::string)
MIR_TYPE(std::string*)
MIR_TYPE(const std::string)
MIR_TYPE(const std::string*)
