#include "../model.hpp"
#include <iostream>
#include <stdexcept>

inline EntityId id = 0;

template <typename C>
C& test_get(EntityId id)
{
    auto result = test_world.try_get<C>(id);

    if (!result)
    {
        throw std::runtime_error("Get failed.");
    }

    return std::get<C&>(result.value());
}

void test_create()
{
    id = test_world.create();

    test_world.add(
        id,       
        Position{1, 4}, 
        Health{10}, 
        Detector{5}
    );

    const auto& data = test_world.read();

    std::cout << "------------------------------------------------\n";

    std::cout << "Created entity:";

    std::cout << "\n - Id:  " << id;
    std::cout << "\n - Archetype index:  " << data.entities.archetype_index.at(id);
    std::cout << "\n - Component index:  " << data.entities.component_index.at(id);

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
    test_world.add(id, Name{"HHHH"});

    const auto& data = test_world.read();

    std::cout << "------------------------------------------------\n";

    std::cout << "Added component:";

    std::cout << "\n - Id:  " << id;
    std::cout << "\n - Archetype index:  " << data.entities.archetype_index.at(id);
    std::cout << "\n - Component index:  " << data.entities.component_index.at(id);

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
    test_world.remove<Position>(id);

    const auto& data = test_world.read();

    std::cout << "------------------------------------------------\n";

    std::cout << "Removed component:";

    std::cout << "\n - Id:  " << id;
    std::cout << "\n - Archetype index:  " << data.entities.archetype_index.at(id);
    std::cout << "\n - Component index:  " << data.entities.component_index.at(id);

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
    test_world.destroy(id);

    const auto& data = test_world.read();

    std::cout << "------------------------------------------------\n";

    std::cout << "Destroyed entity:";

    std::cout << "\n - Id:  " << id;
    std::cout << "\n - Archetype index:  " << data.entities.archetype_index.at(id);
    std::cout << "\n - Component index:  " << data.entities.component_index.at(id);

    std::cout << "\n------------------------------------------------\n";
}

void test_query()
{
    const auto& data = test_world.read();

    for (auto [id, pos, health, det] : triple_q)
    {
        std::cout << "------------------------------------------------\n";

        std::cout << "Querying entity:";
        std::cout << "\n - Id:  " << id;
        std::cout << "\n - Archetype index:  " << data.entities.archetype_index.at(id);
        std::cout << "\n - Component index:  " << data.entities.component_index.at(id);
        std::cout << "\nComponents:";
        std::cout << "\n - Health: " << health.value;
        std::cout << "\n - Position: x: " << pos.x << " y: " << pos.y;
        std::cout << "\n - Detector: target: " << det.target;

        std::cout << "\n------------------------------------------------\n";

        health.value++;
        pos.x++;
    };
}

void test_queue()
{
    test_world.queue([](){
        std::cout << "------------------------------------------------\n";

        std::cout << "Executing queue entry";
        std::cout << "\n - Id:  " << id;

        std::cout << "\n------------------------------------------------\n";

        test_create();

        std::cout << "------------------------------------------------\n";

        std::cout << "Executed queue entry";

        std::cout << "\n------------------------------------------------\n";
    });
}

void test_update()
{
    std::cout << "------------------------------------------------\n";

    std::cout << "Executing update";

    std::cout << "\n------------------------------------------------\n";

    test_world.update();

    std::cout << "------------------------------------------------\n";

    std::cout << "Executed update";

    std::cout << "\n------------------------------------------------\n";
}

int main()
{
    std::cout << "=== Running tests ===\n";
    
    test_create();
    test_add();
    test_query();
    test_remove();
    // test_destroy();

    // test_queue();
    // test_update();

    const auto& data = test_world.read();

    for (size_t i = 0; i < data.archetypes.size; i++)
    {
        std::cout << "------------------------------------------------\n";

        std::cout << "Archetype: ";
        std::cout << "\n - Index: " << i;
        std::cout << "\n - End: " << data.archetypes.end.at(i);
        std::cout << "\n - Total: " << data.archetypes.total.at(i);

        std::cout << "\n - Bitmask: ";
        for (size_t j = 0; j < data.archetypes.mask.at(i).size(); j++)
        {
            std::cout << data.archetypes.mask.at(i).test(j);
        }

        std::cout << "\n - Entities: ";
        for (size_t j = 0; j < data.archetypes.end.at(i); j++)
        {
            std::cout << "\n ---- id " << j << ": " << data.archetypes.entity_ids.at(i).at(j);
        }

        std::cout << "\n------------------------------------------------\n";
    }


    std::cout << "=== Run succeeded ===\n";

    return 0;
}