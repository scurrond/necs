#include "../model.hpp"
#include <iostream>
#include <stdexcept>

EntityId id; 

template <typename C>
C& test_get(EntityId id)
{
    C* result = test_world.try_get<C>(id);

    if (!result)
    {
        throw std::runtime_error("Get failed.");
    }

    return *result;
}

void test_create()
{
    id = test_world.create();

    auto& entity = test_world.read_entity(id);

    std::cout << "------------------------------------------------\n";

    std::cout << "Created entity:";

    std::cout << "\n - Group:  " << entity.group;
    std::cout << "\n - Group index:  " << entity.group_index;
    std::cout << "\n - Alive:  " << entity.alive;

    std::cout << "\n------------------------------------------------\n";
};

void test_add()
{
    test_world.add(id, Position{1, 4}, Health{10}, Detector{5}, Name{"H"});

    auto& entity = test_world.read_entity(id);

    std::cout << "------------------------------------------------\n";

    std::cout << "Added components:";

    std::cout << "\n - Group:  " << entity.group;
    std::cout << "\n - Group index:  " << entity.group_index;
    std::cout << "\n - Alive:  " << entity.alive;

    auto& det = test_get<Detector>(id);
    std::cout << "\n - Detector: target: " << det.target;

    auto& pos = test_get<Position>(id);
    std::cout << "\n - Pos: x: " << pos.x << " y: " << pos.y;

    auto& health = test_get<Health>(id);
    std::cout << "\n - Health: " << health.value;

    auto& name = test_get<Name>(id);
    std::cout << "\n - Name: " << name.value;

    std::cout << "\n------------------------------------------------\n";
}

void test_remove()
{
    test_world.remove<Position>(id);

    auto& entity = test_world.read_entity(id);

    std::cout << "------------------------------------------------\n";

    std::cout << "Removed component:";

    std::cout << "\n - Group:  " << entity.group;
    std::cout << "\n - Group index:  " << entity.group_index;
    std::cout << "\n - Alive:  " << entity.alive;

    std::cout << "\n------------------------------------------------\n";
}

void test_destroy()
{
    test_world.destroy(id);

    auto& entity = test_world.read_entity(id);

    std::cout << "------------------------------------------------\n";

    std::cout << "Destroyed entity:";

    std::cout << "\n - Group:  " << entity.group;
    std::cout << "\n - Group index:  " << entity.group_index;
    std::cout << "\n - Alive:  " << entity.alive;
    std::cout << "\n - To reuse count:  " << test_world.read_catalog().to_reuse.size();

    std::cout << "\n------------------------------------------------\n";
}

void test_iter()
{
    std::cout << "------------------------------------------------\n";

    std::cout << "Pre-iteration: ";
    std::cout << "\n - Cached: " << test_world.is_cached<Health, Position, Detector>();

    std::cout << "\n------------------------------------------------\n";

    for (auto [id, health, pos, det] : test_world.iter<Health, Position, Detector>())
    {
        std::cout << "------------------------------------------------\n";

        std::cout << "Iterating: ";

        std::cout << "\n - Health: " << health.value;
        std::cout << "\n - Position: x: " << pos.x << " y: " << pos.y;
        std::cout << "\n - Detector: target: " << det.target;

        std::cout << "\n------------------------------------------------\n";

        health.value++;
        pos.x++;
    };

    std::cout << "------------------------------------------------\n";

    std::cout << "Post-iteration: ";
    std::cout << "\n - Cached: " << test_world.is_cached<Health, Position, Detector>();

    std::cout << "\n------------------------------------------------\n";
}

int main()
{
    std::cout << "=== Running tests ===\n";
    
    test_create();
    test_add();
    test_iter();
    test_remove();
    test_destroy();

    std::cout << "=== Run succeeded ===\n";

    return 0;
}