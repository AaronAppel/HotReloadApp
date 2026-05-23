// #include <iostream>

#include "plugin.h"

#include "../Mirror/MIR_Mirror.h"

struct MyStruct
{
    char value = 0;
    int value2 = 0;
};

MIR_TYPE_ID(0, MyStruct)

MIR_CLASS(MyStruct)
// MIR_CLASS_MEMBER_FLAGS(value, 0)
MIR_CLASS_MEMBER(value)
MIR_CLASS_MEMBER(value2)
MIR_CLASS_END

int global = 0;
void run() {
    // std::cout << "Running plugin version 1!" << std::endl;
    global++;
}

const Mirror::TypeInfo* MyStructTypeInfo()
{
    return Mir::InfoForType<MyStruct>();
}