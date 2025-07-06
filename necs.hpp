#pragma once 

#include <algorithm>
#include <bitset>
#include <cassert>
#include <cstddef>
#include <functional>
#include <optional>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ecs
{
    // ----------------------------------------------------------------------------
    // Basic definitions
    // ----------------------------------------------------------------------------

    using id = size_t;

    using task = std::function<void()>;

    template <size_t N>
    using bitmask = std::bitset<N>;

    template <typename C>
    using pool = std::vector<std::remove_const_t<C>>;

    template <typename C>
    using store = std::vector<pool<C>>;

    template <typename... Cs>
    using extraction = std::tuple<store<Cs>&...>;

    template <typename... Cs>
    using item = std::tuple<const id&, Cs&...>;

    // ----------------------------------------------------------------------------
    // Template meta
    // ----------------------------------------------------------------------------

    template <typename T, typename Tuple>
    struct index_of;

    template <typename T, typename... Ts>
    struct index_of<T, std::tuple<T, Ts...>> : std::integral_constant<size_t, 0> {};

    template <typename T, typename U, typename... Ts>
    struct index_of<T, std::tuple<U, Ts...>> : std::integral_constant<size_t, 1 + index_of<std::remove_const_t<T>, std::tuple<Ts...>>::value> {};

    template <typename Tuple, typename... Ts>
    struct indices_of { using type = std::index_sequence<index_of<Ts, Tuple>::value...>; };

    // ----------------------------------------------------------------------------
    // Tables
    // ----------------------------------------------------------------------------

    template <typename... Cs>
    using component_table = std::tuple<store<Cs>...>; // stores at the component type, pools at the archetype index

    struct entity_table
    {
        size_t size = 0;
        
        std::vector<size_t> component_index;        // stores the location of the entity's components
        std::vector<size_t> archetype_index;        // stores the location of the entity's archetype
    };  

    template <size_t N>
    struct archetype_table
    {      
        size_t size = 0;
     
        std::vector<size_t>          end;              // the last viable (non-dead) index across each pool 
        std::vector<size_t>          total;            // the last initialized index across each pool
        std::vector<bitmask<N>>      mask;             // the bitmask of the archetype
        std::vector<std::vector<id>> entity_ids;       // entities in this archetype at their component index
    };

    template <size_t N>
    struct group_table
    {
        size_t size = 0;

        std::vector<bitmask<N * 2>>      mask;               // the bitmask of the group
        std::vector<std::vector<size_t>> archetype_indices;  // archetype indices matching the group
    };

    // ----------------------------------------------------------------------------
    // Query 
    // ----------------------------------------------------------------------------

    template <typename... Ts>
    struct include {};

    template <typename... Ts>
    struct exclude {};

    template <typename... Ts>
    struct query_filter;

    template <>
    struct query_filter<> { using params = query_filter<include<>, exclude<>>; };

    template <typename... Incs>
    struct query_filter<include<Incs...>> { using params = query_filter<include<Incs...>, exclude<>>; };

    template <typename... Excs>
    struct query_filter<exclude<Excs...>> { using params = query_filter<include<>, exclude<Excs...>>; };

    template <typename... Incs, typename... Excs>
    struct query_filter<include<Incs...>, exclude<Excs...>> { using params = query_filter<include<Incs...>, exclude<Excs...>>; };

    template <size_t N, typename... Cs>
    struct query_data
    {
        size_t                  group_index;       // the query's group index

        group_table<N>&         groups;            // group table reference
        archetype_table<N>&     archetypes;        // archetype table reference
        extraction<Cs...> stores;            // matching component stores
    };

    template <size_t N, typename... Cs>
    class query_range
    {
        query_data<N, Cs...>& m_data;               // reference to the range's result

        size_t m_current_component = 0;             // current entity component index in archetype group
        size_t m_current_archetype = 0;             // current archetype index in at this index in m_data.groups

        void advance()
        {
            std::vector<size_t>& _archetype_indices = m_data.groups.archetype_indices[m_data.group_index];

            while(m_current_archetype < _archetype_indices.size())
            {   
                if (m_current_component >= m_data.archetypes.end[_archetype_indices[m_current_archetype]]) // skips empty as well
                {
                    m_current_archetype++;     
                    m_current_component = 0;    
                }
                else 
                {
                    break;
                }
            }
        }

        public:

            query_range(query_data<N, Cs...>& _data)
            : m_data(_data) { advance(); }

            query_range(query_data<N, Cs...>& _data, size_t&& _last_archetype)
            : m_data(_data), m_current_archetype(_last_archetype) {}

            bool operator!=(const query_range<N, Cs...>& _other)
            {
                return _other.m_current_archetype != m_current_archetype
                || _other.m_current_component != m_current_component;
            }

            auto operator++() -> query_range<N, Cs...>&
            {
                m_current_component++;

                advance();

                return *this;
            }

            auto operator*() -> item<Cs...>
            {
                std::vector<size_t>& _archetype_indices = m_data.groups.archetype_indices[m_data.group_index];

                size_t _archetype_index = _archetype_indices[m_current_archetype];

                id& _id = m_data.archetypes.entity_ids[_archetype_index][m_current_component];

                return std::tie<const id, Cs...>(_id, std::get<store<Cs>&>(m_data.stores)[_archetype_index][m_current_component]...);
            }
    };

    template <size_t N, typename... Cs>
    class query_result
    {
        query_data<N, Cs...> m_data; 

        template <typename C>
        auto get_pool(size_t& _archetype_index) -> pool<C>&
        {
            return std::get<store<C>&>(m_data.stores)[_archetype_index];
        }

        public:

            query_result(query_data<N, Cs...>&& _data)
            : m_data(_data) {}

            auto read() const -> const query_data<N, Cs...>&
            {
                return m_data;
            }

            template <typename Callback>
            void iter(Callback&& _callback) 
            {
                if (std::is_invocable_v<Callback, item<Cs...>>)
                {
                    std::vector<size_t>& _archetype_indices = m_data.groups.archetype_indices[m_data.group_index];

                    for (size_t& _archetype_index : _archetype_indices)
                    {
                        std::vector<id>& _entity_ids = m_data.archetypes.entity_ids[_archetype_index];

                        for (size_t _component_index = 0; _component_index < m_data.archetypes.end[_archetype_index]; _component_index++)
                        {
                            const id& _id = _entity_ids[_component_index];

                            _callback(std::tie(_id, get_pool<Cs>(_archetype_index)[_component_index]...));
                        }
                    }
                }
            }

            auto begin() -> query_range<N, Cs...>
            {
                return query_range<N, Cs...>(m_data);
            }

            auto end() -> query_range<N, Cs...> 
            {
                return query_range<N, Cs...>(m_data, m_data.groups.archetype_indices[m_data.group_index].size());
            }
    };

    // ----------------------------------------------------------------------------
    // World 
    // ----------------------------------------------------------------------------

    struct world_queue 
    {
        size_t end = 0;
        size_t total = 0;

        std::vector<task> tasks;
    };

    template <typename... Cs>
    struct world_data
    {
        static constexpr size_t N = sizeof...(Cs);

        entity_table           entities;
        group_table<N>         groups;
        archetype_table<N>     archetypes;
        component_table<Cs...> components;

        std::unordered_map<bitmask<N>, size_t>     archetype_index_map;
        std::unordered_map<bitmask<N * 2>, size_t> group_index_map;
    };

    // Main API. Exposes functionality for manipulating entities and components. 
    template <typename... Cs>
    struct world
    {
        static constexpr size_t N = sizeof...(Cs);

        template <typename T>
        using store_index = index_of<std::remove_const_t<T>, std::tuple<Cs...>>;        
        template <typename... Ts>
        using store_indices = indices_of<std::tuple<Cs...>, std::remove_const_t<Ts>...>::type;

        world_data<Cs...> m_data;
        world_queue       m_queue;

        template <typename... Ts>
        auto create_bitmask() -> bitmask<N> 
        {
            bitmask<N> _mask;

            (_mask.set(store_index<Ts>{}, true),...);

            return _mask;
        }

        template <typename T>
        auto get_pool(size_t& _archetype_index) -> pool<T>&
        {
            return std::get<store<T>>(m_data.components)[_archetype_index];
        }

        template <typename T>
        auto get_store() -> store<T>&
        {
            return std::get<store<T>>(m_data.components);
        }
        
        template <typename... Ts, typename... Incs, typename... Excs>
        auto get_group(query_filter<include<Incs...>, exclude<Excs...>>) -> size_t
        {   
            bitmask<N> _include_filter = create_bitmask<Ts..., Incs...>();
            bitmask<N> _exclude_filter = create_bitmask<Excs...>();
 
            bitmask<N * 2> _group_mask;

            for (size_t i = 0; i < N; i++)
            {
                if (_include_filter.test(i))
                {
                    _group_mask.set(i, true);
                }

                if (_exclude_filter.test(i))
                {
                    _group_mask.set(i + N, true);
                }
            }

            if (!m_data.group_index_map.contains(_group_mask))
            {
                size_t _group_index = m_data.groups.size;

                m_data.groups.mask.emplace_back(_group_mask);
                m_data.groups.archetype_indices.emplace_back(std::vector<size_t>{});

                m_data.group_index_map[_group_mask] = _group_index;

                m_data.groups.size++;

                for (size_t _archetype_index = 0; _archetype_index < m_data.archetypes.size; _archetype_index++)
                {
                    bitmask<N>& _archetype_mask = m_data.archetypes.mask[_archetype_index];

                    if ((_include_filter & _archetype_mask) == _include_filter && (_exclude_filter & _archetype_mask).none())
                    {
                        std::vector<size_t>& _archetype_indices = m_data.groups.archetype_indices[_group_index];
                        _archetype_indices.emplace_back(_archetype_index);
                    }
                }

                return _group_index;
            }
            else 
            {
                return m_data.group_index_map[_group_mask];
            }
        }  

        auto get_archetype(bitmask<N>& _archetype_mask) -> size_t
        {
            if (!m_data.archetype_index_map.contains(_archetype_mask))
            {
                size_t _archetype_index = m_data.archetypes.size;

                m_data.archetypes.end.emplace_back(0);
                m_data.archetypes.total.emplace_back(0);
                m_data.archetypes.mask.emplace_back(_archetype_mask);
                m_data.archetypes.entity_ids.emplace_back(std::vector<id>{});

                m_data.archetype_index_map[_archetype_mask] = _archetype_index;

                m_data.archetypes.size++;

                // create a pool for each component at the archetype index

                (std::get<store<Cs>>(m_data.components).emplace_back(pool<Cs>()),...);

                // add to group

                add_to_group(_archetype_mask, _archetype_index);

                return _archetype_index;
            }
            else 
            {
                return m_data.archetype_index_map[_archetype_mask];
            }
        }

        template <typename T>
        void change_pool(size_t& _component_index, size_t& _current_archetype_index, size_t& _new_archetype_index)
        {
            size_t _store_index = store_index<T>{};

            bitmask<N>& _current_archetype_mask = m_data.archetypes.mask[_current_archetype_index];
            bitmask<N>& _new_archetype_mask = m_data.archetypes.mask[_new_archetype_index];

            if (_current_archetype_mask.test(_store_index))
            {
                pool<T>& _pool = get_pool<T>(_current_archetype_index);

                if (_new_archetype_mask.test(_store_index))
                {
                    T& _component = _pool[_component_index];

                    add_to_pool(_new_archetype_index, _component);   
                }
        
                std::swap(_pool[_component_index], _pool[m_data.archetypes.end[_current_archetype_index] - 1]);
            }
        }

        template <typename... Ts>
        auto change_archetype(id& _id, bool removing_components = false) -> size_t
        {
            size_t _current_archetype_index = m_data.entities.archetype_index[_id];
            size_t _component_index = m_data.entities.component_index[_id];

            bitmask<N> _current_mask = m_data.archetypes.mask[_current_archetype_index];

            bitmask<N> _new_mask = removing_components 
            ? _current_mask & ~create_bitmask<Ts...>() // erase non-matching
            : _current_mask | create_bitmask<Ts...>(); // keep both sets of bits

            size_t _new_archetype_index = get_archetype(_new_mask);

            (change_pool<Cs>(
                _component_index, 
                _current_archetype_index, 
                _new_archetype_index 
            ),...);

            remove_from_archetype(_id, _current_archetype_index);

            add_to_archetype(_id, _new_archetype_index);

            return _new_archetype_index;
        }

        template <typename T>
        void add_to_pool(size_t& _archetype_index, T& _component) 
        {
            pool<T>& _pool = get_pool<T>(_archetype_index);

            if (m_data.archetypes.end[_archetype_index] < m_data.archetypes.total[_archetype_index])
            {
                // reuse a "dead" index in the pool
                 _pool.at(m_data.archetypes.end[_archetype_index]) = _component;
            }
            else 
            {
                // allocate more memory in each pool
                 _pool.emplace_back(_component);
            } 
        }

        void add_to_group(bitmask<N>& _archetype_mask, size_t& _archetype_index) 
        {
            for (const auto& [_group_mask, _group_index] : m_data.group_index_map)
            {
                bitmask<N> _include_filter;
                bitmask<N> _exclude_filter;

                for (size_t i = 0; i < N; ++i)
                {
                    if (_group_mask.test(i)) _include_filter.set(i);
    
                    if (_group_mask.test(i + N)) _exclude_filter.set(i);
                }

                if ((_include_filter & _archetype_mask) == _include_filter && (_exclude_filter & _archetype_mask).none())
                {
                    std::vector<size_t>& _archetype_indices = m_data.groups.archetype_indices[_group_index];
                    _archetype_indices.emplace_back(_archetype_index);
                }
            }
        }

        void add_to_archetype(id& _id, size_t& _archetype_index)
        {
            size_t& _end = m_data.archetypes.end[_archetype_index];
            size_t& _total = m_data.archetypes.total[_archetype_index];
            std::vector<id>& _entity_ids = m_data.archetypes.entity_ids[_archetype_index];

            size_t _component_index = _end; // will always be the last valid index + 1

            if (_end < _total)
            {
                // index was reused
                _entity_ids[_end] = _id;
                _end++;
            }
            else 
            {
                // new index was created
                _entity_ids.emplace_back(_id);
                _end++;
                _total++;
            } 

            m_data.entities.archetype_index[_id] = _archetype_index;
            m_data.entities.component_index[_id] = _component_index;
        }

        template <typename T>
        void remove_from_pool(size_t& _component_index, size_t& _archetype_index) 
        {
            size_t _store_index = store_index<T>{};

            bitmask<N>& _current_archetype_mask = m_data.archetypes.mask[_archetype_index];

            if (_current_archetype_mask.test(_store_index))
            {
                pool<T>& _pool = get_pool<T>(_archetype_index);
        
                std::swap(_pool[_component_index], _pool[m_data.archetypes.end[_archetype_index] - 1]);
            }
        }
        
        void remove_from_archetype(id& _id, size_t& _archetype_index) 
        {
            std::vector<id>& _entity_ids = m_data.archetypes.entity_ids[_archetype_index];
            size_t& _component_index = m_data.entities.component_index[_id];
            size_t& _end = m_data.archetypes.end[_archetype_index];

            std::swap(_entity_ids[_component_index], _entity_ids[ _end - 1]);
            id _swapped_id = _entity_ids[_component_index];
            m_data.entities.component_index[_swapped_id] = _component_index;
            _end--;
        }

        public:

            bool exists(id _id) 
            {
                return _id < m_data.entities.size;
            }

            bool empty(id _id)
            {
                if (exists(_id))
                {
                    return m_data.entities.archetype_index[_id] == 0;
                }

                return false;
            }

            template <typename T, typename... Ts>
            bool has(id _id) 
            {   
                if (exists(_id))
                {
                    size_t& _archetype_index = m_data.entities.archetype_index[_id];
                    bitmask<N>& _archetype_mask = m_data.archetypes.mask[_archetype_index];

                    if constexpr (sizeof...(Ts) == 0)
                    {               
                        return _archetype_mask.test(store_index<T>());
                    }
                    else 
                    {
                        bitmask<N> _mask = create_bitmask<T, Ts...>();
                        return (_archetype_mask & _mask) == _mask;
                    }
                }

                return false;
            }   

            auto create() -> id 
            {
                bitmask<N> _empty_mask;

                size_t _empty_index = get_archetype(_empty_mask);
                
                size_t& _end = m_data.archetypes.end[_empty_index];

                id _id;

                if (_end > 0)
                {
                    _id = m_data.archetypes.entity_ids[_empty_index][_end - 1];
                }
                else 
                {
                    _id = m_data.entities.size;

                    m_data.entities.component_index.emplace_back(_end);
                    m_data.entities.archetype_index.emplace_back(_empty_index);
                    
                    add_to_archetype(_id, _empty_index);

                    m_data.entities.size++;
                }

                return _id;
            }

            void destroy(id _id) 
            {
                if (!empty(_id))
                {
                    size_t _component_index = m_data.entities.component_index[_id];
                    size_t _archetype_index = m_data.entities.archetype_index[_id];

                    // only non-empty
                    (remove_from_pool<Cs>(_component_index, _archetype_index),...);

                    remove_from_archetype(_component_index, _archetype_index);

                    _archetype_index = 0;

                    add_to_archetype(_id, _archetype_index);
                }
            }

            template <typename T, typename... Ts>
            void add(id _id, T&& _component, Ts&&... _components) 
            {
                if (!has<T, Ts...>(_id))
                {
                    size_t _new_archetype_index = change_archetype<T, Ts...>(_id);

                    add_to_pool<T>(_new_archetype_index, _component);
                    (add_to_pool<Ts>(_new_archetype_index, _components),...);
                }
            }

            template <typename T, typename... Ts>
            void remove(id _id) 
            {
                if (has<T, Ts...>(_id))
                {
                    change_archetype<T, Ts...>(_id, true);
                }
            }

            template <typename T, typename... Ts>
            auto get(id _id) -> std::tuple<T&, Ts&...>
            {
                size_t _component_index = m_data.entities.component_index[_id];
                size_t _archetype_index = m_data.entities.archetype_index[_id];

                return std::tie<T, Ts...>(
                    get_pool<T>(_archetype_index)[_component_index],  
                    get_pool<Ts>(_archetype_index)[_component_index]...
                );
            }

            template <typename T, typename... Ts>
            auto try_get(id _id) -> std::optional<std::tuple<T&, Ts&...>>
            {
                if (has<T, Ts...>(_id))
                {
                    return get<T, Ts...>(_id);
                }

                return std::nullopt;
            }

            template <typename... Ts, typename... Fs>
            auto query(query_filter<Fs...> = query_filter<>{})
            {
                return query_result<N, Ts...>({
                    get_group<Ts...>(typename query_filter<Fs...>::params{}),
                    m_data.groups,
                    m_data.archetypes,
                    std::tie(get_store<Ts>()...)
                });
            }

            template <typename... Ts, typename Callback, typename... Fs>
            void iter(Callback&& _callback, query_filter<Fs...> _filter = query_filter<>{}) 
            {
                if (std::is_invocable_v<Callback, item<Ts...>>)
                {
                    size_t _group_index = get_group<Ts...>(_filter);

                    std::vector<size_t>& _archetype_indices = m_data.groups.archetype_indices[_group_index];

                    for (size_t& _archetype_index : _archetype_indices)
                    {
                        std::vector<id>& _entity_ids = m_data.archetypes.entity_ids[_archetype_index];

                        for (size_t _component_index = 0; _component_index < m_data.archetypes.end[_archetype_index]; _component_index++)
                        {
                            const id& _id = _entity_ids[_component_index];

                            _callback(std::tie(_id, get_pool<Ts>(_archetype_index)[_component_index])...);
                        }
                    }
                }
            }

            void queue(task&& _task) 
            {
                if (m_queue.end < m_queue.total)
                {
                    m_queue.tasks[m_queue.end] = _task;
                }
                else 
                {
                    m_queue.tasks.emplace_back(_task);
                    m_queue.total++;
                }

                m_queue.end++;
            }

            void update() 
            {
                for (task& _task : m_queue.tasks)
                {
                    _task();
                }
                
                m_queue.end = 0;
            }

            auto read() const -> const world_data<Cs...>&
            {
                return m_data;
            }
    };
};