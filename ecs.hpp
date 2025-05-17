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

    /**
     * Internal namespace containing structs for filtering tuples.
     */
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

    template <typename... Cs> // Filters & extracts select components.
    struct For {};
    template <typename... Cs> // Includes arhcetypes with select components.
    struct With {};
    template <typename... Cs> // Excludes archetypes with select components.
    struct Without {};

    // ----------------------------------------------------------------------------
    // Basic definitions & aliases 
    // ----------------------------------------------------------------------------

    using Id = size_t; // Reusable unique identifier alias. 
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

    // ----------------------------------------------------------------------------
    // Archetype
    // ----------------------------------------------------------------------------

    /** 
     * Alias for archetypes including entity ids.
     * Used to register archetypes in the registry to be able to query for ids.
     */
    template <typename... Cs>
    using Archetype = Data<Id, Cs...>;

    /**
     * Type-erased archetype data.
     * Used for component checks and lookups when all we have is the entity id.
     * Initiated by the Registry.
     */
    struct Archetypes
    {
        // udpate calls per archetype storage 
        Map<Type, Func<void, Id>> update; 
        // type-erased references to storages 
        Map<Type, Any> storage;
        // inserts false by default, maintains 01 checks
        Map<Type, Map<Type, bool>> has; 
        // gets a pool reference if it exists, never insert
        Map<Type, Map<Type, Any>> pool; 
    };
    
    // ----------------------------------------------------------------------------
    // Entities
    // ----------------------------------------------------------------------------

    /**
     * Entity metadata storage.
     * Contains information on the entity's location & status at the index (id) of an 
     * entity. Contains state management data.
     */
    struct Entities 
    {
        // ---- State management ---- //

        size_t total;

        Vec<Id> to_reuse; // Ids to reuse.
        Vec<Id> to_remove; // Entities to remove on next update.

        // ---- Entity data ---- //

        Vec<Type> type; // Entity storage type.
        Vec<bool> alive; // Living status.
        Vec<bool> locked; // Id can be reused after death.
        Vec<size_t> index; // Storage index pointer.
    };  

    // ----------------------------------------------------------------------------
    // Pool
    // ----------------------------------------------------------------------------

    /**
     * Stores a single vector of components. 
     * Data viability and checks are managed by the Storage that contains it.
     * @tparam C The type of component stored in the pool.
     */
    template <typename C>
    struct Pool 
    {
        Vec<C> data;

        // Expands the underlying data vector.
        void push(C& component)
        {
            data.push_back(component);
        }

        // Overwrites some data in the vector.
        void insert(C& component, size_t index)        
        {
            data[index] = component;
        }

        // Erases all data after an index.
        void erase(size_t start_index)
        {
            data.erase(data.begin() + start_index, data.end());
        }

        // Swaps the contents of the vector.
        void swap(size_t index_1, size_t index_2)
        {
            std::swap(data[index_1], data[index_2]);
        }

        // Clears all component data.
        void clear()
        {
            data.clear();
        }
    };

    // ----------------------------------------------------------------------------
    // Chunk
    // ----------------------------------------------------------------------------

    /**
     * References to a storage's component vectors and last living entity.
     */
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
            if (index >= end) 
            {
                throw Error("@IterItem::get - index out of bounds.");
            }
            else 
            {
                return std::tie<Cs...>((vec<Cs>()[index])...);
            }
        }
    };

    // ----------------------------------------------------------------------------
    // Storage
    // ----------------------------------------------------------------------------

    template <typename... Cs>
    struct StorageIterator
    {          
        size_t current = 0;
        Chunk<Cs...> item;

        StorageIterator(Chunk<Cs...> _item)
        :  item(_item) {}

        bool operator!=(const StorageIterator<Cs...>& other) const 
        {
            return current != other.current;
        }

        auto operator*() -> Data<Cs&...>
        {
            return item.get(current);
        }

        StorageIterator<Cs...>& operator++() 
        {
            ++current;
            return *this;            
        }

        StorageIterator<Cs...>& begin() 
        {
            current = 0;
            return *this;            
        }

        StorageIterator<Cs...>& end() 
        {
            current = item.end;
            return *this;            
        }
    };

    template <typename A>
    struct Storage;

    /**
     * Base storage, templated for a single archetype.
     * Storages live for the entire duration of the Registry's lifetime.
     * 
     * @tparam Cs... The Data<Cs...> must be a valid Archetype with unique components.
     */
    template <typename... Cs>
    struct Storage<Data<Cs...>>
    {
        using First = std::decay_t<decltype(std::get<0>(Data<Cs...>{}))>;
        static_assert(std::is_same_v<First, Id>, "@Storage - template data must be a valid Archetype containing an Id.");
        using Components = filter::merge_types<Data<Cs...>>::type;
        static_assert(std::is_same_v<Components, Data<Cs...>>, "@Storage - components must not repeat.");
        using StorageData = Data<Pool<Cs>...>;

        size_t end = 0; // last living entity + 1
        size_t total = 0; // data capacity
        StorageData data; // component vectors

        template <typename C>
        auto pool() -> Pool<C>&
        {
            static_assert(
                filter::has_type<C, Data<Cs...>>{}, 
                "@Storage::get - The requested pool is not inside this Storage."
            );
            return std::get<Pool<C>>(data);
        }

        template <typename C>
        auto vec() -> VecRef<C>
        {
            return pool<std::remove_const_t<C>>().data;
        }

        template <typename... Ts>
        auto item() -> Chunk<Ts...>
        {
            static_assert(filter::has_all_types<Data<Cs...>, Ts...>::value, "@Storage::item - Types to iter must be present in storage.");

            return {end, std::tie((vec<Ts>())...)};
        }

        template <typename... Ts>
        auto iter() -> StorageIterator<Ts...>
        {
            static_assert(filter::has_all_types<Data<Cs...>, Ts...>::value, "@Storage::iter - Types to iter must be present in storage.");

            return StorageIterator<Ts...>(item<Ts...>());
        }

        auto remove(size_t index) -> Id 
        {
            if (index >= end)
            {
                throw Error("@Storage::remove - the index is out of bounds.");
            };

            (pool<Cs>().swap(index, end - 1),...);
            end--;

            return pool<First>().data[index];
        }
   
        void create(Data<Cs...> entity)
        {
            if (end == total)
            {
                (pool<Cs>().push(std::get<Cs>(entity)),...);
                total++;
            }
            else 
            {
                (pool<Cs>().insert(std::get<Cs>(entity), end),...);
            }

            end++;
        }

        void trim()
        {
            if (end < total)
            {
                (pool<Cs>().erase(end),...);
                total -= total - end;
            }
        }

        void clear()
        {
            (pool<Cs>().clear(),...);
            end = 0;
            total = 0;
        }
    }; 

    template <typename... As>
    using Storages = Data<Storage<As>...>;

    // ----------------------------------------------------------------------------
    // Query
    // ----------------------------------------------------------------------------
    template <typename... Cs>
    using QueryData = Vec<Chunk<Cs...>>;

    template <typename... Cs>
    class QueryIterator
    {
        size_t m_current_storage = 0;
        size_t m_current_entity = 0;
        QueryData<Cs...> m_data;

        void advance()
        {
            m_current_storage++;

            while (m_current_storage < m_data.size())
            {                
                if (m_data[m_current_storage].end == 0)
                {
                    m_current_storage++;
                }
                else 
                {
                    return;
                }
            }
        }

        public:
            QueryIterator(QueryData<Cs...> _data)
            : m_data(_data) 
            {
                if (_data.size() == 0)
                {
                    throw Error("@Iterator::Iterator - number of storages / data cannot be 0.");
                }
            }

            bool operator!=(const QueryIterator<Cs...>& other) const 
            {
                return m_current_storage != other.m_current_storage;
            }

            auto operator*() -> Data<Cs&...>
            {
                return m_data[m_current_storage].get(m_current_entity);
            }

            QueryIterator<Cs...>& operator++() 
            {   
                m_current_entity++;

                if (m_current_entity >= m_data[m_current_storage].end) advance();

                return *this;
            }

            QueryIterator<Cs...>& begin()
            {
                m_current_storage = 0;
                m_current_entity = 0;

                if (m_data[0].end == 0) advance();

                return *this;
            }

            QueryIterator<Cs...>& end()
            {
                m_current_storage = m_data.size();
                m_current_entity = m_data[m_current_storage].end;

                return *this;
            }
    };

    template <typename... As,typename... Cs, typename... Ws, typename... Wos>
    auto InitQueryData(Storages<As...>& storages, For<Cs...>, With<Ws...>, Without<Wos...>) -> QueryData<Cs...>
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
                data.push_back(storage.template item<Cs...>());
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

    template <typename... Ts>
    class Query;

    template <typename... Cs>
    struct Query<For<Cs...>> : public QueryIterator<Cs...>
    {
        template <typename... As>
        Query(Storages<As...>& storages) 
        : QueryIterator<Cs...>(InitQueryData(storages, For<Cs...>{}, With<>{}, Without<>{})) {}
    };

    template <typename... Cs, typename... Ws>
    struct Query<For<Cs...>, With<Ws...>> : QueryIterator<Cs...>
    {
        template <typename... As>
        Query(Data<Storage<As>&...> storages) 
            : QueryIterator<Cs...>(InitQueryData(storages, For<Cs...>{}, With<Ws...>{}, Without<>{})) {}
    };

    template <typename... Cs, typename... Wos>
    struct Query<For<Cs...>, Without<Wos...>> : QueryIterator<Cs...>
    {
        template <typename... As>
        Query(Data<Storage<As>&...> storages) 
            : QueryIterator<Cs...>(InitQueryData(storages, For<Cs...>{}, With<>{}, Without<Wos...>{})) {}
    };

    template <typename... Cs, typename... Ws, typename... Wos>
    struct Query<For<Cs...>, With<Ws...>, Without<Wos...>> : QueryIterator<Cs...>
    {
        template <typename... As>
        Query(Data<Storage<As>&...> storages) 
            : QueryIterator<Cs...>(InitQueryData(storages, For<Cs...>{}, With<Ws...>{}, Without<Wos...>{})) {}
    };

    template <typename... Qs>
    using Queries = Data<Qs...>;

    // ----------------------------------------------------------------------------
    // Listener & Events
    // ----------------------------------------------------------------------------

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

    struct EntityCreated { Id id; };
    struct EntityRemoved { Id id; };
    template <typename T>
    struct DataCreated {};
    template <typename T>
    struct DataRemoved {};

    template <typename... As, typename... Es>
    auto InitListeners(Data<As...>, Data<Es...>)
    {
        return []<typename... Cs>(Data<Cs...>)
        {
            return Data
            <
            Listener<EntityCreated>, 
            Listener<EntityRemoved>, 
            Listener<DataCreated<As>>..., 
            Listener<DataRemoved<As>>..., 
            Listener<DataCreated<Cs>>..., 
            Listener<DataRemoved<Cs>>...,
            Listener<Es>...
            >{};
        }
        (filter::merge_types<As...>{});
    };

    template <typename As, typename Es>
    using Listeners = std::invoke_result_t<decltype(InitListeners(As{}, Es{}))>;

