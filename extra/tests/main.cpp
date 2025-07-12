#include "../model.hpp"
#include <iostream>
#include <stdexcept>

inline EntityId id = { 0, 0 };

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

    std::cout << "\n - Id index: " << id.index << ", version: " << id.version;
    std::cout << "\n - Archetype index:  " << data.entities.archetype_index.at(id.index);
    std::cout << "\n - Component index:  " << data.entities.component_index.at(id.index);

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

    std::cout << "\n - Id index: " << id.index << ", version: " << id.version;
    std::cout << "\n - Archetype index:  " << data.entities.archetype_index.at(id.index);
    std::cout << "\n - Component index:  " << data.entities.component_index.at(id.index);

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

    std::cout << "\n - Id index: " << id.index << ", version: " << id.version;
    std::cout << "\n - Archetype index:  " << data.entities.archetype_index.at(id.index);
    std::cout << "\n - Component index:  " << data.entities.component_index.at(id.index);

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

    std::cout << "\n - Id index: " << id.index << ", version: " << id.version;
    std::cout << "\n - Archetype index:  " << data.entities.archetype_index.at(id.index);
    std::cout << "\n - Component index:  " << data.entities.component_index.at(id.index);

    std::cout << "\n------------------------------------------------\n";
}

void test_query()
{
    const auto& data = test_world.read();

    for (auto [id, pos, health, det] : triple_q)
    {
        std::cout << "------------------------------------------------\n";

        std::cout << "Querying entity:";
        std::cout << "\n - Id index: " << id.index << ", version: " << id.version;
        std::cout << "\n - Archetype index:  " << data.entities.archetype_index.at(id.index);
        std::cout << "\n - Component index:  " << data.entities.component_index.at(id.index);
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
        
    //explode_archetypes();

    test_create();
    test_add();
    //test_query();
    test_remove();
    test_destroy();

    test_queue();
    test_update();


    print_archetypes();
    print_metadata();

    std::cout << "\n=== Run succeeded ===\n";

    return 0;
}