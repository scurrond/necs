#pragma once 

#include "../model.hpp"

using namespace necs;

Registry<ArchetypeTypes, QueryTypes, EventTypes> reg;

int main() 
{    
    auto writer = reg.get_writer();
    auto query = reg.get_query<SingleQuery>();

    writer.populate(Monster(), 100);

    for (auto [pos] : query.iter())
    {
        pos.x--;
    }

    return 0;
}

void create()
{   
    auto writer = reg.get_writer();

    // Adds a single monster to the system
    writer.create(Monster());

    // Adds 100 monsters to the system, calls create under the hood
    writer.populate(Monster(), 100);
};

void events()
{
    auto listener = reg.get_listener<QuitEvent>();

    // Subscribe to events    
    listener.subscribe
    ([](QuitEvent event){
        // do stuff
    });

    // Call events
    listener.call(QuitEvent{});

    // Disable event listener 
    listener.close();

    // Enable event listener
    listener.open();
}

void manage()
{
    auto writer = reg.get_writer();

    // Queue data for removal
    writer.remove(0);

    // Commit changes
    writer.update();

    // Clean up dead memory
    writer.trim<Monster>();
}

void check()
{
    // read-only info struct
    auto reader = reg.get_reader();

    // is the entity with id 0 a monster
    reader.is_type<Monster>(0);

    // is the entity with id 0 alive
    reader.is_alive(0);

    // can the entity's id be reused on death
    reader.is_locked(0);

    // does the entity have a name component
    reader.has_component<Name>(0);

    // does this archetype exist
    reader.has_archetype<Monster>();
}

void access()
{   
    // get a writer for all archetypes 
    auto writer = reg.get_writer();

    // get a writer for specific archetypes 
    auto writer2 = reg.get_writer<Monster, Tree>();

    // GET returns nullopt if entity is dead or the type is incorrect
    auto [p] = writer.get<Monster, Position>(0).value();

    // FIND filters and iterates over every archetype that matches the components
    auto [h] = writer2.find<Health>(1).value();

    // ITER returns a storage iterator over some components
    for (auto [d] : writer.iter<Monster, Detector>())
    {
        // do stuff
    }
}

void query()
{
    // Retrieve a query
    auto query = reg.get_query<QuadQuery>();

    // Iterate with for loop 
    for (auto [id, health, pos, det] : query.iter())
    {
        health.value--;
        pos.x++;
        std::cout << "Target: " << det.target;
    }

    // Iterate with callback
    query.for_each([](Item<EntityId, Health, Position, Detector> item) 
    {
        auto [id, health, pos, det] = item;

        health.value--;
        pos.x++;
        std::cout << "Target: " << det.target;
    });
}