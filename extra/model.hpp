#pragma once 

#include "../necs.hpp"

using namespace necs;

// COMPONENTS

struct Health { int value; };
struct Detector { int target; };
struct Position { float x; float y; };
struct Name { std::string value; };

// ARCHETYPES

using Monster = Archetype<Position, Health, Detector>;
using Tree = Archetype<Position, Health>;

using ArchetypeTypes = Data<Monster, Tree>;

// QUERIES

using SingleQuery = Query<For<Position>>;
using DoubleQuery = Query<For<Position, Health>>;
using TripleQuery = Query<For<EntityId, Health, const Detector>>;
using QuadQuery = Query<For<EntityId, Health, Position, const Detector>>;

using QueryTypes = Data<SingleQuery, DoubleQuery, TripleQuery, QuadQuery>;

// EVENTS

struct StartGame { int value; };
struct QuitGame { int value; };

using EventTypes = Data<StartGame, QuitGame>;
