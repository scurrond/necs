#pragma once 

#include <algorithm>
#include <array>
#include <bitset>
#include <cstddef>
#include <functional>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace ecs
{
    // ----------------------------------------------------------------------------
    // Template meta
    // ----------------------------------------------------------------------------

    template <typename T>
    struct empty {};

    template <typename T, typename Tuple>
    struct index_of;

    template <typename T, typename... Ts>
    struct index_of<T, std::tuple<T, Ts...>> : std::integral_constant<size_t, 0> {};

    template <typename T, typename U, typename... Ts>
    struct index_of<T, std::tuple<U, Ts...>> : std::integral_constant<size_t, 1 + index_of<std::remove_const_t<T>, std::tuple<Ts...>>::value> {};

    template <typename Tuple, typename... Ts>
    struct indices_of 
    {
        using type = std::index_sequence<index_of<Ts, Tuple>::value...>;
    };

    template <typename Tuple, typename... Ts>
    using indices_of_t = indices_of<Tuple, Ts...>::type;

    // ----------------------------------------------------------------------------
    // Basic defintions
    // ----------------------------------------------------------------------------

    // Reausable unique id per entity.
    using id = size_t;

    template <size_t N> 
    using bitmask = std::bitset<N>;
    
    // Queueable function.
    using task = std::function<void()>;

    // Item returned by queries.
    template <typename... Cs>
    using item = std::tuple<const id&, Cs&...>;

    // ----------------------------------------------------------------------------
    // Data storage
    // ----------------------------------------------------------------------------

    // Entity location.
    struct entity 
    {
        size_t archetype_index;        // entity's archetype index
        size_t component_index;        // entity's component index
    };

    // Archetype-level storage.
    template <size_t N> 
    struct archetype            
    {   
        size_t end;                     // final valid entity
        size_t total;                   // pool size
        bitmask<N> mask;                // unique archetype mask
        std::vector<id> entities;       // entities in this store at their component index
        std::array<size_t, N> pools;    // indices to component vectors inside their pools
    }; 

    using tasks = std::vector<task>;
    using group = std::vector<size_t>;      // A size_t vector alias containing the store indices matching a query.
    using groups = std::vector<group>;
    using entities = std::vector<entity>;
    template <typename C>
    using pool = std::vector<C>;            // Vector alias, base component store.
    template <typename C>
    using store = std::vector<pool<C>>;     // Stores a 2D vector of pools.
    template <typename... Cs>
    using stores = std::tuple<store<Cs>...>;
    template <size_t N>
    using archetypes = std::vector<archetype<N>>;
    template <size_t N>
    using bitmask_index = std::unordered_map<bitmask<N>, size_t>;

    // ----------------------------------------------------------------------------
    // Query
    // ----------------------------------------------------------------------------
    
    template <typename... Ts>
    struct with {};

    template <typename... Ts>
    struct without {};

    template <typename... Ts>
    struct query_param;

    template <typename... Ws, typename... Wos>
    struct query_param<with<Ws...>, without<Wos...>> {};  

    template <typename... Wos>
    struct query_param<without<Wos...>, with<>>{};  

    template <typename... Ws>
    struct query_param<with<Ws...>, without<>>{};

    template <typename... Ts>
    struct query;

    template <typename C, typename Param>
    struct query<C, Param> {};

    template <typename... Cs, typename Param>
    struct query<std::tuple<Cs...>, Param> {};

    // Key for storing query groups.
    template <size_t N> 
    using query_singature = std::tuple<bitmask<N>, bitmask<N>, bitmask<N>>;

    /**
    * Component iterator.
    * @tparam N The number of components in the system.
    * @tparam Is... Index sequence consiting of the indices of Cs... in the world.
    * @tparam Cs... The components to extract.
    */
    template <size_t N, typename Seq , typename... Cs>
    class query_result
    {
        using q = query_result<N, Seq, Cs...>;

        size_t m_group_index;                           // the index of the group of this query
        groups& m_groups;                               // reference to groups
        entities& m_entities;                           // reference to entities
        archetypes<N>& m_archetypes;                    // reference to archetypes
        std::tuple<store<Cs>&...> m_stores;             // reference to the matching stores

        template <size_t I, typename C>
        C& get_component(const size_t& _component_index, archetype<N>& _archetype)
        {
            store<C>& _store = std::get<store<C>&>(m_stores);

            size_t _pool_index = _archetype.pools.at(I);

            pool<C>& _pool = _store.at(_pool_index);

            return _pool.at(_component_index);
        }

        template <size_t... Is>
        item<Cs...> get_item(const id& _id, const size_t& _component_index,  archetype<N>& _archetype, std::index_sequence<Is...>)
        {
            return std::tie(_id, get_component<Is, Cs>(_component_index, _archetype)...);
        }

        public:

            query_result(size_t _group_index, groups& _groups, entities& _entities, archetypes<N>& _archetypes, std::tuple<store<Cs>&...> _stores)
            : m_group_index(_group_index), m_groups(_groups), m_entities(_entities), m_archetypes(_archetypes), m_stores(_stores) {}

            class iterator
            {   
                q& m_query;                                     // parent query

                group& m_group;                                 // the group of the underlying query
                std::tuple<pool<Cs>*...> m_pools;               // current extracted pools 

                size_t m_current_entity_index = 0;              // current entity in archetype group
                size_t m_current_group_index = 0;               // current archetype index in at this index in m_groups
                int m_prev_valid_group_index = -1;              // keeps track in order to set pools       

                template <typename C>
                C& get_component(size_t& _component_index)
                {
                    return std::get<pool<C>*>(m_pools) -> at(_component_index);
                }

                template <size_t I, typename C>
                pool<C>* get_pool(archetype<N>& _archetype)
                {
                    store<C>& _store = std::get<store<C>&>(m_query.m_stores);

                    size_t _pool_index = _archetype.pools.at(I);

                    pool<C>& _pool = _store.at(_pool_index);

                    return &_pool;
                }

                template <size_t... Is>
                void set_pools(std::index_sequence<Is...>)
                {
                    size_t _archetype_index = m_group.at(m_current_group_index);
                    archetype<N>& _archetype = m_query.m_archetypes.at(_archetype_index);

                    m_pools = std::make_tuple(get_pool<Is, Cs>(_archetype)...);
                }

                void advance()
                {
                    while(m_current_group_index < m_group.size())
                    {
                        size_t& _archetype_index = m_group.at(m_current_group_index);
                        archetype<N>& _archetype = m_query.m_archetypes.at(_archetype_index);

                        if (m_current_entity_index >= _archetype.end) // skips empty as well
                        {
                            m_current_group_index++;       // increase group
                            m_current_entity_index = 0;    // reset entity index
                        }
                        else 
                        {
                            // extract pools
                            if (m_prev_valid_group_index < (int)m_current_group_index)
                            {
                                set_pools(Seq{});
                                m_prev_valid_group_index = (int)m_current_group_index;
                            }

                            break;
                        }
                    }
                }

                public:

                    iterator(q& _query)
                    : m_query(_query), m_group(_query.m_groups.at(_query.m_group_index)) { advance(); }

                    iterator(q& _query, size_t _last_group_index)
                    : m_query(_query), m_group(_query.m_groups.at(_query.m_group_index)), m_current_group_index(_last_group_index) {}

                    bool operator!=(const iterator& _other)
                    {
                        return _other.m_current_group_index != m_current_group_index
                        || _other.m_current_entity_index != m_current_entity_index;
                    }

                    iterator& operator++()
                    {
                        m_current_entity_index++;

                        advance();

                        return *this;
                    }

                    item<Cs...> operator*()
                    {
                        size_t _archetype_index = m_group.at(m_current_group_index);
                        id& _id = m_query.m_archetypes.at(_archetype_index).entities.at(m_current_entity_index);
                        entity& _entity = m_query.m_entities.at(_id);

                        return std::tie<const id, Cs...>(_id, get_component<Cs>(_entity.component_index)...);
                    }
            };

            template <typename Callback>
            void for_each(Callback&& _callback)
            {
                if constexpr (std::is_invocable_v<Callback, item<Cs...>>)
                {
                    group& _group = m_groups.at(m_group_index);

                    for (size_t& _archetype_index : _group)
                    {
                        archetype<N>& _archetype = m_archetypes[_archetype_index];

                        for (auto& _id : _archetype.entities)
                        {
                            entity& _entity = m_entities.at(_id);
                            _callback(get_item(_id, _entity.component_index, _archetype, Seq{}));
                        }
                    }
                }
            }

            iterator begin()
            {
                return iterator(*this);
            }

            iterator end()
            {
                return iterator(*this, m_groups.at(m_group_index).size());
            }
    };
    
    // ----------------------------------------------------------------------------
    // Data access
    // ----------------------------------------------------------------------------

    // Simple object that gives readonly access to data.
    template <size_t N, typename... components>
    struct reader
    {
        const groups& readonly_groups;                           // available query groups
        const entities& readonly_entities;                       // available entities at their id
        const archetypes<N>& readonly_archetypes;                // available archetypes 
        const stores<components...>& readonly_stores;            // available component stores

        const bitmask_index<N>& readonly_group_indices;          // group indices at query bitmask
        const bitmask_index<N>& readonly_archetypes_indices;     // archetype indices at their bitmask
    };

    // Main API.
    template <typename... components>
    class world
    {
        static constexpr size_t N = sizeof...(components);

        template <typename C>
        using store_index = index_of<C, std::tuple<components...>>;
        template <typename... Cs>
        using store_indices = indices_of_t<std::tuple<components...>, Cs...>;

        tasks m_tasks;                             // task queue
        groups m_groups;                           // available query groups
        entities m_entities;                       // available entities at their id
        archetypes<N> m_archetypes;                // available archetypes 
        stores<components...> m_stores;            // available component stores

        bitmask_index<N> m_group_indices;          // group indices at query bitmask
        bitmask_index<N> m_archetype_indices;     // archetype indices at their bitmask

        size_t create_group(bitmask<N>& _query_mask)
        {
            group _group {};

            for (size_t i = 1; i < m_archetypes.size(); i++)
            {
                archetype<N>& _archetype = m_archetypes.at(i);

                if ((_archetype.mask & _query_mask) == _query_mask)
                {
                    _group.push_back(i);
                }
            }

            m_group_indices[_query_mask] = m_groups.size();
            m_groups.push_back(_group);

            return m_groups.size() - 1;
        }

        template <typename C>
        void create_pool(archetype<N>& _archetype)
        {
            size_t _store_index = store_index<C>{};

            if (_archetype.mask.test(_store_index))
            {
                store<C>& _store = std::get<store<C>>(m_stores);

                size_t _pool_index = _store.size();

                // stores index to pool at store / component index
                _archetype.pools[_store_index] = _pool_index;

                // add new pool to store
                _store.push_back({});
            }
        }

        size_t create_archetype(bitmask<N>& _archetype_mask)
        {
            size_t _archetype_index = m_archetypes.size();
            std::array<size_t, N> _pool_indices;
            std::vector<id> _entities;
            m_archetypes.emplace_back(0, 0, _archetype_mask, _entities, _pool_indices);
            m_archetype_indices[_archetype_mask] = _archetype_index;

            archetype<N>& _archetype = m_archetypes.at(_archetype_index);

            (create_pool<components>(_archetype),...);

            add_to_group(_archetype_mask, _archetype_index);

            return _archetype_index;
        }
        
        void add_id(id& _id, archetype<N>& _archetype)
        {
            size_t _archetype_index = m_archetype_indices.at(_archetype.mask);
            size_t _component_index = _archetype.end; // will always be the last valid index + 1

            if (_archetype.end < _archetype.total)
            {
                // index was reused
                _archetype.entities.at(_component_index) = _id;
                _archetype.end++;
            }
            else 
            {
                // new index was created
                _archetype.entities.emplace_back(_id);
                _archetype.end++;
                _archetype.total++;
            } 

            entity& _entity = m_entities.at(_id);   
            _entity.archetype_index = _archetype_index;
            _entity.component_index = _component_index;
        }

        void add_to_group(bitmask<N>& _archetype_mask, size_t& _archetype_index)
        {
            for (auto& [_group_mask, _group_index] : m_group_indices)
            {
                if ((_group_mask & _archetype_mask) == _group_mask)
                {   
                    group& _group = m_groups.at(_group_index);

                    _group.push_back(_archetype_index);
                }
            }
        }

        template <typename C>
        void add_to_pool(archetype<N>& _archetype, C& _component)
        {            
            pool<C>& _pool = get_pool<C>(_archetype);

            if (_archetype.end < _archetype.total)
            {
                // reuse a "dead" index in the pool
                 _pool.at(_archetype.end) = _component;
            }
            else 
            {
                // allocate more memory in each pool
                 _pool.emplace_back(_component);
            } 
        }

        template <typename... Cs>
        size_t get_group()
        {
            bitmask<N> _query_mask = create_bitmask<Cs...>();

            return m_group_indices.contains(_query_mask) 
            ? m_group_indices.at(_query_mask)
            : create_group(_query_mask);
        }

        template <typename C>
        pool<C>& get_pool(archetype<N>& _archetype)
        {
            size_t _store_index = store_index<C>{};
            size_t _pool_index = _archetype.pools[_store_index];

            store<C>& _store = std::get<store<C>>(m_stores);
            return _store.at(_pool_index);
        }

        archetype<N>& get_archetype(bitmask<N>& _archetype_mask)
        {
            size_t _archetype_index = m_archetype_indices.contains(_archetype_mask)
            ? m_archetype_indices.at(_archetype_mask)
            : create_archetype(_archetype_mask);

            return m_archetypes[_archetype_index];
        }

        // Removes an id, swaps and updates the swapped entity.
        void change_id(size_t& _component_index, archetype<N>& _archetype)
        {
            std::swap(_archetype.entities.at(_component_index), _archetype.entities.at(_archetype.end - 1));
            id _swapped_id = _archetype.entities.at(_component_index);
            entity& _swapped_entity = m_entities.at(_swapped_id);
            _swapped_entity.component_index = _component_index;
            _archetype.end--;
        }

        // Copies a component from one pool to the other where applicable.
        template <typename C>
        void change_pool(size_t _component_index, archetype<N>& _current_archetype, archetype<N>& _new_archetype)
        {
            size_t _store_index = store_index<C>{};

            if (_current_archetype.mask.test(_store_index))
            {
                pool<C>& _pool = get_pool<C>(_current_archetype);

                // copy 
                if (_new_archetype.mask.test(_store_index))
                {
                    C& _component = _pool.at(_component_index);

                    add_to_pool(_new_archetype, _component);   
                }
        
                // remove
                std::swap(_pool.at(_component_index), _pool.at(_current_archetype.end - 1));
            }
        }

        template <typename... Cs>
        archetype<N>& change_archetype(id& _id, bool removing_components = false)
        {
            entity& _entity = m_entities.at(_id);

            bitmask<N> _current_mask = m_archetypes.at(_entity.archetype_index).mask;

            bitmask<N> _new_mask = removing_components 
            ? _current_mask & ~create_bitmask<Cs...>() // erase all
            : _current_mask | create_bitmask<Cs...>(); // keep both sets of bits

            archetype<N>& _new_archetype = get_archetype(_new_mask);
            archetype<N>& _current_archetype = m_archetypes.at(_entity.archetype_index);

            (change_pool<components>(_entity.component_index, _current_archetype, _new_archetype),...);

            // update old archetype
            change_id(_entity.component_index, _current_archetype);

            // update new archetype
            add_id(_id, _new_archetype);

            return _new_archetype;
        }

        public:

            bool has_entity(id _id)
            {
                return _id < m_entities.size();
            }

            template <typename... Cs>
            bool has_group()
            {
                bitmask<N> _query_mask = create_bitmask<Cs...>();

                return m_group_indices.contains(_query_mask);
            }

            bool has_group(bitmask<N>&& _query_mask)
            {
                return m_group_indices.contains(_query_mask);
            }

            template <typename... Cs>
            bool has_archetype()
            {
                bitmask<N> _archetype_mask = create_bitmask<Cs...>();

                return m_archetype_indices.contains(_archetype_mask);
            }

            bool has_archetype(bitmask<N>&& _archetype_mask)
            {
                return m_archetype_indices.contains(_archetype_mask);
            }

            template <typename C>
            bool has_component(id _id)
            {
                if (has_entity(_id))
                {
                    entity& _entity = m_entities.at(_id);
                    bitmask<N>& _archetype_mask = m_archetypes.at(_entity.archetype_index).mask;                    
                    return _archetype_mask.test(store_index<C>{});
                }

                return false;
            }

            template <typename... Cs>
            bool has_components(id _id)
            {
                if (has_entity(_id))
                {
                    entity& _entity = m_entities.at(_id);
                    bitmask<N>& _archetype_mask = m_archetypes.at(_entity.archetype_index).mask;                    
                    bitmask<N> _query_mask = create_bitmask<Cs...>();

                    return (_archetype_mask & _query_mask) == _query_mask;
                }

                return false;
            }

            // Creates a reader object giving readonly access to world data.
            auto create_reader()
            {
                return reader<N, components...>{
                    m_groups,
                    m_entities,
                    m_archetypes,
                    m_stores,
                    m_group_indices,
                    m_archetype_indices
                };
            }  

            // Creates a query object for iterating over components.
            template <typename... Cs>
            auto create_query()
            {
                return query_result<N, store_indices<Cs...>, Cs...>(
                    get_group<Cs...>(),
                    m_groups,
                    m_entities,
                    m_archetypes,
                    std::tie(std::get<store<Cs>>(m_stores)...)
                );
            }

            // Creates a bitmask of size N (component count) for the desired component types.
            template <typename... Cs>
            bitmask<N> create_bitmask()
            {
                bitmask<N> _mask;

                if constexpr (sizeof...(Cs) > 0)
                {
                    auto set_bit = [&_mask]<typename C>(empty<C>)
                    {
                        _mask.set(store_index<C>{}, true);
                    };

                    (set_bit(empty<Cs>{}),...);
                }

                return _mask;
            }

            // Allocates a new entity id. Creates and puts it at the 0 archetype or reuses an empty id if available.
            id create_entity()
            {
                bitmask<N> _empty_mask;
                archetype<N>& _empty_archetype = get_archetype(_empty_mask);

                id _id;

                // create or reuse id
                if (_empty_archetype.end > 0)
                {
                    _id = _empty_archetype.entities.at(_empty_archetype.end);
                }
                else 
                {
                    _id = m_entities.size();
                    m_entities.emplace_back(0, 0);
                    add_id(_id, _empty_archetype);
                }

                return _id;
            }

            // Moves entity into empty archetype.
            void destroy_entity(id _id)
            {
                if (has_components(_id))  // ignores empty entities
                {
                    entity& _entity = m_entities.at(_id);
                    archetype<N>& _current_archetype = m_archetypes.at(_entity.archetype_index);
                    bitmask<N> empty_mask{}; // empty archetype
                    archetype<N>& _empty_archetype = get_archetype(empty_mask);

                    // copy nothing (because empty), but still perform the change
                    change_id(_entity.component_index, _current_archetype);
                    add_id(_id, _empty_archetype);
                }
            }

            // Gets a readonly entity struct at the id. Unsafe getter.
            const entity& get_entity(id _id)
            {
                return m_entities.at(_id);
            }

            // Extracts a single component. Unsafe getter, perform check beforehand.
            template <typename C>
            C& get_component(id _id)
            {
                entity& _entity = m_entities.at(_id);
                archetype<N>& _archetype = m_archetypes.at(_entity.archetype_index);
                pool<C>& _pool = get_pool<C>(_archetype);

                return _pool.at(_entity.component_index);
            }

            // Extracts several components. Unsafe getter, perform check beforehand.
            template <typename C, typename... Cs>
            auto get_components(id _id) -> std::tuple<C&, Cs&...>
            {
                return std::tie(get_component<C>(_id), get_component<Cs>(_id)...);
            }

            // Gets a readonly entity struct at the id. Safe getter.
            const entity* try_get_entity(id _id)
            {
                if (has_entity(_id))
                {
                    return &get_entity(_id);
                }
                else   
                {
                    return nullptr;
                }
            }

            // Extracts a single component. Safe getter, returns nullptr if the id is invalid.
            template <typename C>
            C* try_get_component(id _id)
            {
                if (has_component<C>(_id))
                {
                    return &get_component<C>(_id);
                }
                else   
                {
                    return nullptr;
                }
            }

            // Extracts several components. Safe getter, returns nullopt if the id is invalid.
            template <typename C, typename... Cs>
            auto try_get_components(id _id) -> std::optional<std::tuple<C&, Cs&...>>
            {
                if (has_components<C, Cs...>(_id))
                {
                    return get_components<C, Cs...>(_id);
                }
                else   
                {
                    return std::nullopt;
                }
            }

            template <typename C>
            void add_component(id _id, C&& _component)
            {
                if (!has_component<C>(_id))
                {
                    archetype<N>& _new_archetype = change_archetype<C>(_id);
                    add_to_pool(_new_archetype, _component);
                }
            }

            template <typename C, typename... Cs>
            void add_components(id _id, C&& _component, Cs&&... _components)
            {
                if (!has_components<C, Cs...>(_id))
                {
                    archetype<N>& _new_archetype = change_archetype<C, Cs...>(_id);

                    add_to_pool(_new_archetype, _component);
                    (add_to_pool(_new_archetype, _components),...);
                }
            }

            template <typename C>
            void remove_component(id _id)
            {
                if (has_component<C>(_id))
                {
                    change_archetype<C>(_id, true);
                }
            }  

            template <typename C, typename... Cs>
            void remove_components(id _id)
            {
                if (has_components<C, Cs...>(_id))
                {
                    change_archetype<C, Cs...>(_id, true);
                }
            }  

            // Resets all data.
            void clear()
            {
                m_groups = groups{};
                m_entities = entities{};
                m_archetypes = archetypes<N>{};
                m_stores = stores<components...>{};
                m_group_indices = bitmask_index<N>{};
                m_archetype_indices = bitmask_index<N>{};
            }

            // Queues a task to be executed later.
            void queue(task&& _task)
            {
                m_tasks.push_back(_task);
            }

            // Executes all queued tasks.
            void update() 
            {
                for (auto& task : m_tasks)
                {
                    task();
                }

                m_tasks.clear();
            }
    };
};