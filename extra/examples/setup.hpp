#pragma once 

#include "../model.hpp"

inline ecs::id id;

inline void create()
{   
    // Adds an empty entity to the world
    test_world.create();

    // Adds components to the entity if neither is present
    test_world.add(id, Health{}, Detector{});

    // Adds an entity with Sprite and Position components to the world
    test_world.create(Sprite{}, Position{});

    // Adds 100 entities with Position, Rotation & Scale components to the world
    test_world.populate(100, Position{}, Rotation{}, Scale{});

    // Queues a create call to be executed on update
    test_world.queue([](){
        test_world.create(Sprite{}, Health{});
    });
};

inline void destroy()
{
    // Removes the components from the entity if both are present   
    test_world.remove<Sprite, Rotation>(id);

    // Removes the entity from the system 
    test_world.destroy(id);

    // Queues a destroy call to be executed on update
    test_world.queue([](){
        test_world.destroy(id);
    });

    // Resets world data and task queue
    test_world.reset();
}

inline void check()
{

    // Checks that the id exists and that the version matches
    test_world.is_valid(id);

    // Checks valid status and then entity's alive status
    test_world.is_alive(id);

    // Checks if the entity has all the requested components
    test_world.has<Sprite, Rotation>(id);
}

inline void access()
{   
    // Retrieves components directly, panics, unsafe getter
    auto [ pos ] = test_world.get<Position>(id);
    
    // Retrieves components if they exist, returns nullopt otherwise
    auto [ sprite ] = test_world.try_get<Sprite>(id).value();

    // Gives readonly access to the world's data
    test_world.read(); 
}

inline void query()
{
    // Create a simple query, either when needed (will cache itself) or beforehand
    // Queries will be valid as long as their parent world doesn't copy itself
    auto simple_q = test_world.query<Position, Health, const Sprite>();

    // Create queries with filter params
    test_world.query<Position>(ecs::query_filter<ecs::include<Rotation>>{});
    test_world.query<Position>(ecs::query_filter<ecs::exclude<Sprite>>{});
    test_world.query<Position>(ecs::query_filter<ecs::include<Rotation>, ecs::exclude<Sprite>>{});

    // Gives readonly access to query data
    simple_q.read();

    // More utility methods
    simple_q.has_member(0);
    simple_q.has_entity(0, 0);
    simple_q.has_members();     
    simple_q.has_entities(0);
    simple_q.member_count(); 
    simple_q.entity_count();
    simple_q.entity_count(0);
    
    // Get first item in the query, unsafe getter
    simple_q.first();

    // Get first item in the query, safe getter, returns std::nullopt if query is empty
    simple_q.try_first();

    // Iterate with range loop
    for (auto [id, pos, health, sprite] : simple_q)
    {
        health.value--;
        pos.x++;
    }

    // Define callback arg alias if desired, must be an ecs::item matching the query's signature
    using SimpleItem = ecs::item<Position, Health, const Sprite>;

    // Iterate with callback, 2-4x faster but clunky and cannot be broken
    simple_q.iter([](SimpleItem item) 
    {
        auto [id, pos, health, sprite] = item;

        health.value--;
        pos.x++;
    });
}