#pragma once 

#include <algorithm>
#include <any>
#include <array>
#include <exception>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <tuple>
#include <typeindex>
#include <type_traits>
#include <typeinfo>
#include <vector>

namespace necs
{
    // ----------------------------------------------------------------------------
    // Filters
    // ----------------------------------------------------------------------------

    namespace filter
    {
        template<typename... Tuples>
        struct tuple_type_cat;

        template<typename... Ts>
        struct tuple_type_cat<std::tuple<Ts...>> { using type = std::tuple<Ts...>; };

        template<typename... T1s, typename... T2s, typename... Rest>
        struct tuple_type_cat<std::tuple<T1s...>, std::tuple<T2s...>, Rest...> 
        {
            using type = typename tuple_type_cat<std::tuple<T1s..., T2s...>, Rest...>::type;
        };

        template<typename...>
        struct unique_types;

        template<>
        struct unique_types<> { using type = std::tuple<>; };

        template<typename T, typename... Rest>
        struct unique_types<T, Rest...> 
        {
            using Tail = typename unique_types<Rest...>::type;

            static constexpr bool is_duplicate = (std::is_same_v<T, Rest> || ...);

            using type = std::conditional_t
            <
                is_duplicate,
                Tail,
                decltype(std::tuple_cat(
                    std::declval<std::tuple<T>>(), 
                    std::declval<Tail>()
                ))
            >;
        };

        template<typename... Tuples>
        struct merge_types 
        {
            using combined = typename tuple_type_cat<Tuples...>::type;

            template<typename Tuple>
            struct to_types;

            template<typename... Ts>
            struct to_types<std::tuple<Ts...>> 
            {
                using type = typename unique_types<Ts...>::type;
            };

            using type = typename to_types<combined>::type;
        };

        template<typename T, typename Tuple>
        struct has_type;

        template<typename T>
        struct has_type<T, std::tuple<>> : std::false_type {};

        template<typename T, typename Head, typename... Tail>
        struct has_type<T, std::tuple<Head, Tail...>>
            : std::conditional_t
            <
                std::is_same_v<std::remove_const_t<T>, std::remove_const_t<Head>>, 
                std::true_type, 
                has_type<std::remove_const_t<T>, std::tuple<std::remove_const_t<Tail>...>>
            > {};

        template<typename T, typename... Ts>
        struct has_all_types : std::conjunction<has_type<std::remove_const_t<Ts>, T>...> {};

        template<typename T, typename... Ts>
        struct has_any_type : std::disjunction<has_type<std::remove_const_t<Ts>, T>...> {};

        template <std::size_t I, typename Seq>
        struct concat_index;

        template <std::size_t I, std::size_t... Is>
        struct concat_index<I, std::index_sequence<Is...>> 
        {
            using type = std::index_sequence<I, Is...>;
        };

        template <typename Tuple, typename Seq, typename... Ts>
        struct filter_index;

        template <typename Tuple, std::size_t I, std::size_t... Is, typename... Ts>
        struct filter_index<Tuple, std::index_sequence<I, Is...>, Ts...> 
        {
            using tail = typename filter_index
            <
                Tuple, 
                std::index_sequence<Is...>, 
                Ts...
            >::type;

            using type = std::conditional_t
            <
                has_all_types<std::tuple_element_t<I, Tuple>, Ts...>::value,
                typename concat_index<I, tail>::type,
                tail
            >;
        };

        template <typename Tuple, typename... Ts>
        struct filter_index<Tuple, std::index_sequence<>, Ts...> 
        {
            using type = std::index_sequence<>;
        };

        template <typename Tuple, typename Seq, typename With, typename Without>
        struct match_index;

