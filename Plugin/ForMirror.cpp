// #include <iostream>

#include "ForMirror.h"

MIR_TYPE(float[7])

#include "Plugin.h"

MIR_CLASS(MyStructAlternate)
MIR_CLASS_MEMBER_FLAGS(value, 0)
MIR_CLASS_MEMBER_FLAGS(value2, 0)
MIR_CLASS_MEMBER_FLAGS(value4, 0)
MIR_CLASS_END

MIR_CLASS(MyStruct)
MIR_CLASS_MEMBER_FLAGS(value, 0)
MIR_CLASS_MEMBER_FLAGS(value2, 0)
MIR_CLASS_MEMBER_FLAGS(value3, 0)
MIR_CLASS_MEMBER_FLAGS(value4, 0)
MIR_CLASS_END

std::vector<const Mirror::TypeInfo*> StructTypeInfos()
{
    return {
        Mir::InfoForType<MyStruct>(),
        Mir::InfoForType<MyStructAlternate>()
    };
}