//     // ----------------------------------------------------------------------------
//     // Unique
//     // ----------------------------------------------------------------------------

//     /**
//      * Type-erased system data. Unique resources can be stored here.
//      * Provides O1 access to storage component data at the cost of cache misses.
//      * It is still much faster than find functions that filter and iterate through archetypes.
//      * TODO: store dynamic queries.
//      */
//     class Uniques
//     {
//         Map<Type, StorageRef&> m_storages;

//         template <typename A>
//         void init(Storage<A>& storage)
//         {
//             m_storages[key<A>()] = storage.ref();
//         }

//         public:
//             template <typename... As>
//             Uniques(Storages<As...>& storages)
//             {
//                 (init(std::get<Storage<As>>(storages)),...);
//             }

//             template <typename T>
//             auto key() -> Type
//             {
//                 return std::type_index(typeid(T));
//             }

//             auto ref(Type archetype_key) -> StorageRef&
//             {
//                 return m_storages[archetype_key];
//             }

//             template <typename C>
//             auto has(Type archetype_key)
//             {
//                 Type component_key = key<C>();
//                 auto& r = ref(archetype_key);

//                 return r.has_component[component_key];
//             }

//             template <typename C>
//             auto vector(Type archetype_key) -> Option<VecRef<C>>
//             {
//                 Type component_key = key<C>();
//                 auto& r = ref(archetype_key);