        template 
        <
            typename Tuple, 
            std::size_t I, 
            std::size_t... Is, 
            typename... Ws, 
            typename... Wos>
        struct match_index
        <
            Tuple, std::index_sequence<I, Is...>, 
            std::tuple<Ws...>, 
            std::tuple<Wos...>
        > 
        {
            using Tail = typename match_index
            <
                Tuple, 
                std::index_sequence<Is...>, 
                std::tuple<Ws...>, 
                std::tuple<Wos...>
            >::type;

            using CurrentTuple = std::tuple_element_t<I, Tuple>;

            static constexpr bool match =
                has_all_types<CurrentTuple, Ws...>::value &&
                !has_any_type<CurrentTuple, Wos...>::value;

            using type = std::conditional_t
            <
                match,
                typename concat_index<I, Tail>::type,
                Tail
            >;
        };

        template <typename Tuple, typename With, typename Without>
        struct match_index<Tuple, std::index_sequence<>, With, Without> {
            using type = std::index_sequence<>;
        };

        template <typename Tuple, typename With = std::tuple<>, typename Without = std::tuple<>>
        using Match = match_index<Tuple, std::make_index_sequence<std::tuple_size_v<Tuple>>, With, Without>::type;
    };

    template <typename... Cs>
    struct For {};
    template <typename... Cs> 
    struct With {};
    template <typename... Cs>
    struct Without {};

    // ----------------------------------------------------------------------------
    // Basic definitions & aliases 
    // ----------------------------------------------------------------------------

    using Type = std::type_index; // std::type_index alias.
    using Any = std::any; // std::any alias.
    using Error = std::runtime_error; // std::runtime_error alias.
    template <typename... Ts> // Tuple alias for convenience.
    using Data = std::tuple<Ts...>; 
    template <typename T> // Vec alias.
    using Vec = std::vector<T>;
    template <typename T> // Reference vec alias.
    using VecRef = std::vector<std::remove_const_t<T>>&;
    template <typename R, typename... Args>
    using Func = std::function<R(Args...)>;
    template <typename T> // Optional type alias.
    using Option = std::optional<T>; 
    template <typename K, typename V> // Map alias
    using Map = std::unordered_map<K, V>;
    template <typename T, template <typename...> class OuterWrapper, template <typename> class InnerWrapper>
    struct WrapData;
    template <template <typename...> class OuterWrapper, template <typename> class InnerWrapper, typename... Ts>
    struct WrapData<Data<Ts...>, OuterWrapper, InnerWrapper> {
        using type = OuterWrapper<InnerWrapper<Ts>...>;
    };

    template <typename T>
    auto GetType() -> Type
    {
        return std::type_index(typeid(T));
    }
    
    // ----------------------------------------------------------------------------
    // Entities
    // ----------------------------------------------------------------------------

    using EntityId = size_t; 

    struct Entities 
    {
        size_t total;
        size_t living_count;
        size_t dead_count;

        Vec<EntityId> to_reuse; // Ids to reuse.
        Vec<EntityId> to_remove; // Entities to remove on next update.

        Vec<Type> type; // Entity storage type.
        Vec<bool> alive; // Living status.
        Vec<bool> locked; // Id can be reused after death.
        Vec<size_t> index; // Storage index pointer.

        auto create(Type t, size_t i) -> EntityId
        {
            EntityId id = 0;

            if (to_reuse.size() == 0)
            {
                id = total;

                type.push_back(t);
                alive.push_back(true);
                locked.push_back(false);
                index.push_back(i);

                total++;
            }
            else 
            {
                id = to_reuse.back();

                type[id] = t;
                alive[id] = true;
                locked[id] = false;
                index[id] = i;

                to_reuse.pop_back();
                dead_count--;
            }

            living_count++;
            return id;
        }
    };  

    // ----------------------------------------------------------------------------
    // Archetype
    // ----------------------------------------------------------------------------

    template <typename... Cs>
    using Archetype = Data<EntityId, Cs...>;

    struct Archetypes
    {
        Map<Type, Map<Type, bool>> has; 
        Map<Type, Func<void, EntityId, bool>> update; 
    };

    // ----------------------------------------------------------------------------
    // Item
    // ----------------------------------------------------------------------------

    template <typename... Cs>
    using Item = Data
    <
        std::conditional_t
        <
            std::is_same_v<Cs, EntityId>,
            const Cs&, Cs&
        >...
    >;

