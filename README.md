# Overview
**NECS** (Nano ECS) is a single-header Entity-Component-System (ECS) library being developed for C++20. It is designed to be simple and easy to use, with a focus on cache friendliness.

## Hello
Thank you for checking out my repo!

This is a personal project serving my own development and needs as a programmer first. Do let me know about any concerns or bugs regarding my code, I appreaciate any feedback and criticism you have. If you want to give my library a go, keep in mind that this project will change frequently as I expand my knowledge pool.

The workflow that NECS supports relies heavily on knowing exactly what the structure of your data is (no arbitrary archetype-switching or runtime types), which is how I prefer to approach design.

Right now it can be used for smaller projects (I have tested it in simulations and small arcade games), but you can also separate your data across several specific registries like for example UI, render or psysics data. I have been using this approach and for me it works rather well.

A built-in hierarchy system is coming eventually to enable dynamic structures.

## Design goals
- **Header-only** - the library is a single header and can be dropped into any project.
- **No external dependencies** - uses only the standard library.
- **Templated** - data is automatically sorted in its correct place & can be accessed directly.
- **Cache-friendly** - all the data is structured inside tuples and vectors, using an SoA approach & allowing for fast queries.
- **Compile-time filtering** - a templated tuple-based approach allows for guaranteed access to component vectors with no type-checking or casts.
- **Entity lifecycle management** - entities are associated with a state: live, snoozed, sleeping, awake, killed or dead.
- **Reusable memory** - entity ids and their storage locations are reused to prevent reallocations.
- **Event system** - built-in and user-defined event listeners.
- **Debug support** - user-defined functions using C++20 concepts.
- **Inheritance-free** - type aliases exist for convenience only, no derivation or virtual functions.
- **Centralized** - all actions are performed from the central Registry.

## Upcoming


#  Getting started

## Installation
Just drop the `necs.hpp` file into your include path and you are good to go!

```cpp
#include "necs.hpp"
```

## Requirements 
- C++20

## Example
```cpp
#include "necs.hpp"

using Player = NECS::Entity<Position, Health, Input, Name>;

struct Score 
```
```

