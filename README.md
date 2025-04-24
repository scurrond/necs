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
- Pool memory only grows by default, with dead entities being swapped to the end to allow for reuse. Pools can be manually trimmed but using trim() to remove dead memory.
- User-defined events + several built-in ones to track changes to data.

# Getting started

## Requirements

- C++20

## Installation

Just drop the `necs.hpp` file into your include path and you are good to go!

```cpp
#include "necs.hpp"
```

## Example

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
// Registry data
// ----------------------------------------------------------------------------

using Archetypes = NECS::Data<Monster>;
using Events = NECS::Data<>;
using Singletons = NECS::Data<>;
using Queries = NECS::Data<>;

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


