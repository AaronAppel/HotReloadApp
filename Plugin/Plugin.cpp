// #include <iostream>

#include "plugin.h"

#include "../Mirror/MIR_Ids.h"

struct MyStruct
{
    int value = 0;
    int value2 = 0;
    int value3 = 0;
};


MIR_TYPE_ID(0, MyStruct)

MIR_CLASS(MyStruct)
MIR_CLASS_MEMBER_FLAGS(value, 0)
MIR_CLASS_END

int global = 0;
void run() {
    // std::cout << "Running plugin version 1!" << std::endl;
    global++;
}

const Mirror::TypeInfo* myStructTypeInfo()
{
    return Mir::InfoForType<MyStruct>();
}