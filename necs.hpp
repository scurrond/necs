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
#include <unordered_set>
#include <utility>
#include <vector>

namespace ecs
{
    // ----------------------------------------------------------------------------
    // Unique identifiers
    // ----------------------------------------------------------------------------

    struct id 
    {
        size_t index;       // permanent index in a table
        size_t version;     // reuse counter
    };

    // ----------------------------------------------------------------------------
    // Basic definitions
    // ----------------------------------------------------------------------------

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
    // Listener
    // ----------------------------------------------------------------------------

    template <typename... Args>
    struct listener 
    {
        std::function<void(Args...)> callback = [](Args...){};
    };

    // ----------------------------------------------------------------------------
    // Tables
    // ----------------------------------------------------------------------------

    template <typename... Cs>
    using component_table = std::tuple<store<Cs>...>;

    struct entity_table
    {
        size_t size = 0;    // total entities in this table
        
        std::vector<size_t> component_index;        // stores the location of the entity's components
        std::vector<size_t> archetype_index;        // stores the location of the entity's archetype
        std::vector<size_t> version;                // stores the number of reuses of this entity's index
        std::vector<bool>   alive;                  // stores the entity's status

        std::vector<size_t> free;                   // reusable dead entity indices
    };  

    struct member_table
    {
        size_t size = 0;    // total members in this table

        std::vector<size_t> archetype_index;        // the index of the member archetype inside the archetye_table
        std::vector<size_t> membership_index;       // the index of of a membership inside an archetype's membership_table
    };

    struct membership_table
    {
        size_t size = 0;    // total memberships in this table

        std::vector<size_t> group_index;            // the index of a group inside the group_table
        std::vector<size_t> member_index;           // the index of a member inside a group's member_table
    };

    template <size_t N>
    struct archetype_table
    {      
        size_t size = 0;   // total archetypes in this table
     
        std::vector<size_t>                    end;          // the last viable (non-dead) index across each pool 
        std::vector<size_t>                    total;        // the last initialized index across each pool
        std::vector<size_t>                    version;      // the number of times this archetype index has been reused
        std::vector<bool>                      alive;        // the status of the archetype
        std::vector<bitmask<N>>                mask;         // the bitmask of the archetype
        std::vector<std::vector<id>>           entity_ids;   // entities in this archetype at their component index
        std::vector<membership_table>          memberships;  // indices into the member table 

        std::vector<size_t>                    free;         // reusable archetype indices
        std::unordered_map<bitmask<N>, size_t> index;        // indices of all the archetypes in the system
    };

    template <size_t N>
    struct group_table
    {
        size_t size = 0;   // total groups in this table

        std::vector<bitmask<N*2>>                   mask;     // the bitmask of the group
        std::vector<member_table>                   members;  // living archetype indices matching the group
        std::vector<std::unordered_set<bitmask<N>>> cache;    // archetype bitmasks matching each group

        std::unordered_map<bitmask<N*2>, size_t>    index;    // indices of all the groups in the system
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
        extraction<Cs...>       stores;            // matching component stores
    };

    template <size_t N, typename... Cs>
    class query_range
    {
        query_data<N, Cs...>& m_data;               // reference to the range's result

        size_t m_current_entity = 0;             // current entity component index in archetype group
        size_t m_current_member = 0;             // current archetype index in at this index in m_data.groups

