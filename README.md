# necs
# One-Header ECS in Modern C++

**NECS** (Nano ECS) is a single-header, cache-friendly Entity-Component-System (ECS) library for C++20. Designed for performance and simplicity, NECS supports use in game engines, simulations, or any architecture needing data-oriented design.

---

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
---

##  Installation

Just drop the `necs.hpp` file into your include path and you are good to go!

```cpp
#include "necs.hpp"
