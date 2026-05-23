#pragma once

#ifdef HOTRELOADAPP_EXPORTS
#define HOTRELOADAPP_API __declspec(dllexport)
#else
#define HOTRELOADAPP_API __declspec(dllimport)
#endif

struct Entity
{
    int health = 100;
    float speed = 1.0f;
    bool alive = true;
};

extern "C"
{
    HOTRELOADAPP_API void AddEntity();

    HOTRELOADAPP_API int EntityCount();

    HOTRELOADAPP_API Entity* GetEntity(int index);
}