    // ----------------------------------------------------------------------------
    // Pool
    // ----------------------------------------------------------------------------

    template <typename C>
    struct Pool
    {
        Vec<C> data;

        void push(C& component)
        {
            data.push_back(component);
        }

        void insert(C& component, size_t index)        
        {
            data[index] = component;
        }

        void erase(size_t start_index)
        {
            data.erase(data.begin() + start_index, data.end());
        }

        void swap(size_t index_1, size_t index_2)
        {
            std::swap(data[index_1], data[index_2]);
        }

        void clear()
        {
            data.clear();
        }
    };

    // ----------------------------------------------------------------------------
    // Chunk
    // ----------------------------------------------------------------------------

    template <typename... Cs>
    struct Chunk 
    {
        size_t& end;
        Data<VecRef<Cs>...> components;    
        
        template <typename C>
        auto vec() -> VecRef<C>
        {
            return std::get<VecRef<C>>(components);
        }

        auto get(size_t index) -> Data<Cs&...>
        {
            return std::tie<Cs...>((vec<Cs>()[index])...);
        }
    };

    // ----------------------------------------------------------------------------
    // Storage
    // ----------------------------------------------------------------------------

    template <typename A>
    class Storage
    {
        using First = std::decay_t<decltype(std::get<0>(A{}))>;
        static_assert(std::is_same_v<First, EntityId>, "@Storage - template data must be a valid Archetype containing an Id.");
        using Components = filter::merge_types<A>::type;
        static_assert(std::is_same_v<Components, A>, "@Storage - components must not repeat.");
        using StorageData = WrapData<A, Data, Pool>::type;

        size_t m_end = 0;
        size_t m_total = 0;
        StorageData m_data;
        
        public: 
            template <typename C>
            auto get_pool() -> Pool<C>&
            {
                static_assert(
                    filter::has_type<C, A>{}, 
                    "@Storage::get_pool - The requested pool is not inside this Storage."
                );
            
                return std::get<Pool<C>>(m_data);
            }

            template <typename C>
            auto get_vec() -> VecRef<C>
            {
                return get_pool<std::remove_const_t<C>>().data;
            }

            template <typename... Cs>
            auto get_chunk() -> Chunk<Cs...>
            {
                return {m_end, std::tie((get_vec<Cs>())...)};
            }

            auto get_total() -> size_t&
            {
                return m_total;
            }

            auto get_count() -> size_t&
            {
                return m_end;
            }

            template <typename C>
            auto get_component(size_t index) ->  C&
            {
                return get_vec<C>()[index];
            }

            template <typename... Cs>
            auto get_components(size_t index) -> Data<Cs&...>
            {
                return std::tie(get_component<Cs>(index)...);
            }

            template <typename... Cs>
            void create(Data<Cs...> entity)
            {
                if (m_end == m_total)
                {
                    (get_pool<Cs>().push(std::get<Cs>(entity)),...);
                    m_total++;
                }
                else 
                {
                    (get_pool<Cs>().insert(std::get<Cs>(entity), m_end),...);
                }

                m_end++;
            }

            auto remove(size_t index) -> EntityId 
            {
                [this, &index]<typename... Cs>(Data<Cs...>)
                {
                    (get_pool<Cs>().swap(index, m_end - 1),...);
                    m_end--;
                }
                (A{});

                return get_vec<EntityId>()[index];
            }

            void trim()
            {
                if (m_end < m_total)
                {
                    [this]<typename... Cs>(Data<Cs...>)
                    {
                        (get_pool<Cs>().erase(m_end),...);
                        m_total -= m_total - m_end;
                    }
                    (A{});
                }
            }

            template <typename... Cs>
            auto iter()
            {
                return iterator<Cs...>(get_chunk<Cs...>());
            }

            template <typename... Cs>
            class iterator 
            {
                size_t current = 0;
                Chunk<Cs...> chunk;

                public: 
                    iterator(Chunk<Cs...> _chunk)
                    :  chunk(_chunk) {}

