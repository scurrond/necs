#pragma once 

#include "../../necs.hpp"

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

using Monster = NECS::Data<Position, Name>;

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

void create()
{   
    // Adds a single monster to the system
    registry.create(Monster());

    // Adds 100 monsters to the system, calls create under the hood
    registry.populate(Monster(), 100);
};

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
    registry.is_dead(0);
}

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