//                 if (r.has_component[component_key])
//                 {
//                     return std::any_cast<VecRef<C>>(r.get_components[component_key]);
//                 }
//                 else 
//                 {
//                     return std::nullopt;
//                 }
//             }

//             template <typename C>
//             auto component(Type archetype_key, size_t index) -> Option<Ref<C>>
//             {
//                 Option<VecRef<C>> o = vector<C>(archetype_key);

//                 if (o.has_value())
//                 {
//                     auto& [v] = o.value();
                    
//                     return Ref<C>(v[index]);
//                 }
//                 else 
//                 {
//                     std::nullopt;
//                 }
//             }
//     };

//     // ----------------------------------------------------------------------------
//     // Registry
//     // ----------------------------------------------------------------------------

//     template <typename As, typename Qs>
//     class Registry;

//     /**
//      * Main API class for ECS functions. 
//      * Supports the following operations:
//      * 
//      * - CREATE - creates a single entity.
//      * 
//      * - POPULATE - populates a storage with copies of an entity.
//      * 
//      * - QUEUE - queues entities for removal. 
//      * 
//      * - UPDATE - executes all queued removals. 
//      * 
//      * - REMOVE - removes an entity instantly, invalidates iterators.
//      * 
//      * Contains utility functions for reading entity metadata. Exposes system data.
//      * 
//      *  @tparam As... Archetypes.
//      *  @tparam Qs... Queries.
//      */
//     template <typename... As, typename... Qs>
//     class Registry<Data<As...>, Data<Qs...>>
//     {
//         Entities m_entities;
//         Storages<As...> m_storages;
//         Queries<Qs...> m_queries = init_queries();
//         Uniques m_uniques = init_uniques();