                    bool operator!=(const iterator<Cs...>& other) const 
                    {
                        return current != other.current;
                    }

                    auto operator*() -> Item<Cs...>
                    {
                        return chunk.get(current);
                    }

                    iterator<Cs...>& operator++() 
                    {
                        ++current;
                        return *this;            
                    }

                    iterator<Cs...>& begin() 
                    {
                        current = 0;
                        return *this;            
                    }

                    iterator<Cs...>& end() 
                    {
                        current = chunk.end;
                        return *this;            
                    }
            };
    };

    template <typename... As>
    using Storages = Data<Storage<As>...>;

    // ----------------------------------------------------------------------------
    // Query
    // ----------------------------------------------------------------------------

    template <typename... Cs>
    using QueryData = Vec<Chunk<Cs...>>;

    template <typename... Cs>
    class QueryResult
    {        
        QueryData<Cs...> m_data;

        template <typename... As, typename... Ws, typename... Wos>
        auto init(Storages<As...>& storages, With<Ws...>, Without<Wos...>) -> QueryData<Cs...>
        {
            static_assert(std::tuple_size_v<Data<As...>> > 0, "@InitQueryData - storages cannot be empty.");
            static_assert(std::tuple_size_v<Data<Cs...>> > 0, "@InitQueryData - components cannot be empty.");
            static_assert(filter::has_any_type<Data<Cs...>, Ws...>::value == false, "@InitQueryData - For & With component filters cannot repeat.");
            static_assert(filter::has_any_type<Data<Cs...>, Wos...>::value == false, "@InitQueryData - For & Without component filters cannot repeat.");
            static_assert(filter::has_any_type<Data<Ws...>, Wos...>::value == false, "@InitQueryData - With & Without component filters cannot repeat.");
            using Match = filter::Match<Data<As...>, Data<Cs..., Ws...>, Data<Wos...>>;
            static_assert(sizeof(Match{}) > 0, "@InitQueryData - reduntant query, no archetypes match the filter.");

            QueryData<Cs...> data; 

            [&storages, &data]<size_t... Is>(std::index_sequence<Is...>)
            {
                auto push = [&data]<typename A>(Storage<A>& storage)
                {
                    data.push_back(storage.template get_chunk<Cs...>());
                };

                ((push(std::get<Is>(storages))),...);
            }
            (Match{});

            if (data.size() == 0)
            {
                throw Error("@InitQueryData - unexpected empty data.");
            }

            return data;
        }

        public: 
            template <typename... As, typename... Ws, typename... Wos>
            QueryResult(Storages<As...>& storages, With<Ws...> ws, Without<Wos...> wos)
            : m_data(init(storages, ws, wos)) 
            {
                if (m_data.size() == 0)
                {
                    throw Error("@Query::Queery - number of storages / data cannot be 0.");
                }
            }

            auto iter()
            {
                return iterator(m_data);
            }

            template <typename Callback>
            auto forEach(Callback&& callback)
            {
                static_assert(
                    std::is_invocable_v<Callback, Data<Cs&...>>, 
                    "@QueryResult::forEach - Callback must be invocable."
                );

                for (Chunk<Cs...>& chunk : m_data)
                {
                    for (size_t i = 0; i < chunk.end; i++)
                    {
                        callback(chunk.get(i));
                    }
                }
            }   

            class iterator
            {
                size_t m_current_chunk = 0;
                size_t m_current_entity = 0;
                QueryData<Cs...>& m_data;

                void advance()
                {
                    m_current_chunk++;

                    while (m_current_chunk < m_data.size())
                    {                
                        if (m_data[m_current_chunk].end == 0)
                        {
                            m_current_chunk++;
                        }
                        else 
                        {
                            return;
                        }
                    }
                }

                public: 
                    iterator(QueryData<Cs...>& _data)
                    : m_data(_data) {}

                    bool operator!=(const iterator& other) const 
                    {
                        return m_current_chunk != other.m_current_chunk;
                    }

