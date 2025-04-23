# Overview
**NECS** (Nano ECS) is a single-header, cache-friendly Entity-Component-System (ECS) library for C++20. Designed for performance and simplicity, NECS supports its use in game engines, simulations, or any architecture needing data-oriented design.

## Design goals

- **Header-only** - the library is a single header and can be dropped into any project.
- **No external dependencies** - uses only the standard library.
- **Templated** - data is automatically sorted in its correct place.
- **Cache-friendly** - all the data is structured inside tuples and vectors, using an SoA approach & allowing for fast, single-component queries.
- **Compile-time filtering** - a templated tuple-based approach allows for guaranteed access to component vectors with no type-checking or casts.
- **Entity lifecycle management** - entities are associated with a state: live, snoozed, sleeping, awake, killed or dead.
- **Reusable memory** - entity ids and their storage locations are reused to prevent reallocations.
- **Event system** - built-in and user-defined event listeners.
- **Debug & Serialization support** - user-defined functions using C++20 concepts.
- **Inheritance free** - type aliases exist for convenience only, no derivation or virtual functions.
- **Centralized** - all actions are performed from the central Registry.

## Architecture 

NECS is designed with the following concepts in mind:
- **Component** - the smallest unit of data, stored in templated vectors.
- **Entity** - an id pointing to a grouping of components at some index of a storage.
- **Archetype** - a representative type of an entity tuple with all unique components.
- **Registry** - the main, centralized API for the entire system.
- **Listener** - a lambda callback function container for templated events.
- **Singleton** - a unique resource / class.

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

