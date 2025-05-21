#include <chrono>
#include <iostream>

#include "../model.hpp"

Registry<ArchetypeTypes, QueryTypes, EventTypes> reg;

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
    Control control = reg.get_control();

    benchmark("Create 3 components:", [&control](){
        for (int i = 0; i < entity_count; i++)
        {
            control.create(Monster());
        }
    }, 1);
}

void benchmark_query()
{    
    auto& single_query = reg.get_query<SingleQuery>();
    auto& double_query = reg.get_query<DoubleQuery>();
    auto& triple_query = reg.get_query<TripleQuery>();

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

void benchmark_iter()
{
    Control control = reg.get_control();

    benchmark("1-component iter: ", [&control](){
        for (auto [h] : control.iter<Monster, Health>())
        {
            h.value++;
        }
    });

    benchmark("2-component iter: ", [&control](){
        for (auto [h, p] : control.iter<Monster, Health, Position>())
        {
            h.value++;
            p.x++;
        }
    });

    benchmark("3-component iter: ", [&control](){
        for (auto [h, p, d] : control.iter<Monster, Health, Position, Detector>())
        {
            h.value++;
            p.x++;
            d.target++;
        }
    });

    benchmark("4-component iter: ", [&control](){
        for (auto [id, h, p, d] : control.iter<Monster, EntityId, Health, Position, Detector>())
        {
            h.value++;
            p.x++;
            d.target++;
        }
    });
}

void benchmark_get()
{
    Control control = reg.get_control();

    benchmark("1-component get: ", [&control](){
        for (auto [id] : control.iter<Monster, EntityId>())
        {
            auto [health] = control.get_component<Health>(id).value();
            health.value++;
        }
    });

    benchmark("2-component get: ", [&control](){
        for (auto [id] : control.iter<Monster, EntityId>())
        {
            auto [health] = control.get_component<Health>(id).value();
            auto [pos] = control.get_component<Position>(id).value();

            health.value++;
            pos.x++;         
        }
    });

    benchmark("3-component get: ", [&control](){
        for (auto [id] : control.iter<Monster, EntityId>())
        {
            auto [health] = control.get_component<Health>(id).value();
            auto [pos] = control.get_component<Position>(id).value();
            auto [det] = control.get_component<Detector>(id).value();

            health.value++;
            pos.x++;   
            det.target++;      
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
    benchmark_get();

    std::cout << "\n=== Benchmarks succeeded ===\n";

    return 0;
}
