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

void create()
{   
    // Adds a single monster to the system
    registry.create(Monster());

    // Adds 100 monsters to the system, calls create under the hood
    registry.populate(Monster(), 100);
};

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

void access()
{   
    // VIEW returns nullopt if entity is dead or the type is incorrect
    auto [name0] = registry.view<Monster, Name>(0).value();

    // GET panics if the type is incorrect or the entity is DEAD
    auto [name3] =  registry.get<Monster, Name>(3);

    // FIND filters and iterates over every archetype, returns a view
    auto [name1] = registry.find<Name>(1).value();
}

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