//         // ---- Update & Init ---- //

//         template <typename A> // updates an entity instantly
//         void apply_update(Id id)
//         {
//             if (id < m_entities.total && m_entities.alive[id])
//             {
//                 Storage<A>& storage = get_storage<A>();
//                 size_t index = m_entities.index[id];
//                 Id swapped_entity = storage.remove(index);
//                 m_entities.index[swapped_entity] = index;
//                 if (!m_entities.locked[id])
//                 {
//                     m_entities.to_reuse.push_back(id);
//                 }

//                 m_entities.alive[id] = false;

//                 // TODO: fire callbacks
//             }
//         }

//         template <typename A>
//         void init_update()
//         {
//             StorageRef& ref = get_storage<A>().ref();

//             ref.update = [this] (Id id) {
//                 apply_update<A>(id);
//             };
//         }

//         auto init_queries() -> Queries<Qs...>
//         {
//             return std::make_tuple<Qs...>((Qs(m_storages))...);
//         };

//         auto init_uniques() -> Uniques
//         {
//             return Uniques(m_storages);
//         }

//         // ---- Private access ---- //

//         template <typename A>
//         auto get_storage() -> Storage<A>&
//         {
//             std::get<Storage<A>>(m_storages);
//         }

//         template <typename A>
//         auto get_ids() -> Vec<Id>&
//         {
//             return get_storage<A>().ids();
//         }

//         template <typename A, typename C>
//         auto get_vector() -> Vec<C>&
//         {
//             return get_storage<A>().template vector<C>();
//         }

//         public:

//             Registry()
//             {
//                 (init_update<As>(),...);            
//             }

//             // ---- Metadata ---- //

//             template <typename A>
//             bool has_entities() {}
//             bool has_entities() {}
//             bool has_components() {}

//             bool is_alive(Id id) {
//                 return m_entities.alive[id];
//             }

//             bool is_locked(Id id) {
//                 return m_entities.locked[id];
//             }

//             // Checks that the id exists.
//             bool is_valid(Id id)
//             {
//                 return id < m_entities.total;
//             }

//             template <typename A>
//             bool is_type() {} 

//             template <typename A>
//             size_t count() 
//             {
//                 return get_storage<A>().count();
//             }

//             size_t count() 
//             {
//                 return m_entities.to
//             }
//             template <typename A>
//             size_t capacity() {}
//             size_t capacity() {}
//             template <typename A>
//             size_t dead_count() {}
//             size_t dead_count() {}
//             auto type() -> Type {}
//             size_t index() {} 

//             // ---- Public access ---- //

//             // Safe getter, validates entity. Gets component from type-erased data, less performant.
//             template <typename C>
//             auto view(Id id) -> Option<Ref<C>>
//             {
//                 if (is_valid(id) && is_alive(id))
//                 {
//                     return m_uniques.component<C>(m_entities.type[id], m_entities.index[id]);
//                 }
//                 else 
//                 {
//                     return std::nullopt;
//                 }
//             }
            
//             // Validate entity manually beforehand. Unsafe, fast getter.
//             template <typename A, typename C>
//             auto get(Id id) -> C&
//             {
//                 return get_storage<A>().template component<C>(id)
//             }

//             template <typename A, typename... Cs>
//             auto iter() -> StorageIterator<Cs...> {
//                 return get_storage<A>().template iterator<Cs...>();
//             } 

//             template <typename Q>
//             auto query() -> Q& {

//                 if (filter::has_type<Q, Data<Qs...>>{})
//                 {
//                     // retrieve query from tuple
//                 }
//                 else 
//                 {
//                     // create or retrieve dynamic query
//                 }

//                 return std::get<Q>(m_queries);
//             }

//             // ---- Operations ---- //

//             auto create() {}
//             void populate() {}
//             void queue() {}
//             void update() {}
//             void remove() {}
//             void lock() {}

//             // ---- Utility ---- //

//             void trim() {}
//     };  
// };
};