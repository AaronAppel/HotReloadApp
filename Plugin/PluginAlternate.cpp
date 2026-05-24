
#include "Plugin.h"

#include "../HotReloadApp.h"

void runAlternate()
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
