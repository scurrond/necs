#pragma once 

#include "../necs.hpp"

template <typename... Cs>
using Item = ecs::item<Cs...>;

using EntityId = ecs::id;

// COMPONENTS

struct Health { int value; };
struct Detector { int target; };
struct Position { float x; float y; };
struct Name { std::string value; };

// GLOBAL WORLD

inline ecs::world<Position, Health, Detector, Name> test_world;

// ITEMS & QUERIES

using SingleItem = Item<Position>;
using DoubleItem = Item<Position, Health>;
using TripleItem = Item<Position, Health, Detector>;

inline auto single_q = test_world.iter<Position>();
inline auto double_q =test_world.iter<Position, Health>();
inline auto triple_q = test_world.iter<Position, Health, Detector>();