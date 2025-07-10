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

inline void print_metadata()
{
    []<typename... Cs>(const ecs::world_data<Cs...>& data)
    {
        std::cout << "\n------------------------------------------------"

        << "\n" << "Metadata"
        << "\n - Archetype count: " << data.archetypes.size
        << "\n - Entity count: " << data.entities.size
        << "\n - Group count: " << data.groups.size
        << "\n - Memory usage: " 
        << "\n ---- Groups: "  << ecs::memory_usage(data.groups) / 1000 << " KB"
        << "\n ---- Entities: "  << ecs::memory_usage(data.entities) / 1000  << " KB"
        << "\n ---- Components: "  << ecs::memory_usage<Cs...>(data.components) / 1000  << " KB"
        << "\n ---- Archetypes: "  << ecs::memory_usage(data.archetypes) / 1000  << " KB"
        << "\n ---- Archetype index map: "  << ecs::memory_usage(data.archetype_index_map) / 1000  << " KB"
        << "\n ---- Group index map: "  << ecs::memory_usage(data.group_index_map) / 1000  << " KB"
        << "\n ---- Total: "  << ecs::memory_usage(data) / 1000  << " KB"

        << "\n------------------------------------------------";
    }
    (test_world.read());
}

inline void print_archetypes()
{
    const auto& data = test_world.read();

    for (size_t i = 0; i < data.archetypes.size; i++)
    {
        std::cout << "------------------------------------------------\n";

        std::cout << "Archetype: ";
        std::cout << "\n - Index: " << i;
        std::cout << "\n - End: " << data.archetypes.end.at(i);
        std::cout << "\n - Total: " << data.archetypes.total.at(i);

        std::cout << "\n - Bitmask: ";
        for (size_t j = 0; j < data.archetypes.mask.at(i).size(); j++)
        {
            std::cout << data.archetypes.mask.at(i).test(j);
        }

        std::cout << "\n - Entities: ";
        for (size_t j = 0; j < data.archetypes.end.at(i); j++)
        {
            EntityId id = data.archetypes.entity_ids.at(i).at(j);

            std::cout << "\n ---- Entity " << j << ":";
            std::cout << " id: " << id;
            std::cout << " component index: " << data.entities.component_index.at(id);
        }

        std::cout << "\n - Membership in " << data.archetypes.memberships.at(i).size << " groups: ";
        for (size_t j = 0; j < data.archetypes.memberships.at(i).size; j++)
        {
            std::cout << "\n ---- Group: " << data.archetypes.memberships.at(i).group_index.at(j) 
            << " member index: " <<  data.archetypes.memberships.at(i).member_index.at(j);
        }

        std::cout << "\n------------------------------------------------\n";
    }
}