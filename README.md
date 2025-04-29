# Overview

**NECS** (Nano ECS) is a single-header Entity-Component-System (ECS) library being developed for C++20. It is designed to be simple, small and easy to use.

## Contents

### encs.hpp

Library header file.

### extra/model.hpp

Data models for testing and benchmarking.

### extra/benchmarks

- Main benchmarking setup. All benchmarks are done on an archetype with 3 components that are being updated with arbitrary data, iterated a 1000 times and then have averages calculated for them. Some benchmarks are missing for snooze, kill and wake operations for updates and executes.
- Build file for benchmarks. Currently only available for windows. You must include an entity count as a console argument when running it.
- Result text files for different operations per entity count.

NOTE:
Currently there is a query overhead for very low entity counts (under 100) of a few 100ns. Queries seem to start performing well at 100+, and well outpacing the for_each iteration method at 1000+ entities. See the relevant files for results.

### extra/tests

Test setup & build file. Not finished.

### extra/examples

Example files of different setups. Not finished.

## Foreword

Hello & thank you for checking out my repo! Let me know about any concerns or bugs regarding my code, I appreaciate any feedback and criticism you have.

If you want to give my library a go, keep in mind that this is a personal project serving my own development and needs as a programmer first, and that the project will change frequently as I expand my knowledge pool.

The workflow that NECS supports relies heavily on knowing exactly what the structure of your data is (no arbitrary archetype-switching or runtime types), which is how I prefer to approach design. A built-in entity hierarchy system is coming eventually to enable dynamic structures.

Currently, NECS supports smaller projects and single-threaded use only. Read: I haven't tested it enough yet.

## Features

- The library is a single header and can be dropped into any project.
- It uses the standard library only.
- Entity data is structured inside tuples and vectors, using an SoA approach & allowing for fast queries. This also allows for compile-time filtering with some template magic.
- Sleep system: entities are associated with a state and, in addition to their archetype storage, are located in one of two pools: living or sleeping. These are iterated through separately.
- Dead entities are not considered during iterations, but their ids (indices) are reused (an id-lock system is coming eventually that will allow for ids to be lifetime-unique).
- Pool memory only grows by default, with dead entities being swapped to the end to allow for reuse. Pools can be manually trimmed by using trim() to remove dead memory.
- User-defined events + several built-in ones to track changes to data.

# Getting started

## Requirements

- C++20

## Installation

Just drop the `necs.hpp` file into your include path and you are good to go!

```cpp
#include "necs.hpp"
```

## Example setup

```cpp
#pragma once

#include "../../necs.hpp"

using namespace NECS;

// ----------------------------------------------------------------------------
// Components
// ----------------------------------------------------------------------------

struct Name
{
    std::string value;
};

struct Position
{
    size_t x;
    size_t y;
};

struct Health
{
    int value;
};

// ----------------------------------------------------------------------------
// Archetypes
// ----------------------------------------------------------------------------

using Monster = Data<Position, Name, Health>;

// ----------------------------------------------------------------------------
// Registry data
// ----------------------------------------------------------------------------

using Archetypes = Data<Monster>;
using Events = Data<>;
using Singletons = Data<>;

Registry<Archetypes, Events, Singletons> registry;

int main()
{
    registry.populate(Monster(), 100);

    for (auto [id, data] : registry.query_in<Monster, Name>())
    {
        auto& [name] = data;

        name.value = "New name";
    }

    return 0;
}
```

## Creating

```cpp
void create()
{
    // Adds a single monster to the system
    registry.create(Monster());

    // Adds 100 monsters to the system, calls create under the hood
    registry.populate(Monster(), 100);
}
```

## Checking

```cpp
void check()
{
    // read-only location info reference for entity with id 0
    auto& [type, index, state, id_lock] = registry.info(0);

    // are there any entities of this archetype in living
    registry.is_empty<Monster>();

    // are there any entities of this archetype in sleeping
    registry.is_empty<Monster>(true);

    // is the entity with id 0 a monster
    registry.is_type<Monster>(0);

    // is the entity with id 0 dead
    registry.is_state(0, DEAD);

    // can the entity's id be reused on death
    registry.is_locked(0);
}
```

## State management

```cpp
void manage()
{
    // Changes data after update is called
    registry.queue(0, KILL);
    registry.queue(1, SNOOZE);
    registry.update();

    // Changes data instantly
    registry.execute(1, WAKE);
    registry.execute(2, KILL);
    registry.execute(3, SNOOZE);
}
```

## Single access

```cpp
void access()
{
    // VIEW returns nullopt if entity is dead or the type is incorrect
    auto [name0] = registry.view<Monster, Name>(0).value();

    // GET panics if the type is incorrect or the entity is DEAD
    auto [name3] =  registry.get<Monster, Name>(3);

    // FIND filters and iterates over every archetype, returns a view
    auto [name1] = registry.find<Name>(1).value();
}
```

## Query

