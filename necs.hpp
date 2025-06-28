#pragma once 

#include <algorithm>
#include <bitset>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace ecs
{
// ----------------------------------------------------------------------------
    // Basic definitions & utils
    // ----------------------------------------------------------------------------

    // Reusable unique entity ids.
    using id = size_t;

    // Queueable function.
    using task = std::function<void()>;

    // Bundled component extraction.
    template <typename... Ts>
    using item = std::tuple<const id&, Ts&...>;

    // Vector alias with const removed.
    template <typename C>
    using pool = std::vector<std::remove_const_t<C>>;

    // ----------------------------------------------------------------------------
    // Template meta
    // ----------------------------------------------------------------------------

    template <typename T, typename Tuple>
    struct index_of;

    template <typename T, typename... Ts>
    struct index_of<T, std::tuple<T, Ts...>> : std::integral_constant<size_t, 0> {};

    template <typename T, typename U, typename... Ts>
    struct index_of<T, std::tuple<U, Ts...>> : std::integral_constant<size_t, 1 + index_of<std::remove_const_t<T>, std::tuple<Ts...>>::value> {};

    // ----------------------------------------------------------------------------
    // Data storage
    // ----------------------------------------------------------------------------

    // Sparse-dense component storage.
    template <typename C>
    struct store 
    {
        std::vector<int> sparse;        // component indices at the entity's id  
        std::vector<id> dense;          // entity ids at component indices
        pool<C> component_pool;         // component vector
    };

    struct entity
    {
        bool alive = true;              // entity status
        size_t group;                   // entity's group's index in group vector
        size_t group_index;             // entity's index in group entities
    };

    // Metadata container.
    struct catalog
    {
        std::vector<id> to_reuse;       // ids to reuse
        std::vector<entity> entities;   // instantiated entities
    };

    template <size_t N>
    struct group
    {
        std::vector<id> entities;    // ids in this group
        std::bitset<N> mask;         // component mask
    };

    // ----------------------------------------------------------------------------
    // Iterator
    // ----------------------------------------------------------------------------

    // Component iterator. 
    template <size_t N, typename... Cs>
    class query
    {
        using stores = std::tuple<store<Cs>&...>;       // relevant stores
        using groups = std::vector<group<N>>&;          // groups reference
        using group_indices = std::vector<size_t>&;     // groups to use for this query

        stores m_stores;
        groups m_groups;
        group_indices m_group_indices;

        size_t m_current_entity_index = 0;      // current entity in group
        size_t m_current_group_index = 0;       // current group in groups

        void advance()
        {
            while(m_current_group_index < m_group_indices.size())
            {
                size_t& _group_index = m_group_indices[m_current_group_index];
                group<N>& _group = m_groups[_group_index];

                if (m_current_entity_index >= _group.entities.size()) // skips empty as well
                {

                    m_current_group_index++;       // increase group
                    m_current_entity_index = 0;    // reset entity index
                }
                else 
                {
                    break;
                }
            }
        }

        template <typename C>
        C& get(id& _id)
        {
            auto& _store = std::get<store<C>&>(m_stores);

            return _store.component_pool[_store.sparse[_id]];
        }

        public:
            query(stores _stores, groups _groups, group_indices _indices)
            : m_stores(_stores), m_groups(_groups), m_group_indices(_indices) {}

            template <typename Callback>
            void for_each(Callback&& _callback)
            {
                if constexpr (std::is_invocable_v<Callback, item<Cs...>>)
                {
                    for (auto& _group_index : m_group_indices)
                    {
                        auto& _group = m_groups[_group_index];

                        for (auto& _id : _group.entities)
                        {
                            _callback(item<Cs...>{_id, get<Cs>(_id)...});
                        }
                    }
                }
            }

            bool operator!=(const query<N, Cs...> _other)
            {
                return _other.m_current_group_index != m_current_group_index
                || _other.m_current_entity_index != m_current_entity_index;
            }

            query<N, Cs...>& operator++()
            {
                m_current_entity_index++;

                advance();

                return *this;
            }

            item<Cs...> operator*()
            {
                size_t& _group_index = m_group_indices[m_current_group_index];
                group<N>& _group = m_groups[_group_index];
                id& _id = _group.entities[m_current_entity_index];

                return std::tie<const id, Cs...>(_id, get<Cs>(_id)...);
            }

            query<N, Cs...>& begin()
            {
                m_current_entity_index = 0;
                m_current_group_index = 0;

                advance();

                return *this;
            }

            query<N, Cs...>& end()
            {
                m_current_entity_index = 0;
                m_current_group_index = m_group_indices.size();

                return *this;
            }
    };

    // ----------------------------------------------------------------------------
    // World 
    // ----------------------------------------------------------------------------
    
    template <typename... components>
    class world 
    {
        static constexpr size_t N = sizeof...(components);

        template <typename C>
        struct value {};          // empty struct for template metaprogramming

        using component_mask = std::bitset<N>;

        template <typename C>
        using component_index = index_of<C, std::tuple<components...>>;

        using tasks = std::vector<task>;
        using stores = std::tuple<store<components>...>;
        using groups = std::vector<group<N>>;
        using group_map = std::unordered_map<component_mask, size_t>;
        using query_map = std::unordered_map<component_mask, std::vector<size_t>>;

        tasks m_tasks;          // task queue
        stores m_stores;        // component stores
        groups m_groups;        // vector of groups
        catalog m_catalog;      // entity metadata store
        group_map m_group_map;  // group index at group bitmask
        query_map m_query_map;  // group indices at query bitmask

        void cache_groups(component_mask& _query_mask)
        {
            std::vector<size_t> _query_groups;

            for (size_t i = 0; i < m_groups.size(); i++)
            {
                group<N>& _group = m_groups[i];

                if ((_group.mask & _query_mask) == _query_mask)
                {
                    _query_groups.push_back(i);
                }
            }

            m_query_map[_query_mask] = _query_groups; 
        }

        template <typename... Cs>
        void change_groups(id& _id, bool removing_components = false)
        {
            auto& _entity = m_catalog.entities[_id];
            auto& _current_group = m_groups[_entity.group];

            component_mask& _current_mask = _current_group.mask;
            component_mask _new_mask = removing_components 
            ? _current_mask & ~mask<Cs...>() // erase where not 1 in both sets
            : _current_mask | mask<Cs...>(); // keep both sets of bits

            // change groups if current group does not match
            if (_new_mask != _current_mask) 
            {
                // remove from old group
                remove_from_group(_entity, _current_group);  
                
                if (m_group_map.contains(_new_mask))
                {
                    size_t _group_index = m_group_map.at(_new_mask);
                    group<N>& _group = m_groups[_group_index];

                    _group.entities.push_back(_id);
                    _entity.group = _group_index;
                    _entity.group_index = _group.entities.size() - 1;                
                }
                else 
                {
                    group<N> _new_group;
                    _new_group.mask = _new_mask;

                    _new_group.entities.push_back(_id);

                    _entity.group = m_groups.size();
                    _entity.group_index = _new_group.entities.size() - 1;

                    m_groups.push_back(_new_group);
                    m_group_map[_new_mask] = _entity.group;

                    for (auto& [_query_mask, _indices] : m_query_map)
                    {
                        if ((_query_mask & _new_mask) == _query_mask)
                        {
                            _indices.push_back(_entity.group);
                        }
                    }
                }
            }
        }

        void remove_from_group(entity& _entity, group<N>& _group)
        {
            auto size = _group.entities.size();

            if (size > 0)
            {
                std::swap(_group.entities[_entity.group_index], _group.entities[size - 1]);
                auto& _swapped_entity = m_catalog.entities[_group.entities[_entity.group_index]];
                _swapped_entity.group_index = _entity.group_index;
                _group.entities.pop_back();   
            }
        }

        template <typename C>
        void remove_from_store(id& _id)
        {
            store<C>& _store = std::get<store<C>>(m_stores);

            size_t _pool_index = _store.sparse[_id];
            size_t _pool_end = _store.component_pool.size() - 1;

            std::swap(_store.dense[_pool_index], _store.dense[_pool_end]);
            std::swap(_store.component_pool[_pool_index], _store.component_pool[_pool_end]);

            // update swapped entity
            id& _swapped_id = _store.dense[_pool_index];
            _store.sparse[_swapped_id] = _pool_index;

            _store.dense.pop_back();
            _store.component_pool.pop_back();

            _store.sparse[_id] = -1;
        }

        public:

            // ----------------------------------------------------------------------------
            // utility
            // ----------------------------------------------------------------------------

            template <typename... Cs>
            component_mask mask()
            {
                component_mask _mask;

                auto set_bit = [&_mask]<typename C>(value<C>)
                {
                    const auto _bit_index = component_index<C>{};

                    _mask.set(_bit_index, true);
                };

                (set_bit(value<Cs>{}),...);

                return _mask;
            }

            // ----------------------------------------------------------------------------
            // readonly data access
            // ----------------------------------------------------------------------------

            // Checks that the entity id is a valid index.  
            bool exists(id _id)
            {
                return _id < m_catalog.entities.size();
            }

            // Checks that the entity id is a valid index + 
            bool is_alive(id _id)
            {
                return exists(_id) && m_catalog.entities[_id].alive;
            }

            bool is_empty(id _id)
            {
                return !is_alive(_id) || m_catalog.entities[_id].group == 0;
            }

            template <typename... Cs>
            bool is_cached()
            {
                component_mask _query_mask = mask<Cs...>();

                return m_query_map.contains(_query_mask);
            }

            template <typename C, typename... Cs>
            bool has(id _id)
            {
                if (is_alive(_id))
                {
                    constexpr size_t size = sizeof...(Cs);

                    if constexpr (size > 0) 
                    {
                        component_mask& _group_mask = m_groups[m_catalog.entities[_id].group].mask;
                        component_mask _query_mask = mask<C, Cs...>();

                        return (_group_mask & _query_mask) == _query_mask;
                    }
                    else
                    {
                        component_mask& _group_mask = m_groups[m_catalog.entities[_id].group].mask;
                        const auto _bit = component_index<C>{};
                        return _group_mask.test(_bit);
                    }
                }   

                return false;
            }

            template <typename C>
            const store<C>& read_store()
            {
                return std::get<C>(m_stores);
            }
            
            const entity& read_entity(id _id)
            {
                return m_catalog.entities[_id];
            }

            const catalog& read_catalog()
            {
                return m_catalog;
            }

            const groups& read_groups()
            {
                return m_groups;
            }

            const group_map& read_group_map()
            {
                return m_group_map;
            }

            const query_map& read_query_map()
            {
                return m_query_map;
            }

            template <typename C>
            const std::vector<int>& read_sparse()
            {
                auto& _store = std::get<store<C>>(m_stores);
                return _store.sparse;
            }

            template <typename C>
            const std::vector<id>& read_dense()
            {
                auto& _store = std::get<store<C>>(m_stores);
                return _store.dense;
            }

            template <typename C>
            const std::vector<C>& read_pool()
            {
                auto& _store = std::get<store<C>>(m_stores);
                return _store.component_pool;
            }

            size_t total_count()
            {
                return m_catalog.entities.size();
            }

            template <typename C>
            size_t component_count()
            {
                auto& _store = std::get<store<C>>(m_stores);
                return _store.component_pool.size();
            }   

            // ----------------------------------------------------------------------------
            // data access 
            // ----------------------------------------------------------------------------

            // Creates an empty entity.
            id create() 
            {
                if (m_groups.empty())
                {
                    m_groups.push_back({}); // 0th group always empty, no components
                    m_group_map[component_mask{}] = 0; // 0 at empty mask
                }

                id _id;

                if (m_catalog.to_reuse.size() > 0)
                {
                    _id = m_catalog.to_reuse.back();

                    m_catalog.entities[_id] = {true, 0, 0};
                    m_catalog.to_reuse.pop_back();
                }
                else 
                {
                    m_catalog.entities.emplace_back(true, 0, 0);

                    auto expand_entities = []<typename C>(store<C>& _store)
                    {
                        _store.sparse.push_back(-1);
                    };
                
                    (expand_entities(std::get<store<components>>(m_stores)),...);

                    _id = m_catalog.entities.size() - 1;
                }   

                group<N>& _zero_group = m_groups[0];

                m_catalog.entities[_id].group_index = _zero_group.entities.size();

                _zero_group.entities.push_back(_id);

                return _id;
            }

            // Removes an entity's component and group data.
            void destroy(id _id)
            {
                auto update_store = [this, &_id]<typename C>(value<C>)
                {
                    if (has<C>(_id))
                    {
                        remove_from_store<C>(_id);
                    }
                };

                (update_store(value<components>{}),...);

                entity& _entity = m_catalog.entities[_id];
                group<N>& _group = m_groups[_entity.group];

                remove_from_group(_entity, _group);

                _entity.alive = false;
                _entity.group = 0;

                m_catalog.to_reuse.push_back(_id);
            }

            template <typename... Cs>
            void add(id _id, Cs&&... _components)
            {
                if (!has<Cs...>(_id))
                {
                    change_groups<Cs...>(_id);

                    auto add_to_store = [this, &_id]<typename C>(C&& _component)
                    {
                        store<C>& _store = std::get<store<C>>(m_stores);

                        size_t _pool_index = _store.component_pool.size();

                        _store.sparse[_id] = _pool_index;

                        _store.dense.push_back(_id);
                        _store.component_pool.push_back(_component);
                    };

                   (add_to_store(std::forward<Cs>(_components)),...);
                }
            }

            //Removes components from an entity.
            template <typename C, typename... Cs>
            void remove(id _id) 
            {
                if (has<C, Cs...>(_id))
                {
                    remove_from_store<C>(_id);
                    (remove_from_store<Cs>(_id),...);
                    change_groups<C, Cs...>(_id, true);
                }
            }

            // Unsafe getter, gets a component from an entity. Perform a check first.
            template <typename C>
            C& get(id _id)
            {
                auto& _store = std::get<store<C>>(m_stores);
                return _store.component_pool[_store.sparse[_id]];
            }

            // Safe getter, returns nullptr if the params are invalid.
            template <typename C>
            C* try_get(id _id)
            {
                if (has<C>(_id))
                {
                    return &get<C>(_id);
                }

                return nullptr;
            }

            // Iterates over all the components with the requested components.
            template <typename... Cs>
            query<N, Cs...> iter()
            {
                component_mask _query_mask = mask<Cs...>();

                // cache the query if it doesn't exist
                if (!m_query_map.contains(_query_mask))
                {
                    cache_groups(_query_mask);
                }

                return query<N, Cs...>(
                    std::tie(std::get<store<Cs>>(m_stores)...),     // extract stores
                    m_groups,                                       // groups
                    m_query_map.at(_query_mask)                     // group indices
                );
            }

            // Caches a query.
            template <typename... Cs>
            void cache()
            {
                component_mask _query_mask = mask<Cs...>();

                if (!m_query_map.contains(_query_mask))
                {
                    cache_groups(_query_mask);
                }
            }

            // Queues functions to be executed later.
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