// #include <iostream>

#include "Plugin.h"

#include "ForMirror.h"

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

#include "../HotReloadApp.h"

void Startup() { }
void Shutdown() { }

extern "C" EXPORT void run()
{
    if (EntityCount() == 0)
    {
        AddEntity();
        AddEntity();
    }

    for (int i = 0; i < EntityCount(); i++)
    {
        Entity* entity = GetEntity(i);

        // entity->health--;

        entity->health -= 10;
        entity->speed += 0.25f;
    }
}
