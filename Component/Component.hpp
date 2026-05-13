#pragma once

struct Entity;

struct Component
{
    Entity* owner = nullptr;
    virtual void update(float dt) = 0;
    virtual ~Component() = default;
};