```cpp
void query()
{
    // Query in a single archetype
    auto iterator = registry.query_in<Monster, Name, Position>();

    for (auto [id, data] : iterator)
    {
        auto& [name, position] = data;

        name.value = "New name";
    }

    // Query with different requirements
    Query<Name> query = registry.query<Name>();
    Query<Name> query_with = registry.query_with<Data<Position>, Name>();
    Query<Name> query_without = registry.query_without<Data<Position>, Name>();
    Query<Name> query_with_without = registry.query_with_without<Data<Position>, Data<Health>, Name>();

    // Iterate with for loop
    for (auto [id, data] : query)
    {
        auto& [name] = data;

        name.value = "New name";
    }

    // Iterate with callback
    query.for_each([](Extraction<Name> e)
    {
        auto& [id, data] = e;

        auto& [name] = data;

        name.value = "New name";
    });
}
```

# Bouncing balls example (using raylib, OUTDATED)

```cpp
#include "include/raylib.h"
#include "include/necs.hpp"
#include <cmath>

using namespace NECS;

const int WINDOW_W = 800;
const int WINDOW_H = 800;
const float BALL_SPEED = 300;
const float BALL_RADIUS = 10;

// COMPONENTS

struct Position
{
    float x;
    float y;
};
struct Direction
{
    float x;
    float y;
};

// ARCHETYPES
using Ball = Data<Color, Position, Direction>;

// EVENTS
struct SpawnBalls
{
    int amount;
    Vector2 bounds_max;
    Vector2 bounds_min;
};

// QUERIES
using DrawQuery = Query<Position, Color>;
using MoveQuery = Query<Position, Direction>;

// REGISTRY DATA

using Archetypes = Data<Ball>;
using Events = Data<SpawnBalls>;
using Singletons = Data<>;
using Queries = Data<DrawQuery, MoveQuery>;

Registry<Archetypes, Events, Singletons, Queries> registry;

auto ran_char(int min, int max)
{
    return static_cast<unsigned char>(GetRandomValue(min, max));
}

auto ran_float(float min, int max)
{
    return static_cast<float>(GetRandomValue(static_cast<int>(min), static_cast<int>(max)));
}

auto normalize(float& x, float& y)
{
    if (x != 0 && y != 0)
    {
        auto mag = sqrt(pow(x, 2) + pow(y, 2));

        x = x / mag;
        y = y / mag;
    }
}

void InitSystem()
{
    auto spawn_balls = [](SpawnBalls event)
    {
        auto [amount, bounds_max, bounds_min] = event;

        for (int i = 0; i < amount; i++)
        {
            Color col =
            {
                ran_char(0, 255),
                ran_char(0, 255),
                ran_char(0, 255),
                255
            };
            Position pos =
            {
                ran_float(bounds_min.x, bounds_max.x),
                ran_float(bounds_min.y, bounds_max.y)
            };
            Direction dir =
            {
                ran_float(-100, 100),
                ran_float(-100, 100)
            };
            normalize(dir.x, dir.y);

            registry.create(Ball{col, pos, dir});
        }
    };

    registry.subscribe<SpawnBalls>(spawn_balls);
    registry.call<SpawnBalls>
    ({
        GetRandomValue(10, 20),
        {static_cast<float>(WINDOW_W), static_cast<float>(WINDOW_H)},
        {0, 0}
    });
}

void InputSystem()
{
    auto target = GetMousePosition();

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        int f = 5;
        int x = static_cast<int>(target.x);
        int y = static_cast<int>(target.y);

        Vector2 max =
        {
            static_cast<float>(GetRandomValue(x, y + f)),
            static_cast<float>(GetRandomValue(y, y + f))
        };

        Vector2 min =
        {
            static_cast<float>(GetRandomValue(x - f, x)),
            static_cast<float>(GetRandomValue(y - f, y))
        };

        registry.call<SpawnBalls>
        ({
            GetRandomValue(1, 3),
            max,
            min
        });
    }
}

void MoveSystem()
{
    auto delta = GetFrameTime();

    for (auto [id, data] : registry.query<MoveQuery>())
    {
        auto& [pos, dir] = data;

        pos.x += dir.x * BALL_SPEED * delta;
        pos.y += dir.y * BALL_SPEED * delta;

        if (abs(pos.x) > WINDOW_W || pos.x < 0 )
        {
            dir.x = dir.x * -1;
        }
        if (abs(pos.y) > WINDOW_H || pos.y < 0 )
        {
            dir.y = dir.y * -1;
        }
    }
}

void DrawSystem()
{
    BeginDrawing();

    ClearBackground(RAYWHITE);

    for (auto [id, data] : registry.query<DrawQuery>())
    {
        auto& [pos, col] = data;

        DrawCircle(pos.x, pos.y, BALL_RADIUS, col);
    }

    DrawText(TextFormat("Running at: %i FPS, Ball count %i", GetFPS(), registry.pool_count<Ball>()), 190, 200, 20, LIGHTGRAY);

    EndDrawing();
}

int main ()
{
    InitWindow(WINDOW_W, WINDOW_H, "Balls");
    InitAudioDevice();
    InitSystem();

    while (WindowShouldClose() == false)
    {
        InputSystem();
        MoveSystem();
        DrawSystem();
    }

    CloseAudioDevice();
    CloseWindow();
}
```
