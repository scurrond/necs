# Overview

**NECS** (Nano ECS) is a single-header Entity-Component-System (ECS) library being developed for C++20. It is designed to be simple and easy to use, with a focus on cache friendliness.

## In this repo

#### Tests

(todo)

#### Benchmarks

(todo)

#### encs.hpp

Library header file.

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
#include "necs.hpp"

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

// ----------------------------------------------------------------------------
// Archetypes
// ----------------------------------------------------------------------------

using Monster = NECS::Entity<Position, Name>;

// ----------------------------------------------------------------------------
// Queries
// ----------------------------------------------------------------------------

using NameQuery = NECS::Query<Name>;
using PositionQuery = NECS::Query<Position>;
using PositionNameQuery = NECS::Query<Position, Name>;

// ----------------------------------------------------------------------------
// Registry data
// ----------------------------------------------------------------------------

using Archetypes = NECS::Data<Monster>;
using Events = NECS::Data<>;
using Singletons = NECS::Data<>;
using Queries = NECS::Data<NameQuery, PositionQuery, PositionNameQuery>;

NECS::Registry<Archetypes, Events, Singletons, Queries> registry;

int main() 
{    
    registry.populate(Monster(), 100);

    for (auto [id, data] : registry.iter<Monster, Name>())
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
    auto& [type, index, state] = registry.info(0);

    // are there any entities of this archetype in living
    registry.is_empty<Monster>();

    // are there any entities of this archetype in sleeping
    registry.is_empty<Monster>(true);

    // is the entity with id 0 a monster
    registry.is_type<Monster>(0);

    // is the entity with id 0 dead
    registry.is_dead(0);
}
```

## Changing state
```cpp
void change()
{
    // Changes data after update is called
    registry.queue(0, NECS::KILL);
    registry.queue(1, NECS::SNOOZE);
    registry.update();

    // Changes data instantly
    registry.execute(1, NECS::WAKE);
    registry.execute(2, NECS::KILL);
    registry.execute(3, NECS::SNOOZE);
}
```

## Single access
```cpp
void access()
{   
    // VIEW returns nullopt if entity is dead or the type is incorrect
    auto [name0] = registry.view<Monster, Name>(0).value();

    // FIND filters and iterates over every archetype, returns a view
    auto [name1] = registry.find<Name>(1).value();

    // REF returns the entire entity related to this id
    auto [pos, name2] = registry.ref(2).get<Monster>();

    // GET panics if the type is incorrect or the entity is DEAD
    auto [name3] =  registry.get<Monster, Name>(3);
}
```

## Iteration
```cpp
void iterate()
{
    // One-time dynamic storage iterator, useful for iterating through a single archetype
    for (auto [id, data] : registry.iter<Monster, Name, Position>())
    {
        auto& [name, position] = data;

        name.value = "New name";
    } 

    // Templated, pre-configured queries that iterate through the whole system
    // It MUST have non-zero amount of storage chunks to iterate through 
    for (auto [id, data] : registry.query<PositionNameQuery>())
    {
        auto& [position, name] = data;

        name.value = "New name";
    }

    // Dynamic alternative to queries, filter and iterates
    registry.for_each<Position, Name>
    ([](NECS::EntityId id, NECS::Data<Position&, Name&> data) {
        auto& [position, name] = data;

        name.value = "New name";
    });
}
```

# Bouncing balls example (using raylib)
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

    auto& listener = registry.listener<SpawnBalls>();

    listener.subscribe(spawn_balls);
    listener.call
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

        registry.listener<SpawnBalls>().call
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

    int counter = 0;

    while (WindowShouldClose() == false)
    {   
        if (counter > 1000)
        {
            auto count = registry.pool_count<Ball>();
            
            if (count > 0)
            {
                EntityId id = GetRandomValue(0, registry.pool_count<Ball>());
                registry.queue(id, KILL);
                registry.update();
            }
            counter = 0;

        }
        else 
        {
            counter++;
        }

        InputSystem();
        MoveSystem();
        DrawSystem();
    }   

    CloseAudioDevice();
    CloseWindow();
}
```