                    auto operator*() -> Item<Cs...>
                    {
                        return m_data[m_current_chunk].get(m_current_entity);
                    }

                    auto operator++() -> iterator&
                    {   
                        m_current_entity++;

                        if (m_current_entity >= m_data[m_current_chunk].end) advance();

                        return *this;
                    }

                    iterator& begin()
                    {
                        m_current_chunk = 0;
                        m_current_entity = 0;

                        if (m_data[0].end == 0) advance();

                        return *this;
                    }

                    iterator& end()
                    {
                        m_current_chunk = m_data.size();
                        m_current_entity = m_data[m_current_chunk].end;

                        return *this;
                    }
            };
    };

    template <typename... Ts>
    class Query;

    template <typename... Cs>
    struct Query<For<Cs...>> : public QueryResult<Cs...>
    {
        template <typename... As>
        Query(Storages<As...>& storages) 
        :  QueryResult<Cs...>(storages, With<>{}, Without<>{}) {}
    };

    template <typename... Cs, typename... Ws>
    struct Query<For<Cs...>, With<Ws...>> : QueryResult<Cs...>
    {
        template <typename... As>
        Query(Storages<As...>& storages) 
            : QueryResult<Cs...>(storages, With<Ws...>{}, Without<>{}) {}
    };

    template <typename... Cs, typename... Wos>
    struct Query<For<Cs...>, Without<Wos...>> : QueryResult<Cs...>
    {
        template <typename... As>
        Query(Storages<As...>& storages) 
            : QueryResult<Cs...>(storages, For<Cs...>{}, With<>{}, Without<Wos...>{}) {}
    };

    template <typename... Cs, typename... Ws, typename... Wos>
    struct Query<For<Cs...>, With<Ws...>, Without<Wos...>> : QueryResult<Cs...>
    {
        template <typename... As>
        Query(Storages<As...>& storages) 
            :  QueryResult<Cs...>(storages, For<Cs...>{}, With<Ws...>{}, Without<Wos...>{}) {}
    };

    template <typename... Qs>
    struct Queries : public Data<Qs...>
    {
        template <typename Q, typename... As>
        auto init(Storages<As...>& storages) -> Q
        {
            return Q(storages);
        }

        template <typename... As>
        Queries(Storages<As...>& storages)
        : Data<Qs...>(std::make_tuple(init<Qs>(storages)...)) {};
    };

    // ----------------------------------------------------------------------------
    // Listener & built-in events
    // ----------------------------------------------------------------------------

    struct EntityCreated { EntityId id; };
    struct EntityRemoved { EntityId id; };
    struct DataUpdated {Type type; };

    template <typename E>
    class Listener 
    {
        bool m_ready; 
        bool m_initialized;
        Func<void, E> m_callback = {};

        public:
            template <typename Callback>
            void subscribe(Callback&& callback)
            {
                static_assert(std::is_invocable_v<Callback, E>, "@Listener::subscribe - the callback args do not match.");

                m_callback = callback;
                m_ready = true;
                m_initialized = true;
            }

            void close()
            {
                if (m_initialized) m_ready = false;
            }

            void open()
            {
                if (m_initialized) m_ready = true;
            }

            void call(E event)
            {
                if (m_ready) m_callback(event);
            }
    };

    template <typename... Es>
    using Listeners = Data
    <
        Listener<EntityCreated>, 
        Listener<EntityRemoved>, 
        Listener<DataUpdated>, 
        Listener<Es>...
    >;

    // ----------------------------------------------------------------------------
    // Info
    // ----------------------------------------------------------------------------

    class Info
    {
        protected: 
            Entities& m_entities;
            Archetypes& m_archetypes;

        public:
            Info(Entities& _entities, Archetypes& _archetypes)
            : m_entities(_entities), m_archetypes(_archetypes) {}

            bool exists(EntityId id) const
            {
                return id <= m_entities.total;
            }

            bool is_alive(EntityId id) const
            {
                return exists(id) && m_entities.alive[id];
            }

            bool is_locked(EntityId id) const
            {
                return exists(id) && m_entities.locked[id];
            }

