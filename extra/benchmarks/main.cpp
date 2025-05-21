#include <chrono>
#include <iostream>

#include "../model.hpp"

Registry<Archetypes, Events, Singletons> reg;

int entity_count = 0;

template <typename F>
void benchmark(std::string msg, F func, int iterations = 1000)
{
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) func();
    auto stop = std::chrono::high_resolution_clock::now();
    
    auto total_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start);
    auto avg_duration = total_duration.count() / iterations;
    float avg_per_entity = static_cast<float>(avg_duration) / static_cast<float>(entity_count); 

    std::cout 
    << "\n------------------------------------------------"
    << "\n" << msg
    << "\n - Average duration: " << avg_duration << "ns"
    << "\n - Average per entity: " << avg_per_entity << "ns"
    << "\n - Iterations: " << iterations
    << "\n - Entities: " << entity_count
    << "\n------------------------------------------------";
}

void benchmark_create()
{
    benchmark("Create 3 components: ", [](){
        for (int i = 0; i < entity_count; i++)
        {
            reg.create(A3());
        }
    }, 1);
}

void benchmark_new()
{
    using Monster = necs::Archetype<Position, Health, Detector, int, float>;
    using Tree = necs::Archetype<Position, Health>;

    using SingleQuery = necs::Query<necs::For<Position>>;
    using DoubleQuery = necs::Query<necs::For<Position, Health>>;
    using TripleQuery = necs::Query<necs::For<necs::EntityId, Health, const Detector>>;

    using ArchetypeData = necs::Data<Monster, Tree>;
    using QueryData = necs::Data<SingleQuery, DoubleQuery, TripleQuery>;
    using EventData = necs::Data<>;

    necs::Registry<ArchetypeData, QueryData, EventData> registry;

    necs::Storage<Monster>& monster_storage = registry.get_storage<Monster>();

    benchmark("Create 3 components:", [&monster_storage](){
        for (int i = 0; i < entity_count; i++)
        {
            monster_storage.create(Monster());
        }
    }, 1);

    auto single_iter = monster_storage.iter<Health>();
    auto double_iter = monster_storage.iter<Health, Position>();
    auto triple_iter = monster_storage.iter<Health, Position, Detector>();
    auto quad_iter = monster_storage.iter<necs::EntityId, Health, Position, Detector>();

    benchmark("1-component iter: ", [&single_iter](){
        for (auto [h] : single_iter)
        {
            h.value++;
        }
    });

    benchmark("2-component iter: ", [&double_iter](){
        for (auto [h, p] : double_iter)
        {
            h.value++;
            p.x++;
        }
    });

    benchmark("3-component iter: ", [&triple_iter](){
        for (auto [h, p, d] : triple_iter)
        {
            h.value++;
            p.x++;
            d.target++;
        }
    });

    benchmark("4-component iter: ", [&quad_iter](){
        for (auto [id, h, p, d] : quad_iter)
        {
            h.value++;
            p.x++;
            d.target++;
        }
    });
    
    auto& single_query = registry.get_query<SingleQuery>();
    auto& double_query = registry.get_query<DoubleQuery>();
    auto& triple_query = registry.get_query<TripleQuery>();

    benchmark("1-component query ", [&single_query](){
        for (auto [pos] : single_query.iter())
        {
            pos.x++;
        }
    });

    benchmark("2-component query ", [&double_query](){
        for (auto  [pos, health]  : double_query.iter())
        {
            pos.x++;
            health.value++;
        }
    });

    benchmark("3-component query ", [&triple_query](){
        for (auto [id, health, detector] : triple_query.iter())
        {
            health.value++;
        }
    });
}

void benchmark_query()
{    
    benchmark("1-component query ", [](){
        for (auto [id, data] : reg.query<Health>())
        {
            auto& [health] = data;
            health.value++;
        }
    });

    benchmark("2-component query: ", [](){
        for (auto [id, data] : reg.query<Health, Position>())
        {
            auto& [health, pos] = data;
            health.value++;
            pos.x++;
        }
    });

    benchmark("3-component query: ", [](){
        for (auto [id, data] : reg.query<Health, Position, Name>())
        {
            auto& [health, pos, name] = data;
            health.value++;
            pos.x++;
        }
    });
}

void benchmark_iter()
{
    benchmark("1-component iter: ", [](){
        for (auto [id, data] : reg.query_in<A3, Health>())
        {
            auto& [health] = data;
            health.value++;
        }
    });

    benchmark("2-component iter: ", [](){
        for (auto [id, data] : reg.query_in<A3, Health, Position>())
        {
            auto& [health, pos] = data;
            health.value++;
            pos.x++;
        }
    });

    benchmark("3-component iter: ", [](){
        for (auto [id, data] : reg.query_in<A3, Health, Position, Name>())
        {
            auto& [health, pos, name] = data;
            health.value++;
            pos.x++;
            name.value = "F";
        }
    });
}

void benchmark_get()
{
    auto& ids = reg.ids<A3>();

    benchmark("1-component get: ", [&ids](){
        for (auto& id : ids)
        {
            auto [health] = reg.get<A3, Health>(id);
            health.value++;
        }
    });

    benchmark("2-component get: ", [&ids](){
        for (auto& id : ids)
        {
            auto [health, pos] = reg.get<A3, Health, Position>(id);
            health.value++;
            pos.x++; 
        }
    });

    benchmark("3-component get: ", [&ids](){
        for (auto& id : ids)
        {
            auto [health, pos, name] = reg.get<A3, Health, Position, Name>(id);
            health.value++;
            pos.x++; 
            name.value = "F";
        }
    });
}

void benchmark_view()
{
    auto& ids = reg.ids<A3>();

    benchmark("1-component view: ", [&ids](){
        for (auto& id : ids)
        {
            auto view = reg.view<A3, Health>(id);

            auto& [health] = view.value();
            health.value++;
        }
    });

    benchmark("2-component view: ", [&ids](){
        for (auto& id : ids)
        {
            auto view = reg.view<A3, Health, Position>(id);

            auto& [health, pos] = view.value();
            health.value++;
            pos.x++; 
        }
    });

    benchmark("3-component view: ", [&ids](){
        for (auto& id : ids)
        {
            auto view = reg.view<A3, Health, Position, Name>(id);

            auto& [health, pos, name] = view.value();
            health.value++;
            pos.x++; 
            name.value = "F";
        }
    });
}

void benchmark_find()
{
    auto& ids = reg.ids<A3>();

    benchmark("1-component find: ", [&ids](){
        for (auto& id : ids)
        {
            auto view = reg.find<Health>(id);

            auto& [health] = view.value();
            health.value++;
        }
    });

    benchmark("2-component find: ", [&ids](){
        for (auto& id : ids)
        {
            auto view = reg.find<Health, Position>(id);

            auto& [health, pos] = view.value();
            health.value++;
            pos.x++; 
        }
    });

    benchmark("3-component find: ", [&ids](){
        for (auto& id : ids)
        {
            auto view = reg.find<Health, Position, Name>(id);

            auto& [health, pos, name] = view.value();
            health.value++;
            pos.x++; 
            name.value = "F";
        }
    });
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: benchmarks.exe <int>\n";
        return 1;
    }

    entity_count = std::stoi(argv[1]);

    std::cout << "\n=== Running benchmarks for: " << entity_count << " entities ===";

    benchmark_create();
    benchmark_iter();
    benchmark_query();
    // benchmark_get();
    // benchmark_view();
    // benchmark_find();
 
    benchmark_new();



    std::cout << "\n=== Benchmarks succeeded ===\n";

    return 0;
}
