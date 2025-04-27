#include <chrono>
#include <iostream>

#include "../model.hpp"

Registry<Archetypes, Events, Singletons, Queries> reg;

int entity_count = 100000;

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

void benchmark_for_each()
{
    benchmark("For each name: ", [](){
        reg.for_each<Name>
        ([](EntityId id, Data<Name&> data){
            auto& [name] = data;
            name.value = "F";
        });
    });

    benchmark("For each name & pos: ", [](){
        reg.for_each<Name, Position>
        ([](EntityId id, Data<Name&, Position&> data){
            auto& [name, position] = data;
            name.value = "F";
            position.x++;
        });
    });

    benchmark("For each name, pos & health: ", [](){
        reg.for_each<Name, Position, Health>
        ([](EntityId id, Data<Name&, Position&, Health&> data){
            auto& [name, position, health] = data;
            name.value = "F";
            position.x++;
            health.value++;
        });
    });
}

void benchmark_query()
{
    benchmark("Single query: ", [](){
        for (auto [id, data] : reg.query<SingleQuery>())
        {
            auto& [health] = data;
            health.value++;
        }
    });

    benchmark("Double query: ", [](){
        for (auto [id, data] : reg.query<DoubleQuery>())
        {
            auto& [health, pos] = data;
            health.value++;
            pos.x++;
        }
    });

    benchmark("Triple query: ", [](){
        for (auto [id, data] : reg.query<TripleQuery>())
        {
            auto& [health, pos, name] = data;
            health.value++;
            pos.x++;
            name.value = "F";
        }
    });
}

void benchmark_iter()
{

    benchmark("Single iter: ", [](){
        for (auto [id, data] : reg.iter<A3, Health>())
        {
            auto& [health] = data;
            health.value++;
        }
    });

    benchmark("Double iter: ", [](){
        for (auto [id, data] : reg.iter<A3, Health, Position>())
        {
            auto& [health, pos] = data;
            health.value++;
            pos.x++;
        }
    });

    benchmark("Triple iter: ", [](){
        for (auto [id, data] : reg.iter<A3, Health, Position, Name>())
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

    benchmark("Single get: ", [&ids](){
        for (auto& id : ids)
        {
            auto [health] = reg.get<A3, Health>(id);
            health.value++;
        }
    });

    benchmark("Double get: ", [&ids](){
        for (auto& id : ids)
        {
            auto [health, pos] = reg.get<A3, Health, Position>(id);
            health.value++;
            pos.x++; 
        }
    });

    benchmark("Triple get: ", [&ids](){
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

    benchmark("Single view: ", [&ids](){
        for (auto& id : ids)
        {
            auto view = reg.view<A3, Health>(id);

            auto& [health] = view.value();
            health.value++;
        }
    });

    benchmark("Double view: ", [&ids](){
        for (auto& id : ids)
        {
            auto view = reg.view<A3, Health, Position>(id);

            auto& [health, pos] = view.value();
            health.value++;
            pos.x++; 
        }
    });

    benchmark("Triple view: ", [&ids](){
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

    benchmark("Single find: ", [&ids](){
        for (auto& id : ids)
        {
            auto view = reg.find<Health>(id);

            auto& [health] = view.value();
            health.value++;
        }
    });

    benchmark("Double find: ", [&ids](){
        for (auto& id : ids)
        {
            auto view = reg.find<Health, Position>(id);

            auto& [health, pos] = view.value();
            health.value++;
            pos.x++; 
        }
    });

    benchmark("Triple find: ", [&ids](){
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

void benchmark_ref()
{
    auto& ids = reg.ids<A3>();

    benchmark("Ref: ", [&ids](){
        for (auto& id : ids)
        {
            auto ref = reg.ref(id);

            auto [health, pos, name] = ref.get<A3>();
            health.value++;
            pos.x++; 
            name.value = "F";
        }
    });
}

int main()
{
    std::cout << "=== Running benchmarks ===\n";

    benchmark_create();
    benchmark_for_each();
    benchmark_query();
    benchmark_iter();
    benchmark_get();
    benchmark_view();
    benchmark_find();
    benchmark_ref();

    std::cout << "\n=== Benchmarks succeeded ===\n";

    return 0;
}