            template <typename A>
            bool is_archetype(EntityId id) const
            {
                return is_alive(id) && m_entities.type[id] == GetType<A>();
            }
            
            bool is_archetype(EntityId id, Type archetype) const
            {
                return is_alive(id) && m_entities.type[id] == archetype;
            }

            template <typename A>
            bool has_archetype() const
            {
                return m_archetypes.has.find(GetType<A>()) != m_archetypes.has.end();
            }

            bool has_archetype(Type archetype) const
            {
                return m_archetypes.has.find(archetype) != m_archetypes.has.end();
            }

            template <typename C>
            bool has_component(EntityId id) const 
            {
                return is_alive(id) && m_archetypes.has[m_entities.type[id]][GetType<C>()];
            }

            bool has_component(EntityId id, Type t) const 
            {
                return is_alive(id) && m_archetypes.has[m_entities.type[id]][t];
            }

            auto get_type(EntityId id) const -> const Type&
            {
                return m_entities.type[id];
            }

            auto get_index(EntityId id) const -> const size_t&
            {
                return m_entities.index[id];
            }

            auto get_to_remove() const -> const Vec<EntityId>&
            {
                return m_entities.to_remove;
            }

            auto get_to_reuse() const -> const Vec<EntityId>&
            {
                return m_entities.to_reuse;
            }
            
            auto get_total() const -> const size_t&
            {
                return m_entities.total;
            }
    };

    // ----------------------------------------------------------------------------
    // Control
    // ----------------------------------------------------------------------------

    template <typename... As>
    class Control : public Info
    {
        using ControlData = Data<Storage<As>&...>;

        ControlData m_data;
        Listener<EntityCreated>& m_on_created;
        Listener<DataUpdated>& m_on_updated;

        template <typename A>
        auto get_storage() -> Storage<A>&
        {
            return std::get<Storage<A>&>(m_data);
        }

        template <typename... Cs>
        auto get_matching()
        {
            return [this] <size_t... Is>(std::index_sequence<Is...>)
            {
                return std::tie(std::get<Is>(m_data)...);
            }
            (filter::Match<Data<As...>, Data<Cs...>>{});
        }

        public:

            Control
            (
                Entities& _entities, 
                Archetypes& _archetypes, 
                ControlData _data,
                Listener<EntityCreated>& _on_created, 
                Listener<DataUpdated>& _on_updated
            ) : Info(_entities, _archetypes),
            m_data(_data),
            m_on_created(_on_created), 
            m_on_updated(_on_updated)
            {}

            template <typename A, typename... Cs> 
            auto get(EntityId id) -> Option<Item<Cs...>>
            {
                if (!is_archetype<A>(id) || !is_alive(id))
                {
                    return std::nullopt;
                }
                else 
                {
                    return get_storage<A>().template get_components<Cs...>(m_entities.index[id]);
                }
            }

            template <typename... Cs>
            auto find(EntityId id) -> Option<Item<Cs...>>
            {
                Option<Item<Cs...>> item;

                [this, &item, &id]<typename... Ms>(Data<Storage<Ms>&...> storages)
                {
                    bool found = false;

                    auto f = [this, &item, &id, &found]<typename A>(Storage<A>& storage)
                    {
                        if (found) return;

                        if (is_archetype<A>(id))
                        {
                            found = true;

                            item = storage.template get_components<Cs...>(get_index(id));
                        }
                    };  

                    ((f(std::get<Storage<Ms>&>(storages))),...);
                }
                (get_matching<Data<Cs...>>());

                return item;
            }

            template <typename A, typename... Cs> 
            auto iter()
            {
                return get_storage<A>().template iter<Cs...>();
            }

            template <typename A>
            auto create(A entity, bool run_callbacks = false) -> EntityId
            {
                Storage<A>& storage = get_storage<A>();
                Type type = GetType<A>();
                EntityId id = m_entities.create(type, storage.get_count());
                std::get<EntityId>(entity) = id;
                storage.create(entity);

                if (run_callbacks)
                {
                    m_on_created.call({ id });
                    m_on_updated.call({ type });
                }

                return id;
            }

