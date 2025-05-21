#include "../model.hpp"

Registry<ArchetypeTypes, QueryTypes, EventTypes> reg;

void test_subscribe()
{
    auto& listener = reg.get_listener<EntityCreated>();

    listener.subscribe
    ([](EntityCreated event){

        Control<Monster> control = reg.get_control<Monster>();

        std::cout << "\n------------------------------------------------\n";
        std::cout << "Created entity:" 
        << "\n Id: " << event.id 
        << "\n Type: " << control.get_type(event.id).name()
        << "\n Index: " << control.get_index(event.id)
        << "\n Alive: " << control.is_alive(event.id);

        if (!control.is_alive(event.id))
        {
            throw std::runtime_error("Incorrect entity state in entity metadata.");
        }

        std::cout << "\n------------------------------------------------\n";
    });
}

template <typename C>
auto test_get(EntityId id) -> Item<C>
{
    Control<Monster> control = reg.get_control<Monster>();

    Option<Item<C>> result = control.get<Monster, C>(id);

    if (!result.has_value())
    {
        throw std::runtime_error("Get failed.");
    }

    return result.value();
}

void test_create()
{
    Control<Monster> control = reg.get_control<Monster>();

    EntityId id = control.create(Monster(0, Position{1, 4}, Health{10}, Detector{5}), true);

    if (control.get_index(id) != 0)
    {
        throw std::runtime_error("Incorrect entity index in entity metadata.");
    }

    if (!control.is_archetype<Monster>(id))
    {
        throw std::runtime_error("Incorrect entity type in entity metadata.");
    }

    if (!control.has_component<Position>(id))
    {
        throw std::runtime_error("Incorrect entity type in entity metadata.");
    }

    std::cout << "------------------------------------------------\n";

    std::cout << "Components:";

    auto [det] = test_get<Detector>(id);
    std::cout << "\n - Detector: target: " << det.target;

    auto [pos] = test_get<Position>(id);
    std::cout << "\n - Pos: x: " << pos.x << " y: " << pos.y;

    auto [health] = test_get<Health>(id);
    std::cout << "\n - Health: " << health.value;

    std::cout << "\n------------------------------------------------\n";
};

void test_query()
{
    auto& query = reg.get_query<QuadQuery>();

    for (auto [id, health, pos, det] : query.iter())
    {
        std::cout << "------------------------------------------------\n";
        std::cout << "Components from query: ";
        std::cout << "\n - Health: " << health.value;
        std::cout << "\n - Position: x: " << pos.x << " y: " << pos.y;
        std::cout << "\n - Detector: target: " << det.target;
        std::cout << "\n------------------------------------------------\n";

        health.value++;
        pos.x++;
    };
}

void test_populate()
{
    Control<Monster> control = reg.get_control<Monster>();

    control.populate(Monster(), 3);
}

void test_remove()
{
    Control<Monster> control = reg.get_control<Monster>();

    control.remove(0);

    control.update();

    if(control.is_alive(0))
    {
        throw std::runtime_error("The entity was not removed properly.");
    }
}

int main()
{
    std::cout << "=== Running tests ===\n";
    test_subscribe();
    test_create();
    test_query();
    test_populate();
    test_query();
    test_remove();
    std::cout << "=== Run succeeded ===\n";

    return 0;
}