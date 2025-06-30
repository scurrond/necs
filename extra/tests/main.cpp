#include "../model.hpp"
#include <iostream>
#include <stdexcept>

EntityId id; 

template <typename C>
C& test_get(EntityId id)
{
    C* result = test_world.try_get_component<C>(id);

    if (!result)
    {
        throw std::runtime_error("Get failed.");
    }

    return *result;
}

void test_create()
{
    id = test_world.create_entity();

    test_world.add_components(
        id,       
        Position{1, 4}, 
        Health{10}, 
        Detector{5}
    );

    auto& entity = test_world.get_entity(id);

    std::cout << "------------------------------------------------\n";

    std::cout << "Created entity:";

    std::cout << "\n - Archetype index:  " << entity.archetype_index;
    std::cout << "\n - Component index:  " << entity.component_index;

    std::cout << "\nComponents:";

    auto& det = test_get<Detector>(id);
    std::cout << "\n - Detector: target: " << det.target;

    auto& pos = test_get<Position>(id);
    std::cout << "\n - Pos: x: " << pos.x << " y: " << pos.y;

    auto& health = test_get<Health>(id);
    std::cout << "\n - Health: " << health.value;

    std::cout << "\n------------------------------------------------\n";
};

void test_add()
{
    test_world.add_component(id, Name{"HHHH"});

    auto& entity = test_world.get_entity(id);

    std::cout << "------------------------------------------------\n";

    std::cout << "Added component:";

    std::cout << "\n - Archetype index:  " << entity.archetype_index;
    std::cout << "\n - Component index:  " << entity.component_index;

    std::cout << "\nComponents:";

    auto& det = test_get<Detector>(id);
    std::cout << "\n - Detector: target: " << det.target;

    auto& pos = test_get<Position>(id);
    std::cout << "\n - Pos: x: " << pos.x << " y: " << pos.y;

    auto& health = test_get<Health>(id);
    std::cout << "\n - Health: " << health.value;

    auto& name = test_get<Name>(id);
    std::cout << "\n - Name: " << name.value;

    std::cout << "\n------------------------------------------------\n";
};

void test_remove()
{
    test_world.remove_component<Position>(id);

    auto& entity = test_world.get_entity(id);

    std::cout << "------------------------------------------------\n";

    std::cout << "Removed component:";

    std::cout << "\n - Archetype index:  " << entity.archetype_index;
    std::cout << "\n - Component index:  " << entity.component_index;

    std::cout << "\nComponents:";

    auto& det = test_get<Detector>(id);
    std::cout << "\n - Detector: target: " << det.target;

    auto& health = test_get<Health>(id);
    std::cout << "\n - Health: " << health.value;

    auto& name = test_get<Name>(id);
    std::cout << "\n - Name: " << name.value;

    std::cout << "\n------------------------------------------------\n";
}

void test_destroy()
{
    test_world.destroy_entity(id);

    auto& entity = test_world.get_entity(id);

    std::cout << "------------------------------------------------\n";

    std::cout << "Destroyed entity:";

    std::cout << "\n - Archetype index:  " << entity.archetype_index;
    std::cout << "\n - Component index:  " << entity.component_index;

    std::cout << "\n------------------------------------------------\n";
}

void test_iter()
{
    for (auto [id, pos, health, det] : triple_q)
    {
        auto& entity = test_world.get_entity(id);

        std::cout << "------------------------------------------------\n";

        std::cout << "Querying entity:";
        std::cout << "\n - Id:  " << id;
        std::cout << "\n - Archetype index:  " << entity.archetype_index;
        std::cout << "\n - Component index:  " << entity.component_index;
        std::cout << "\nComponents:";
        std::cout << "\n - Health: " << health.value;
        std::cout << "\n - Position: x: " << pos.x << " y: " << pos.y;
        std::cout << "\n - Detector: target: " << det.target;

        std::cout << "\n------------------------------------------------\n";

        health.value++;
        pos.x++;
    };
}

int main()
{
    std::cout << "=== Running tests ===\n";
    
    test_create();
    test_create();
    test_create();
    test_create();

    test_add();
    test_iter();
    test_remove();
    test_destroy();

    auto reader = test_world.create_reader();

    for (auto& archetype : reader.readonly_archetypes)
    {   
        std::cout << "------------------------------------------------\n";

        std::cout << "Archetype: ";
        std::cout << "\n - End: " << archetype.end;
        std::cout << "\n - Total: " << archetype.total;

        std::cout << "\n - Bitmask: ";
        for (size_t i = 0; i < archetype.mask.size(); i++)
        {
            std::cout << archetype.mask.test(i);
        }

        std::cout << "\n - Entities: ";
        for (size_t i = 0; i < archetype.end; i++)
        {
            std::cout << "\n ---- Id " << i << ": " << archetype.entities.at(i);
        }

        std::cout << "\n - Pools: ";
        for (size_t i = 0; i < archetype.end; i++)
        {   
            if (archetype.mask.test(i))
            {
                std::cout << "\n ---- Pool index " << i << ": " << archetype.pools.at(i);
            }
        }

        std::cout << "\n------------------------------------------------\n";
    }

    std::cout << "=== Run succeeded ===\n";

    return 0;
}