            template <typename A>
            void populate(A entity, int count, bool run_callback = false) 
            {
                for (int i = 0; i < count; i++)
                {
                    create(entity);
                }
                    
                if (run_callback) m_on_updated.call({ GetType<A>() });
            }

            void remove(EntityId id) 
            {   
                if (is_alive(id))
                {
                    m_entities.to_remove.push_back(id);
                }
            }

            template <typename A>
            void wipe(bool run_callback = false) 
            {
                for (const EntityId& id : get_storage<A>().template get_vec<EntityId>())
                {
                    remove(id);
                }
            }

            void lock(EntityId id) 
            {
                if (exists(id))
                {
                    m_entities.locked[id] = true;
                }
            }

            template <typename A>
            void trim(bool run_callback = false) 
            {
                Storage<A>& storage = get_storage<A>();
                storage.trim();

                if (run_callback) m_on_updated.call({ GetType<A>() });
            }

            void update(bool run_callbacks = false) 
            {
                for (const EntityId& id : m_entities.to_remove)
                {
                    m_archetypes.update[m_entities.type[id]](id, run_callbacks);
                }

                m_entities.to_remove.clear();
            }
    };

    // ----------------------------------------------------------------------------
    // Registry
    // ----------------------------------------------------------------------------

    template <typename As, typename Qs, typename Es>
    class Registry;

    template <typename... As, typename... Qs, typename... Es>
    class Registry<Data<As...>, Data<Qs...>, Data<Es...>>
    {
        Storages<As...> m_storages;
        Listeners<Es...> m_listeners;
        Queries<Qs...> m_queries{ m_storages };
        Archetypes m_archetypes;
        Entities m_entities;

        template <typename A, typename C>
        auto init_component()
        {
            Type a_type = std::type_index(typeid(A));
            Type c_type = std::type_index(typeid(C));

            m_archetypes.has[a_type][c_type] = true;
        }

        template <typename... Cs>
        auto init_archetype(Storage<Data<Cs...>>& storage)
        {
            using A = Data<Cs...>;

            ((init_component<A, Cs>()),...);
        
            Type a_type = std::type_index(typeid(A));

            m_archetypes.update[a_type] = [this, &storage](EntityId id, bool run_callbacks = false) 
            {
                if (id < m_entities.total && m_entities.alive[id])
                {
                    size_t index = m_entities.index[id];

                    if (index < storage.get_count())
                    {
                        EntityId swapped_id = storage.remove(index);

                        m_entities.index[swapped_id] = index;
                        m_entities.alive[id] = false;

                        if (!m_entities.locked[id])
                        {
                            m_entities.to_reuse.push_back(id);
                        }

                        if (run_callbacks)
                        {
                            get_listener<EntityRemoved>().call({id});
                            get_listener<DataUpdated>().call({m_entities.type[id]});
                        }
                    }
                }
            };
        }

        public: 

            Registry() 
            {
                ((init_archetype(std::get<Storage<As>>(m_storages))),...);
            }

            auto get_info() 
            {
                return Info{m_entities, m_archetypes};
            }

            template <typename... Ts>
            auto get_control() 
            {
                return Control
                {   
                    m_entities, 
                    m_archetypes, 
                    std::tie(get_storage<Ts>()...),
                    get_listener<EntityCreated>(), 
                    get_listener<DataUpdated>()
                };
            }

            template <typename A>
            auto get_storage() -> Storage<A>&
            {
                return std::get<Storage<A>>(m_storages);
            }

            template <typename Q>
            auto get_query() -> Q&
            {
                return std::get<Q>(m_queries);
            }
            
            template <typename E>
            auto get_listener() -> Listener<E>&
            {
                return std::get<Listener<E>>(m_listeners);
            }

            auto get_archetypes() -> Archetypes&
            {
                return m_archetypes;
            }
            
            auto get_entities() -> Entities&
            {
                return m_entities;
            }
    };
};