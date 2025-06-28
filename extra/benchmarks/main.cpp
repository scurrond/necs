#include <chrono>
#include <iostream>

#include "../model.hpp"

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
    benchmark("Create 3 components:", [](){
        for (int i = 0; i < entity_count; i++)
        {
            auto id = test_world.create();

            test_world.add(id, Health{}, Position{}, Detector{});
        }
    }, 1);
}

void benchmark_iter()
{
    benchmark("1-component iter ", [](){
        for (auto [_, pos] : single_q)
        {
            pos.x++;
        }
    });

    benchmark("2-component iter ", [](){

        for (auto [_, pos, health] : double_q)
        {
            pos.x++;
            health.value++;
        }
    });

    benchmark("3-component iter ", [](){
        for (auto [_, pos, health, det] : triple_q)
        {
            pos.x++;
            health.value++;
            det.target++;  
        }
    });
}

void benchmark_for_each()
{
    benchmark("1-component for each ", [](){
        single_q.for_each([](SingleItem item)
        {   
            auto [_, pos ] = item;
            pos.x++;
        });
    });

    benchmark("2-component for each ", [](){
        double_q.for_each([](DoubleItem item)
        {   
            auto [_, pos, health ] = item;
            pos.x++;
            health.value++;
        });
    });

    benchmark("3-component for each ", [](){
        triple_q.for_each([](TripleItem item)
        {   
            auto [ _, pos, health, det ] = item;
            pos.x++;
            health.value++;
            det.target++;      
        });
    });
}

void benchmark_get()
{
    benchmark("1-component get: ", [](){
        for (auto id : test_world.read_dense<Health>())
        {
            auto& health = test_world.get<Health>(id);
            health.value++;
        }
    });

    benchmark("2-component get: ", [](){
        for (auto id : test_world.read_dense<Health>())
        {
            auto& health = test_world.get<Health>(id);
            auto& pos = test_world.get<Position>(id);

            health.value++;
            pos.x++;         
        }
    });

    benchmark("3-component get: ", [](){
        for (auto id : test_world.read_dense<Health>())
        {
            auto& health = test_world.get<Health>(id);
            auto& pos = test_world.get<Position>(id);
            auto& det = test_world.get<Detector>(id);


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
    benchmark_for_each();
    benchmark_get();

    std::cout << "\n=== Benchmarks succeeded ===\n";

    return 0;
}
