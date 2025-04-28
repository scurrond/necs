#include "../model.hpp"

Registry<Archetypes, Events, Singletons> reg;

void test_subscribe()
{
    reg.subscribe<EntityCreated>
    ([](EntityCreated event){
        auto& info = reg.info(event.id);

        std::cout << "\n------------------------------------------------\n";
        std::cout << "Created entity:" 
        << "\n Id: " << event.id 
        << "\n Type: " << info.type.name()
        << "\n Index: " << info.index
        << "\n State: " << info.state;

        if (info.state != LIVE)
        {
            throw std::runtime_error("Incorrect entity state in entity metadata.");
        }

        std::cout << "\n------------------------------------------------\n";
    });
}

template <typename A, typename... Cs>
auto test_view(EntityId id) -> Data<Cs&...>
{
    View<Cs...> view = reg.view<A, Cs...>(id);

    if (!view.has_value())
    {
        throw std::runtime_error("View failed.");
    }

    return view.value();
}

template <typename A, typename... Cs>
auto test_find(EntityId id) -> Data<Cs&...>
{
    View<Cs...> find = reg.find<Cs...>(id);

    if (!find.has_value())
    {
        throw std::runtime_error("Find failed.");
    }

    return find.value();
}

template <typename A>
auto test_ref(EntityId id)
{
    EntityRef ref = reg.ref(id);

    if (!ref.is_type<A>())
    {
        throw std::runtime_error("Ref with incorrect type.");
    }
    else if (ref.is_empty())
    {
        throw std::runtime_error("Ref empty.");
    }

    return ref.get<A>();
}

void test_create()
{
    EntityId id = reg.create(A3(Health{10}, Position{1, 4}, Name{"First"}));

    auto& info = reg.info(id);

    if (info.index != 0)
    {
        throw std::runtime_error("Incorrect entity index in entity metadata.");
    }
    if (!reg.is_type<A3>(id))
    {
        throw std::runtime_error("Incorrect entity type in entity metadata.");
    }

    std::cout << "------------------------------------------------\n";

    std::cout << "Components:";

    auto [name] = test_view<A3, Name>(id);
    std::cout << "\n - Name: " << name.value;

    auto [pos] = test_find<A3, Position>(id);
    std::cout << "\n - Pos: x: " << pos.x << " y: " << pos.y;

    auto [health] = reg.get<A3, Health>(id);
    std::cout << "\n - Health: " << health.value;

    std::cout << "\n------------------------------------------------\n";
};

void test_query()
{
    for (auto [id, data] : reg.query<Health, Position, Name>())
    {
        auto& [health, pos, name] = data;

        std::cout << "------------------------------------------------\n";
        std::cout << "Components from query: ";
        std::cout << "\n - Health: " << health.value;
        std::cout << "\n - Position: x: " << pos.x << " y: " << pos.y;
        std::cout << "\n - Name: " << name.value;
        std::cout << "\n------------------------------------------------\n";

        health.value++;
        pos.x++;
        name.value = "F";
    };
}

void test_id_locking()
{

}

void test_populate()
{
    reg.populate(A3(), 3);
}

int main()
{
    std::cout << "=== Running tests ===\n";
    test_subscribe();
    test_create();
    test_query();
    test_populate();
    test_query();
    std::cout << "=== Run succeeded ===\n";

    return 0;
}