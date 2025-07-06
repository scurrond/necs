#pragma once 

#include "../necs.hpp"
#include <iostream>

template <typename... Cs>
using Item = ecs::item<Cs...>;
using EntityId = ecs::id;

// COMPONENTS

struct Health { int value; };
struct Detector { int target; };
struct Position { float x; float y; };
struct Name { std::string value; };
struct Sprite {};
struct Collider {};
struct Rotation {};
struct Scale {};
struct Shape {};
struct Gravity {};
struct Velocity {};
struct Damage {};
struct Hurtbox {};
struct Hitbox {};
struct Material {};
struct Texture {};


// GLOBAL WORLD

struct World : public ecs::world<Position, Health, Detector, Name, Sprite, Collider, Rotation, Scale, Shape, Gravity, Velocity, Damage, Hurtbox, Hitbox, Material, Texture> {};
inline World test_world;

// ITEMS & QUERIES

using SingleItem = Item<Position>;
using DoubleItem = Item<const Position, Health>;
using TripleItem = Item<Position, Health, Detector>;

inline auto single_q = test_world.query<Position>();
inline auto double_q = test_world.query<const Position, Health>();
inline auto triple_q = test_world.query<Position, Health, Detector>();

inline void explode_archetypes()
{
    []<typename... Cs>(ecs::world<Cs...>&) 
    {
        using Components = std::tuple<Cs...>;

        constexpr size_t num_components = sizeof...(Cs);

        auto add_entity_with_combination = [&](auto bitset)
        {
            EntityId id = test_world.create();

            // Helper to add only selected components
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                // Only add components where bitset[Is] is true
                ((bitset[Is] ? test_world.add(id, std::decay_t<std::tuple_element_t<Is, Components>>{}) : void()), ...);
            }(std::make_index_sequence<num_components>{});

            test_world.destroy(id);
        };

        const size_t total_combinations = (1 << num_components);
        for (size_t mask = 1; mask < total_combinations; ++mask)
        {
            std::bitset<num_components> bitset(mask);
            add_entity_with_combination(bitset);
        }
    }
    (test_world);

    std::cout << "\nArchetype count: " << test_world.read().archetypes.size << "\n";
}