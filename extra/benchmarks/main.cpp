#include <chrono>
#include <iostream>

#include "../model.hpp"

size_t entity_count = 0;

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
        for (size_t i = 0; i < entity_count; i++)
        {
            test_world.create(Health{}, Position{}, Detector{});
        }
    }, 1);
}

void benchmark_destroy()
{
    benchmark("Destroy:", [](){
        for (size_t i = 0; i < entity_count; i++)
        {
            test_world.destroy(i);
        }

    }, 1);
}

void benchmark_iter()
{
    benchmark("1-component iter ", [](){
        single_q.iter([](SingleItem item){
            auto [_, pos]  = item;
            pos.x++;
        });
    });

    benchmark("2-component iter ", [](){
        double_q.iter([](DoubleItem item){
            auto [_, pos, health]  = item;
            health.value += pos.x;
        });
    });

    benchmark("3-component iter ", [](){
        triple_q.iter([](TripleItem item){
            auto [_, pos, health, det]  = item;
            pos.x++;
            health.value++;
            det.target++;  
        });
    });
}

void benchmark_query()
{
    benchmark("1-component query ", [](){
        for (auto [_, pos] : single_q)
        {
            pos.x++;
        }
    });

    benchmark("2-component query ", [](){

        for (auto [_, pos, health] : double_q)
        {
            health.value += pos.x;
        }
    });

    benchmark("3-component query ", [](){
        for (auto [_, pos, health, det] : triple_q)
        {
            pos.x++;
            health.value++;
            det.target++;  
        }
    });
}

void benchmark_remove()
{
    benchmark("1-component remove: ", [](){
        for (size_t i = 0; i < entity_count; i++)
        {
            test_world.remove<Health>(i);
        }
    }, 1);

    benchmark("2-component remove: ", [](){
        for (size_t i = 0; i < entity_count; i++)
        {
            test_world.remove<Position, Detector>(i);
        }
    }, 1);
}

void benchmark_add()
{
    benchmark("1-component add: ", [](){
        for (size_t i = 0; i < entity_count; i++)
        {
            test_world.add(i, Sprite{});
        }
    }, 1);

    benchmark("2-component add: ", [](){
        for (size_t i = 0; i < entity_count; i++)
        {
            test_world.add(i, Shape{}, Texture{});
        }
    }, 1);

    benchmark("3-component add: ", [](){
        for (size_t i = 0; i < entity_count; i++)
        {
            test_world.add(i, Scale{}, Rotation{}, Velocity{});
        }
    }, 1);
}

void benchmark_get()
{
    benchmark("1-component get: ", [](){
        for (size_t i = 0; i < entity_count; i++)
        {
            auto [ health ] = test_world.get<Health>(i);
            health.value++;
        }
    });

    benchmark("2-component get: ", [](){
        for (size_t i = 0; i < entity_count; i++)
        {
            auto [ pos, health ] = test_world.get<Position, Health>(i);
            health.value++;
            pos.x++;  
        }
    });

    benchmark("3-component get: ", [](){
        for (size_t i = 0; i < entity_count; i++)
        {
            auto [ pos, health, det ] = test_world.get<Position, Health, Detector>(i);
            health.value++;
            pos.x++;  
            det.target++;      
        }
    });
}

void benchmark_queue()
{
    // 1 task per entity to match benchmarking setup
    benchmark("Queue add 1 component:", [](){
        test_world.queue([](){
            for (size_t i = 0; i < entity_count; i++)
            {
                test_world.add(i, Health{});        
            }
        });

    }, 1);

    benchmark("Update add 1 component:", [](){
        test_world.update();
    }, 1);

    benchmark("Queue add 2 components:", [](){

        test_world.queue([](){
            for (size_t i = 0; i < entity_count; i++)
            {
                test_world.add(i, Position{}, Detector{});        
            }
        });
    }, 1);

    benchmark("Update add 2 components:", [](){
        test_world.update();
    }, 1);
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: benchmarks.exe <int>\n";
        return 1;
    }

    entity_count = std::stoi(argv[1]);

    std::cout << "\n=== Running benchmarks for: " << entity_count << " entities ===";

    test_world.config().max_empty_archetypes = 1000;

    explode_archetypes();

    benchmark_create();
    benchmark_get();
    benchmark_add();
    benchmark_query();
    benchmark_iter();
    benchmark_remove();
    benchmark_destroy();
    benchmark_queue();

    print_metadata();

    std::cout << "\n=== Benchmarks succeeded ===\n";

    return 0;
}