        void advance()
        {
            member_table& _members = m_data.groups.members[m_data.group_index];

            while(m_current_member < _members.size)
            {   
                if (m_current_entity >= m_data.archetypes.end[_members.archetype_index[m_current_member]]) // skips empty as well
                {
                    m_current_member++;     
                    m_current_entity = 0;    
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

            query_range(query_data<N, Cs...>& _data, size_t& _last_member)
            : m_data(_data), m_current_member(_last_member) {}

            bool operator!=(const query_range<N, Cs...>& _other)
            {
                return _other.m_current_member != m_current_member
                || _other.m_current_entity != m_current_entity;
            }

            auto operator++() -> query_range<N, Cs...>&
            {
                m_current_entity++;

                advance();

                return *this;
            }

            auto operator*() -> item<Cs...>
            {
                member_table& _members = m_data.groups.members[m_data.group_index];

                size_t _archetype_index = _members.archetype_index[m_current_member];

                id& _id = m_data.archetypes.entity_ids[_archetype_index][m_current_entity];

                return std::tie<const id, Cs...>(_id, std::get<store<Cs>&>(m_data.stores)[_archetype_index][m_current_entity]...);
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
                    for (size_t _archetype_index : m_data.groups.members[m_data.group_index].archetype_index)
                    {
                        for (size_t _component_index = 0; _component_index < m_data.archetypes.end[_archetype_index]; _component_index++)
                        {
                            const id& _id = m_data.archetypes.entity_ids[_archetype_index][_component_index];

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
                return query_range<N, Cs...>(m_data, m_data.groups.members[m_data.group_index].size);
            }
    };

    // ----------------------------------------------------------------------------
    // World 
    // ----------------------------------------------------------------------------

    struct world_queue 
    {
        size_t end = 0;
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

        // ----------------------------------------------------------------------------
        // Utility 
        // ----------------------------------------------------------------------------

        template <typename... Ts>
        auto create_bitmask() -> bitmask<N> 
        {
            bitmask<N> _mask;

            (_mask.set(store_index<Ts>{}, true),...);

            return _mask;
        }

        bool match_bitmask(bitmask<N>& _mask, bitmask<N>& _include_filter, bitmask<N>& _exclude_filter)
        {
            return (_include_filter & _mask) == _include_filter && (_exclude_filter & _mask).none();
        }

        // ----------------------------------------------------------------------------
        // Id management 
        // ----------------------------------------------------------------------------

        auto get_id( ) -> id
        {
            return m_data.entities.free.empty() ? create_id() : reuse_id();
        }

        auto create_id() -> id
        {
            size_t _index = m_data.entities.size;

            m_data.entities.component_index.emplace_back(0);
            m_data.entities.archetype_index.emplace_back(0);
            m_data.entities.version.emplace_back(0);
            m_data.entities.alive.emplace_back(true);

            m_data.entities.size++;

            return { _index, 0 };
        }

        auto reuse_id() -> id
        {
            size_t _index = m_data.entities.free.back();

            m_data.entities.version[_index]++;
            m_data.entities.alive[_index] = true;

            m_data.entities.free.pop_back();

            return { _index, m_data.entities.version[_index] };
        }

        void destroy_id(id& _id)
        {
            m_data.entities.alive[_id.index] = false;
            m_data.entities.free.emplace_back(_id.index);
        }

        void add_id(id& _id, size_t& _archetype_index)
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

            m_data.entities.archetype_index[_id.index] = _archetype_index;
            m_data.entities.component_index[_id.index] = _component_index;
        }

        void remove_id(id& _id, size_t& _archetype_index) 
        {
            std::vector<id>& _entity_ids = m_data.archetypes.entity_ids[_archetype_index];
            size_t& _component_index = m_data.entities.component_index[_id.index];
            size_t& _end = m_data.archetypes.end[_archetype_index];

            std::swap(_entity_ids[_component_index], _entity_ids[_end - 1]);
            id _swapped_id = _entity_ids[_component_index];
            m_data.entities.component_index[_swapped_id.index] = _component_index;
            _end--;

            if (_end == 0) destroy_archetype(_archetype_index);
        }

        // ----------------------------------------------------------------------------
        // Member management
        // ----------------------------------------------------------------------------

        auto create_member(size_t& _archetype_index, size_t& _group_index)
        {
            member_table& _members = m_data.groups.members[_group_index];
            membership_table& _membership = m_data.archetypes.memberships[_archetype_index];
        
            size_t _member_index = _members.size;
            size_t _membership_index = _membership.size;

            _members.archetype_index.emplace_back(_archetype_index);
            _members.membership_index.emplace_back(_membership_index);
            _members.size++;

            _membership.group_index.emplace_back(_group_index);
            _membership.member_index.emplace_back(_member_index);
            _membership.size++;
        }

        void destroy_member(size_t& _archetype_index, size_t& _group_index, size_t& _member_index)
        {
            assert(m_data.groups.members[_group_index].archetype_index[_member_index] == _archetype_index);

            size_t _index_to_swap = m_data.groups.members[_group_index].size - 1;

            std::swap(m_data.groups.members[_group_index].archetype_index[_member_index], m_data.groups.members[_group_index].archetype_index[_index_to_swap]);
            std::swap(m_data.groups.members[_group_index].membership_index[_member_index], m_data.groups.members[_group_index].membership_index[_index_to_swap]);

            m_data.groups.members[_group_index].archetype_index.pop_back();
            m_data.groups.members[_group_index].membership_index.pop_back();
            m_data.groups.members[_group_index].size--;

            // update archetype member

            size_t _swapped_archetype_index = m_data.groups.members[_group_index].archetype_index[_member_index];
            size_t _swapped_membership_index = m_data.groups.members[_group_index].membership_index[_member_index];

            m_data.archetypes.memberships[_swapped_archetype_index].member_index[_swapped_membership_index] = _member_index;
        }   

        // ----------------------------------------------------------------------------
        // Membership management
        // ----------------------------------------------------------------------------

        auto create_memberships(size_t _archetype_index)
        {
            for (size_t _group_index = 0; _group_index < m_data.groups.size; _group_index++)
            {
                bitmask<N>& _archetype_mask = m_data.archetypes.mask[_archetype_index];

                if (m_data.groups.cache[_group_index].contains(_archetype_mask))
                {
                    create_member(_archetype_index, _group_index);
                }
                else 
                {
                    bitmask<N * 2>& _group_mask = m_data.groups.mask[_group_index];

                    bitmask<N> _include_filter;
                    bitmask<N> _exclude_filter;

                    for (size_t i = 0; i < N; ++i)
                    {
                        _include_filter[i] = _group_mask[i];
                        _exclude_filter[i] = _group_mask[i + N];
                    }

                    if ((_include_filter & _archetype_mask) == _include_filter && (_exclude_filter & _archetype_mask).none())
                    {
                        create_member(_archetype_index, _group_index);
                        m_data.groups.cache[_group_index].insert(_archetype_mask);
                    }
                }
            }
        }

        void destroy_memberships(size_t& _archetype_index)
        {
            for (size_t _membership_index = 0; _membership_index < m_data.archetypes.memberships[_archetype_index].size; _membership_index++)
            {
                size_t _group_index = m_data.archetypes.memberships[_archetype_index].group_index[_membership_index];
                size_t _member_index = m_data.archetypes.memberships[_archetype_index].member_index[_membership_index];

                destroy_member(_archetype_index, _group_index, _member_index);
            }

            // reset memberships
            m_data.archetypes.memberships[_archetype_index] = membership_table{};
        }

        // ----------------------------------------------------------------------------
        // Group management 
        // ----------------------------------------------------------------------------

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

            if (!m_data.groups.index.contains(_group_mask))
            {
                return create_group(_group_mask, _include_filter, _exclude_filter);
            }
            else 
            {
                return m_data.groups.index[_group_mask];
            }
        }  

        auto create_group(bitmask<N*2>& _group_mask, bitmask<N>& _include_filter, bitmask<N>& _exclude_filter) -> size_t 
        {
            size_t _group_index = m_data.groups.size;

            m_data.groups.mask.emplace_back(_group_mask);
            m_data.groups.members.emplace_back(member_table{});
            m_data.groups.cache.emplace_back(std::unordered_set<bitmask<N>>{});

            m_data.groups.index[_group_mask] = _group_index;

            m_data.groups.size++;

            for (size_t _archetype_index = 0; _archetype_index < m_data.archetypes.size; _archetype_index++)
            {
                bitmask<N>& _archetype_mask = m_data.archetypes.mask[_archetype_index];

                if (m_data.archetypes.alive[_archetype_index] && match_bitmask(_archetype_mask, _include_filter, _exclude_filter)) // skip empty archetypes
                {
                    create_member(_archetype_index, _group_index);
                }
            }

            return _group_index;
        }

        // ----------------------------------------------------------------------------
        // Pool & component management 
        // ----------------------------------------------------------------------------

        template <typename T>
        auto get_pool(size_t& _archetype_index) -> pool<T>&
        {
            return std::get<store<T>>(m_data.components)[_archetype_index];
        }

        template <typename T>
        void create_pool()
        {   
            std::get<store<T>>(m_data.components).emplace_back(pool<T>());
        }

        template <typename T>
        void clear_pool(size_t& _archetype_index)
        {
            pool<T>& _pool = get_pool<T>(_archetype_index);

            if (!_pool.empty()) _pool.clear();
        }

        template <typename T>
        void change_pool(size_t& _component_index, size_t& _current_archetype_index, size_t& _new_archetype_index) 
        {
            if (m_data.archetypes.mask[_current_archetype_index].test(store_index<T>{}))
            {
                auto& _pool = get_pool<T>(_current_archetype_index);

                if (m_data.archetypes.mask[_new_archetype_index].test(store_index<T>{}))
                {
                    T& _component = _pool[_component_index];

                    add_component(_new_archetype_index, _component);   
                }
        
                std::swap(_pool[_component_index], _pool[m_data.archetypes.end[_current_archetype_index] - 1]);
            }
        }

        template <typename T>
        void add_component(size_t& _archetype_index, T& _component) 
        {
            size_t& _end = m_data.archetypes.end[_archetype_index];
            size_t& _total = m_data.archetypes.total[_archetype_index];

            pool<T>& _pool = get_pool<T>(_archetype_index);

            if (_end < _total)
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

        template <typename T>
        void remove_component(size_t& _component_index, size_t& _archetype_index) 
        {
            size_t _store_index = store_index<T>{};

            if (m_data.archetypes.mask[_archetype_index].test(_store_index))
            {
                pool<T>& _pool = get_pool<T>(_archetype_index);
        
                std::swap(_pool[_component_index], _pool[m_data.archetypes.end[_archetype_index] - 1]);
            }
        }

        // ----------------------------------------------------------------------------
        // Archetype management 
        // ----------------------------------------------------------------------------

        auto get_archetype(bitmask<N>& _archetype_mask) -> size_t
        {
            if (!m_data.archetypes.index.contains(_archetype_mask))
            {
                return m_data.archetypes.free.empty() 
                    ? create_archetype(_archetype_mask)
                    : reuse_archetype(_archetype_mask); 
            }
            else 
            {
                return m_data.archetypes.index[_archetype_mask];
            }
        }

        auto create_archetype(bitmask<N>& _archetype_mask) -> size_t
        {
            size_t _archetype_index = m_data.archetypes.size;

            m_data.archetypes.end.emplace_back(0);
            m_data.archetypes.total.emplace_back(0);
            m_data.archetypes.version.emplace_back(0);
            m_data.archetypes.alive.emplace_back(true);
            m_data.archetypes.mask.emplace_back(_archetype_mask);
            m_data.archetypes.entity_ids.emplace_back(std::vector<id>{});
            m_data.archetypes.memberships.emplace_back(membership_table{});
            (create_pool<Cs>(),...);
            m_data.archetypes.size++;

            create_memberships(_archetype_index);

            m_data.archetypes.index[_archetype_mask] = _archetype_index;

            return _archetype_index;
        }

        auto reuse_archetype(bitmask<N>& _archetype_mask) -> size_t
        {
            size_t _archetype_index = m_data.archetypes.free.back();

            m_data.archetypes.mask[_archetype_index] = _archetype_mask;
            m_data.archetypes.index[_archetype_mask] = _archetype_index;
            m_data.archetypes.alive[_archetype_index] = true;
            create_memberships(_archetype_index);
            m_data.archetypes.version[_archetype_index]++;
            m_data.archetypes.free.pop_back();

            return _archetype_index;
        }

        void destroy_archetype(size_t& _archetype_index)
        {            
            destroy_memberships(_archetype_index);
            m_data.archetypes.end[_archetype_index] = 0;
            m_data.archetypes.total[_archetype_index] = 0;
            m_data.archetypes.alive[_archetype_index] = false;
            m_data.archetypes.entity_ids[_archetype_index].clear();
            (clear_pool<Cs>(_archetype_index),...);
            m_data.archetypes.index.erase(m_data.archetypes.mask[_archetype_index]);
            m_data.archetypes.free.emplace_back(_archetype_index);
        }

        template <typename... Ts>
        auto change_archetype(id& _id, bool removing_components = false) -> size_t
        {
            size_t _current_archetype_index = m_data.entities.archetype_index[_id.index];
            size_t _component_index = m_data.entities.component_index[_id.index];

            bitmask<N> _current_mask = m_data.archetypes.mask[_current_archetype_index];

            bitmask<N> _new_mask = removing_components 
            ? _current_mask & ~create_bitmask<Ts...>() // erase non-matching
            : _current_mask | create_bitmask<Ts...>(); // keep both sets of bits

            size_t _new_archetype_index = get_archetype(_new_mask);

            (change_pool<Cs>(_component_index, _current_archetype_index, _new_archetype_index), ...);

            remove_id(_id, _current_archetype_index);
            add_id(_id, _new_archetype_index);

            return _new_archetype_index;
        }

        public:

            bool is_valid(id _id) 
            {
                return _id.index < m_data.entities.size 
                && _id.version == m_data.entities.version[_id.index];
            }

            bool is_alive(id _id)
            {
                return is_valid(_id) && m_data.entities.alive[_id.index];
            }

            template <typename T, typename... Ts>
            bool has(id _id) 
            {   
                if (is_alive(_id))
                {
                    size_t& _archetype_index = m_data.entities.archetype_index[_id.index];
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
            
            auto version(size_t _entity_index) -> size_t
            {
                return m_data.entities.version[_entity_index];
            }

            template <typename... Ts>
            auto create(Ts&&... _components) -> id 
            {
                id _id = get_id();

                bitmask _archetype_mask = create_bitmask<Ts...>();
                size_t _archetype_index = get_archetype(_archetype_mask);

                add_id(_id, _archetype_index);

                (add_component<Ts>(_archetype_index, _components),...);                

                return _id;
            }

            void destroy(id _id) 
            {
                if (!is_alive(_id)) return;

                size_t _component_index = m_data.entities.component_index[_id.index];
                size_t _archetype_index = m_data.entities.archetype_index[_id.index];

                // only non-empty
                (remove_component<Cs>(_component_index, _archetype_index),...);

                remove_id(_id, _archetype_index);

                destroy_id(_id);
            }

            template <typename T, typename... Ts>
            void add(id _id, T&& _component, Ts&&... _components) 
            {
                if (!has<T, Ts...>(_id))
                {
                    size_t _new_archetype_index = change_archetype<T, Ts...>(_id);

                    add_component<T>(_new_archetype_index, _component);
                    (add_component<Ts>(_new_archetype_index, _components),...);
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
                size_t _component_index = m_data.entities.component_index[_id.index];
                size_t _archetype_index = m_data.entities.archetype_index[_id.index];

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
                    std::tie(std::get<store<Ts>>(m_data.components)...)
                });
            }

            void queue(task&& _task) 
            {
                if (m_queue.end < m_queue.tasks.size())
                {
                    m_queue.tasks[m_queue.end] = _task;
                }
                else 
                {
                    m_queue.tasks.emplace_back(_task);
                }

                m_queue.end++;
            }

            void reset()
            {
                m_data = world_data<Cs...>{};
            }

            void update() 
            {

                for (size_t i = 0; i < m_queue.end; i++)
                {
                    m_queue.tasks[i]();
                }

                m_queue.end = 0;
            }

            auto read() const -> const world_data<Cs...>&
            {
                return m_data;
            }
    };

    // ----------------------------------------------------------------------------
    // Memory usage
    // ----------------------------------------------------------------------------

    template <typename K, typename V>
    inline auto memory_usage(const std::unordered_map<K, V>& _map) -> float
    {
        size_t _total = sizeof(_map);

        _total += _map.bucket_count() * sizeof(void*);

        _total += _map.size() * (sizeof(K) + sizeof(V) + sizeof(void*)); 

        return _total;
    }

    template <typename V>
    inline auto memory_usage(const std::unordered_set<V>& _set) -> float
    {
        size_t _total = sizeof(_set);

        _total += _set.bucket_count() * sizeof(void*);

        _total += _set.size() * (sizeof(V) + sizeof(void*)); 

        return _total;
    }

    template <typename C>
    inline auto memory_usage(const store<C>& _store) -> float
    {
        size_t _total = 0;

        _total += sizeof(store<C>);
        _total += _store.capacity() * sizeof(pool<C>);

        for (const pool<C>& _pool : _store)
        {
            _total += _pool.capacity() * sizeof(C);
        }

        return _total;
    }

    template <typename... Cs>
    inline auto memory_usage(const component_table<Cs...>& _components) -> float
    {
        return (memory_usage<Cs>(std::get<store<Cs>>(_components)) + ...);
    };

    template <size_t N>
    inline auto memory_usage(const archetype_table<N>& _archetypes) -> float
    {
        size_t _total = sizeof(_archetypes);

        _total += _archetypes.alive.capacity();
        _total += _archetypes.free.capacity()        * sizeof(size_t);
        _total += _archetypes.end.capacity()         * sizeof(size_t);
        _total += _archetypes.total.capacity()       * sizeof(size_t);
        _total += _archetypes.version.capacity()     * sizeof(size_t);
        _total += _archetypes.mask.capacity()        * sizeof(bitmask<N>);
        _total += _archetypes.entity_ids.capacity()  * sizeof(std::vector<id>);
        _total += _archetypes.memberships.capacity() * sizeof(membership_table);

        for (const auto& vec : _archetypes.entity_ids)
        {
            _total += vec.capacity() * sizeof(id);
        }

        for (const membership_table& _memberships : _archetypes.memberships)
        {
            _total += _memberships.group_index.capacity() * sizeof(size_t);
            _total += _memberships.member_index.capacity() * sizeof(size_t);
        }

        _total += memory_usage(_archetypes.index);

        return _total;
    };

    inline auto memory_usage(const entity_table& _entities) -> float
    {
        return sizeof(_entities)
            + _entities.alive.capacity()
            + _entities.component_index.capacity() * sizeof(size_t)
            + _entities.archetype_index.capacity() * sizeof(size_t)
            + _entities.free.capacity() * sizeof(size_t);

    };

    template <size_t N>
    inline auto memory_usage(const group_table<N>& _groups) -> float
    {
        size_t _total = sizeof(_groups);

        _total += _groups.mask.capacity() * sizeof(bitmask<N * 2>);
        _total += _groups.members.capacity() * sizeof(member_table);

        for (const member_table& _members : _groups.members)
        {
            _total += _members.archetype_index.capacity() * sizeof(size_t);
            _total += _members.membership_index.capacity() * sizeof(size_t);
        }

        for (const std::unordered_set<bitmask<N>>& _cache : _groups.cache)
        {
            _total += memory_usage<bitmask<N>>(_cache);
        }

        _total += memory_usage(_groups.index);

        return _total;
    };

    template <typename... Cs>
    inline auto memory_usage(const world_data<Cs...>& _data) -> float
    {
        constexpr size_t N = sizeof...(Cs);

        size_t _total = 0;
        for (const std::unordered_set<bitmask<N>>& _cache : _data.groups.cache)
        {
            _total += memory_usage<bitmask<N>>(_cache);
        }

        return memory_usage(_data.groups)
            + memory_usage(_data.archetypes)
            + memory_usage(_data.entities)
            + memory_usage<Cs...>(_data.components);
    }
};