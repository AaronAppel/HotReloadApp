// #include <iostream>

#include "Plugin.h"

#include "../HotReloadApp.h"

void runAlternate()
{
    for (int i = 0; i < EntityCount(); i++)
    {
        Entity* entity = GetEntity(i);

        entity->alive = entity->health > 0;
    }
}