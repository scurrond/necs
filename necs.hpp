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

namespace NECS
{
    // //==========================================================================
    // // Filter
    // //==========================================================================  

    namespace Filter
    {
        //==============================
        // Unique type filtering
        //==============================

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

        //==============================
        // Type checking
        //==============================    

        template<typename T, typename Tuple>
        struct has_type;

        template<typename T>
        struct has_type<T, std::tuple<>> : std::false_type {};

        template<typename T, typename Head, typename... Tail>
        struct has_type<T, std::tuple<Head, Tail...>>
            : std::conditional_t
            <
                std::is_same_v<Head, T>, 
                std::true_type, has_type<T, 
                std::tuple<Tail...>>
            > {};

        template<typename T, typename... Ts>
        struct has_all_types : std::conjunction<has_type<Ts, T>...> {};

        //==============================
        // Index filtering
        //==============================

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

        template <typename Tuple, typename... Ts>
        constexpr auto Indices() 
        {
            using Indices = std::make_index_sequence<std::tuple_size_v<Tuple>>;

            return []<size_t... Is>(std::index_sequence<Is...> filter)
            {
                return filter;
            }
            (typename filter_index<Tuple, Indices, Ts...>::type{});
        }
    };
    
    // //==========================================================================
    // // Data
    // //==========================================================================  

    template <typename... Ts> // Tuple alias for convenience.
    using Data = std::tuple<Ts...>; 

    // //==========================================================================
    // // Entity
    // //==========================================================================  

    using EntityId = size_t; // Unique id for entities, used as its reference index.
    using EntityType = std::type_index; // An entity's archetype's type index.
    using EntityIndex = size_t; // The entity's location in its storage.

    enum EntityTask
    {
        KILL, // The entity should be marked as KILLED if LIVE.
        SNOOZE, // The entity should be marked as SNOOZED if LIVE.
        WAKE // The entity should be marked as AWAKE if SLEEPING.
    };

    enum EntityState
    {
        LIVE, // The entity is ready to be used and can be killed or snoozed.
        KILLED, // The entity has been marked for DEAD.
        DEAD, // The entity cannot be used or read anymore.
        SNOOZED, // The entity has been marked for SLEEP.
        SLEEPING, // The entity is put to sleep, it can still be accessed, iterated and changed, but not killed.
        AWAKE, // The entity is marked for LIVE.
    };

    const size_t STATE_COUNT = 6;

    struct EntityInfo
    {
        EntityType type = std::type_index(typeid(int));
        EntityIndex index = -1;
        EntityState state = LIVE;
    };

    struct EntityRef 
    {
        std::any data;
        EntityType type;

        EntityRef(EntityType _type)
            : type(_type) {}

        template <typename... Cs>
        EntityRef(Data<Cs&...> _data)
            : data(_data), type(std::type_index(typeid(Data<Cs...>))) {}

        template <typename A>
        bool is_type() 
        {
            return type == std::type_index(typeid(A));
        }

        bool is_empty()
        {
            return data.has_value();
        } 

        template <typename A>
        auto get()
        {
            return [this]<typename... Cs>(Data<Cs...>)
            {   
                return std::any_cast<Data<Cs&...>>(data);
            }
            (A{});
        }
    };

    struct EntityData
    {
        EntityInfo info; // Location data on the entity inside of its storage.
        std::function<void()> update = {};
        std::function<EntityRef()> ref = {};
    };

    struct Entities 
    {
        std::array<size_t, STATE_COUNT> counter; // Holds the amount of entites available by state.
        std::vector<EntityData> data; // Vector of all the entities currently in the system, at their EntityId index.
        std::vector<EntityId> dead; // Erased ids that can be reused.
        size_t to_update_end = 0;
        std::vector<EntityId> to_update; // Ids to update by the registry.
        
        auto create(EntityInfo info) -> std::pair<EntityId, EntityData&>
        {
            counter[LIVE]++;

            if (dead.size() > 0)
            {   
                counter[DEAD]--;
                EntityId id = dead.back();
                data[id] = {info};
                dead.pop_back();
                return {id, data[id]};
            }
            else 
            {
                EntityId id = data.size();
                data.push_back({info});
                return {id, data[id]};
            }
        }
        
        void update()
        {
            for (size_t i = 0; i < to_update_end; i++)
            {
                EntityId id = to_update[i];

                data[id].update();
            }

            to_update_end = 0;
        }

        template <typename Callback>
        void queue(EntityId id, EntityTask task, Callback&& callback)
        {
            auto& entity = data[id];

            EntityState req_state = 
                task == KILL || task == SNOOZE
                ? LIVE
                : SLEEPING;
                
            EntityState res_state = 
                task == KILL 
                ? KILLED 
                : task == SNOOZE
                ? SNOOZED 
                : AWAKE;

            if (entity.info.state == req_state)
            {
                if (to_update_end == to_update.size())
                {
                    to_update.push_back(id);
                }
                else 
                {
                    to_update[to_update_end] = id;
                }   

                to_update_end++;
                entity.info.state = res_state;
                counter[req_state]--;
                counter[res_state]++;

                callback(req_state, res_state);
            }
        }
    
        void execute(EntityId id, EntityTask task)
        {
            auto& entity = data[id];

            EntityState req_state = 
                task == KILL || task == SNOOZE
                ? LIVE
                : SLEEPING;
                
            EntityState res_state = 
                task == KILL 
                ? KILLED 
                : task == SNOOZE
                ? SNOOZED 
                : AWAKE;

            if (entity.info.state == req_state)
            { 
                entity.info.state = res_state;
                counter[req_state]--;
                counter[res_state]++;
                entity.update();
            }
        }
    };

    // //==========================================================================
    // // View
    // //==========================================================================  

    template <typename... Cs> // Optional tuple alias.
    using View = std::optional<Data<Cs&...>>;

    // //==========================================================================
    // // Listener
    // //==========================================================================  

    template <typename E>
    class Listener 
    {   
        bool m_ready = false;
        std::function<void(E)> m_callback = {};

        public:
            template <typename Callback>
            void subscribe(Callback&& callback)
            {
                static_assert(std::is_invocable_v<Callback, E>, "Event callback must take event as argument.");

                m_callback = callback;
                m_ready = true;
            }

            void unsubscribe()
            {
                m_ready = false;
            }

            void call(E event)
            {
                if (m_ready) m_callback(event);
            }
    };

    template <typename T>
    struct DataUpdated {};
    struct EntityCreated { EntityId id; };
    struct EntityUpdated { EntityId id; EntityState prev_state; EntityState new_state; }; 

    template <typename T>
    auto InitBuiltInListeners()
    {
        return []<typename... As>(std::tuple<As...>) 
        {
            using UniqueComponents = typename Filter::merge_types<As...>::type;

            return [] <typename... Cs> (std::tuple<Cs...>)
            {   
                return std::tuple
                <
                    Listener<EntityCreated>, 
                    Listener<EntityUpdated>, 
                    Listener<DataUpdated<As>>..., 
                    Listener<DataUpdated<Cs>>...
                >{};
            } 
            (UniqueComponents{});
        }
        (T{});
    }

    template <typename As, typename Es>
    auto InitListeners()
    {
        return [] <typename... Ts>(std::tuple<Ts...>)
        {
            using BuiltInEvents = std::invoke_result_t<decltype(InitBuiltInListeners<As>)>;
            using GameEvents = std::tuple<Listener<Ts>...>;

            return std::tuple_cat(BuiltInEvents{}, GameEvents{});
        }
        (Es{});
    }

    template <typename As, typename Es>
    using Listeners = std::invoke_result_t<decltype(InitListeners<As, Es>)>;

    // //==========================================================================
    // // Extraction
    // //==========================================================================  

    template <typename... Cs> // Type returned by iterators and queries.
    using Extraction = std::pair<EntityId, Data<Cs&...>>;

    // //==========================================================================
    // // Iterator
    // //==========================================================================  

    template <typename C>
    using IteratorVector = std::reference_wrapper<std::vector<C>>;

    template <typename... Cs>
    using IteratorData = std::tuple<IteratorVector<Cs>...>;
    
    /**
     * A single storage-level iterator.
     * Iterates through the desired components in a storage.
     * 
     * Since storages exist for the entire duration of the program, 
     * it can safely hold references to the storage's data indefinitely.
     */
    template <typename... Cs>
    struct Iterator
    {  
        IteratorVector<EntityId> ids;
        IteratorData<Cs...> data;
        std::reference_wrapper<size_t> end_index;
    
        EntityIndex current = 0;

        template <typename C>
        auto extract_one() -> C&
        {
            auto& v = std::get<IteratorVector<C>>(data).get();

            return v[current];
        }
    
        auto extract_all() -> Data<Cs&...>
        {
            return std::tie<Cs...>(extract_one<Cs>()...);
        };

        public:
            Iterator(IteratorVector<EntityId> _ids, IteratorData<Cs...> _data, size_t& _end_index)
                : ids(_ids), data(_data), end_index(_end_index) {}

            Iterator(const Iterator<Cs...>&) = default;
            Iterator(Iterator<Cs...>&&) = default;
            Iterator<Cs...>& operator=(const Iterator<Cs...>&) = default;
            Iterator<Cs...>& operator=(Iterator<Cs...>&&) = default;

            void reset()
            {
                current = 0;
            }

            bool done()
            {
                return current == end_index.get();
            }

            bool empty()
            {
                return end_index.get() == 0;
            }

            bool operator!= (const Iterator<Cs...>& other) const 
            {
                return current != other.current;
            }

            bool operator== (const Iterator<Cs...>& other) const 
            {
                return current == other.current;
            }

            auto operator*() -> Extraction<Cs...>
            {
                return {ids.get()[current], extract_all()};
            }

            Iterator<Cs...>& operator++() 
            {
                current++;

                return *this;
            }

            Iterator<Cs...>& begin() 
            {
                current = 0;
                return *this;
            }

            Iterator<Cs...>& end() 
            {
                current = end_index.get();
                return *this;
            }
    };

    // //==========================================================================
    // // Pool
    // //==========================================================================  

    template <typename A>
    auto InitPoolData()
    {   
        return []<typename... Cs>(Data<Cs...>)
        {
            return std::tuple<std::vector<Cs>...>{};
        }
        (A{});
    }

    template <typename A>
    using PoolData = std::invoke_result_t<decltype(InitPoolData<A>)>;

    template <typename A>
    class Pool
    {
        EntityIndex m_end = 0;
        EntityIndex m_total = 0;
        PoolData<A> m_data;
        std::vector<EntityId> m_ids;

        template <typename C>
        auto vector() -> std::vector<C>&
        {
            return std::get<std::vector<C>>(m_data);
        }

        template <typename C>
        void push(C& component)
        {
            vector<C>().push_back(component);
        }

        template <typename C>
        void insert(C& component)        
        {
            vector<C>()[m_end] = component;
        }

        template <typename C>
        void erase()
        {
            std::vector<C>& v = vector<C>();
            v.erase(v.begin() + m_end, v.end());
            m_total = m_end;
        }

        template <typename C>
        void swap(EntityIndex index)
        {
            auto& v = vector<C>();
            std::swap(v[index], v[m_end - 1]);
        }

        public: 
            size_t total()
            {
                return m_total;
            }

            size_t count()
            {
                return m_end;
            }

            template <typename... Cs>
            void add(EntityId id, Data<Cs...> entity)
            {
                if (m_end == m_total)
                {
                    (push<Cs>(std::get<Cs>(entity)),...);
                    m_ids.push_back(id);
                    m_total++;
                }
                else 
                {
                    (insert<Cs>(std::get<Cs>(entity)),...);
                    m_ids[m_end] = id;
                }

                m_end++;
            }

            void trim()
            {
                [this]<typename... Cs>(Data<Cs...>)
                {
                    if (m_end < m_total)
                    {
                        m_ids.erase(m_ids.begin() + m_end, m_ids.end());
                        ((erase<Cs>()),...);
                    }
                }
                (A{});
            }

            auto ids() -> const std::vector<EntityId>&
            {
                return m_ids;
            }

            auto get(EntityIndex index)
            {
                return [this, &index]<typename... Cs>(Data<Cs...>)
                {
                    return get<Cs...>(index);
                }
                (A{});
            }
        
            template <typename... Cs>
            auto get(EntityIndex index) -> Data<Cs&...>
            {
                return std::tie<Cs...>(vector<Cs>()[index]...);
            }

            auto clone(EntityIndex index)
            {
                return [this, &index]<typename... Cs>(Data<Cs...>)
                {
                    return clone<Cs...>(index);
                }
                (A{});
            }    

            template <typename... Cs>
            auto clone(EntityIndex index) -> Data<Cs...>
            {
                return get<Cs...>(index);
            }    
        
            auto remove(EntityIndex index) -> EntityId 
            {
                [this, &index]<typename... Cs>(Data<Cs...>)
                {
                    (swap<Cs>(index),...);
                    std::swap(m_ids[index], m_ids[m_end - 1]);
                    m_end--;
                }
                (A{});

                return m_ids[index];
            }

            template <typename... Cs>
            auto iter() -> Iterator<Cs...>
            {
                return Iterator<Cs...>(m_ids, std::tie(vector<Cs>()...), m_end);
            }
    };

    // //==========================================================================
    // // Storage
    // //==========================================================================  

    template <typename A>
    struct Storage
    {
        Pool<A> living;
        Pool<A> sleeping;

        template <typename... Cs>
        auto get(EntityIndex index, bool sleeping_pool) -> Data<Cs&...>
        {   
            auto& data = sleeping_pool ? sleeping : living;
            return data.template get<Cs...>(index);
        }

        template <typename... Cs>
        auto iter(bool sleeping_pool) -> Iterator<Cs...>
        {
            auto& data = sleeping_pool ? sleeping : living;
            return data.template iter<Cs...>();
        }

        auto pool(bool sleeping_pool) -> Pool<A>&
        {
            return sleeping_pool ? sleeping : living;
        }

        auto apply_kill(EntityIndex index) -> EntityId
        {
            if (living.total() == 0 || living.count() < 1 || index >= living.count())
            {
                throw std::invalid_argument("Entity marked KILLED in empty living vector or index out of bounds.");
            }
            else 
            {
                return living.remove(index);
            }
        }

        auto apply_snooze(EntityIndex index) -> EntityId
        {
            if (living.total() == 0 || living.count() < 1 || index >= living.count())
            {
                throw std::invalid_argument("Entity marked SNOOZED in empty living vector or index out of bounds.");
            }
            else 
            {
                const EntityId& id = living.ids()[index];
                sleeping.add(id, living.clone(index)); // copy entity into living
                return living.remove(index);
            }
        }

        auto apply_wake(EntityIndex index) -> EntityId
        {
            if (sleeping.total() == 0 || sleeping.count() < 1 || index >= sleeping.count())
            {
                throw std::invalid_argument("Entity marked AWAKE in empty sleeping vector or index out of bounds.");
            }
            else 
            {
                const EntityId& id = sleeping.ids()[index];
                living.add(id, sleeping.clone(index)); // copy entity into living
                return sleeping.remove(index);
            }
        }
    };
    
    template <typename As>
    auto InitStorages()
    {
        return []<typename... Ts> (std::tuple<Ts...>)
        {
            return Data<Storage<Ts>...>{};
        }
        (As{});
    };

    template <typename Tuple>
    using Storages = std::invoke_result_t<decltype(InitStorages<Tuple>)>;

    // //==========================================================================
    // // Query
    // //========================================================================== 

    template <typename... Cs> 
    using QueryData = std::vector<Iterator<Cs...>>;

    template <typename... Cs>
    class Query
    {
        bool m_initialized = false;
        QueryData<Cs...> m_living;
        QueryData<Cs...> m_sleeping;
        QueryData<Cs...>& m_data = m_living;
        std::vector<size_t> m_valid;
        size_t m_current = 0;
        size_t m_end = 0;

    public:
        bool initialized()
        {
            return m_initialized;
        }

        template <typename... As>
        void init(std::tuple<Storage<As>...>& storages)
        {   
            if (m_initialized) return; 

            auto f = [this]<typename A>(Storage<A>& storage)
            {
                if constexpr(Filter::has_all_types<A, Cs...>::value)
                {
                    m_living.push_back(storage.living.template iter<Cs...>());
                    m_sleeping.push_back(storage.sleeping.template iter<Cs...>());
                }
            };

            ((f(std::get<Storage<As>>(storages))),...);

            m_initialized = true;
        }

        // makes sure that only non-empty iterators are considered, sets data pool
        void update(bool sleeping_pool)
        {
            m_current = 0;
            m_end = 0;
            m_data = sleeping_pool ? m_sleeping : m_living;

            int size = static_cast<int>(m_data.size());

            for (int i = 0; i < size; i++)
            {
                Iterator<Cs...>& it = m_data[i];

                it.reset();

                if (!it.empty())
                {
                    if (m_end < m_valid.size())
                    {
                        m_valid[m_end] = i;
                    }
                    else 
                    {
                        m_valid.push_back(i);
                    }

                    m_end++;
                }
            }
        }

        bool operator!= (const Query<Cs...>& other) const 
        {
            return m_current != other.m_current;
        }

        auto operator*() -> Extraction<Cs...>
        {
            auto& iterator = m_data[m_valid[m_current]];

            return *iterator;
        }

        Query<Cs...>& operator++() 
        {
            auto& iterator = m_data[m_valid[m_current]];

            ++iterator;

            if (iterator.done())
            {
                m_current++;
            }

            return *this;
        }

        Query<Cs...>& begin() 
        {
            m_current = 0;

            return *this;
        }

        Query<Cs...>& end() 
        {
            m_current = m_end;

            return *this;
        }    
    };

    // //==========================================================================
    // // Debug
    // //==========================================================================  

    template <typename C>
    concept Debuggable = requires(const C& component) 
    {
        { component.debug() };
    };

    /**
     * Allows open access to entity data in development. 
     *
     * Prints debug logs for entities + components if their components are Debbugable. 
     * Since all the data is public, custom logs can be easily implemented externally.
     */
    template <typename... As>
    struct Debugger 
    {
        Entities& entities;
        std::tuple<Storage<As>...>& storages;

        bool sleeping_pool = false;

        template <typename A>
        auto storage() -> Storage<A>&
        {
            return std::get<Storage<A>>(storages);
        }

        template <typename A>
        bool is_type(EntityId id) 
        {
            return info(id).type == std::type_index(typeid(A));
        }

        auto info(EntityId id) -> const EntityInfo& 
        {
            return entities.data[id].info;
        }

        template <typename A>
        auto view_entity(EntityId id)
        {
            return [this, &id]<typename... Cs>(Data<Cs...>)
            {
                return view_components<A, Cs...>(id);
            }
            (A{});
        }

        template <typename A, typename... Cs>
        auto view_components(EntityId id) -> View<Cs...>
        {
            auto& i = info(id);

            if (i.state == DEAD || !is_type<A>(id))
            {
                return std::nullopt;
            }

            return storage<A>().template get<Cs...>(i.index, (i.state == SLEEPING || i.state == AWAKE));
        }
        
        template <typename A>
        void print_entity(EntityId id) 
        {
            if (!is_type<A>(id))
            {
                std::cout << "----------------------------------------\n";
                std::cout << "|---INCORRECT TYPE---|\n";
                std::cout << "----------------------------------------\n";       
                return;         
            }

            auto e = view_entity<A>(id);

            if (!e.has_value())
            {
                std::cout << "----------------------------------------\n";
                std::cout << "|---NOT FOUND---|\n";
                std::cout << "----------------------------------------\n";       
                return;         
            }

            auto& i = info(id);
            auto state = [&i]()
            {
                switch(i.state)
                {
                    case LIVE: return "LIVE";
                    case DEAD: return "DEAD";
                    case SLEEPING: return "SLEEPING";
                    case KILLED: return "KILLED";
                    case SNOOZED: return "SNOOZED";
                    case AWAKE: return "AWAKE";
                    default: return "ERROR";
                }
            };
            
            std::cout << "----------------------------------------\n";
            std::cout << "Entity ID: " << id
                        << " (Index: " << i.index
                        << ", State: " << state() << ")\n";
            std::cout << "Components:\n";

            auto f = [this]<typename C>(C& component)
            {
                if constexpr (Debuggable<C>)
                {
                    component.debug();
                    std::cout << "\n";
                }
                else
                {
                    std::cout << "  - " << typeid(C).name() << ":";
                    std::cout << "<not implemented>\n";
                }
            };

            [this, &f]<typename... Cs>(Data<Cs&...> entity)
            {
                (f(std::get<Cs&>(entity)),...);
            }
            (e.value());

            std::cout << "----------------------------------------\n";
        };

        template <typename A>
        void print_storage(std::string archetype_name = typeid(A).name()) 
        {
            Storage<A>& s = storage<A>();

            Pool<A>& pool = sleeping_pool ? s.sleeping : s.living;

            std::cout << "\n[Archetype: " << archetype_name << "]";
            std::cout << "\n[Storage type: " << (sleeping_pool ? "Sleeping" : "Living") << "]\n";

            if (pool.count() == 0) 
            {
                std::cout << "----------------------------------------\n";
                std::cout << "|---EMPTY---|\n";
                std::cout << "----------------------------------------\n";
                return;
            };

            for (EntityIndex index = 0; index < pool.count(); index++)
            {
                print_entity<A>(pool.ids()[index]);
            }
        };

        void print_all() 
        {
            std::cout << "\n==== NECS Debug ====\n";

            (print_storage<As>(),...);

            std::cout << "\n";
        };
    };

    // //==========================================================================
    // // Registry
    // //==========================================================================  
    /**
     * The main API / entry point for interacting with system data.
     * Contains functions for creating, removing & querying entities.
     */
    template 
    <
        typename Archetypes, 
        typename Events, 
        typename Singletons,
        typename Queries
    >
    class Registry
    {
        Entities m_entities;
        Storages<Archetypes> m_storages;
        Listeners<Archetypes, Events> m_listeners;
        Singletons m_singletons;
        Queries m_queries;
        
        template <typename A>
        auto storage() -> Storage<A>&
        {
            return std::get<Storage<A>>(m_storages);
        }

        template <typename A>
        void call()
        {
            listener<DataUpdated<A>>().call({});

            [this]<typename... Cs>(Data<Cs...>)
            {
                ((listener<DataUpdated<Cs>>().call({})),...);
            }
            (A{});
        }

        template <typename A>
        void apply(EntityId id)
        {
            auto& s = storage<A>();
            auto& [_, index, state] = m_entities.data[id].info;
            
            switch (state)
            {
                case AWAKE:
                {
                    EntityId swapped_entity = s.apply_wake(index);
                    m_entities.data[swapped_entity].info.index = index;
                    index = s.living.count() - 1;
                    break;
                };
                case KILLED: 
                {
                    EntityId swapped_entity = s.apply_kill(index);
                    m_entities.data[swapped_entity].info.index = index;
                    m_entities.dead.push_back(id);
                    break;
                };
                case SNOOZED:
                {
                    EntityId swapped_entity = s.apply_snooze(index);
                    m_entities.data[swapped_entity].info.index = index;
                    index = s.sleeping.count() - 1;
                    break;
                };
                default:
                {
                    return;
                };
            }

            EntityState req_state = state;
            EntityState res_state = 
                req_state == AWAKE 
                ? LIVE
                : req_state == KILLED
                ? DEAD
                : SLEEPING;

            m_entities.counter[req_state]--;
            m_entities.counter[res_state]++;
            state = res_state;
            if (run_callbacks) 
            {
                listener<EntityUpdated>().call({id, req_state, res_state});
                call<A>();
            }
        }

        public:
            bool run_callbacks = true; // Toggles whether internal callbacks should be called.

            template <typename A>
            bool is_type(EntityId id) 
            {
                auto& i = info(id);
                return i.type == std::type_index(typeid(A));
            }

            bool is_dead(EntityId id)
            {
                auto& i = info(id);
                return i.state == DEAD;
            }

            template <typename A>
            bool is_empty(bool sleeping_pool = false)
            {
                return count<A>(sleeping_pool) == 0;
            }

            size_t total()
            {
                return m_entities.data.size();
            }

            size_t count(EntityState state)
            {
                return m_entities.counter[state];
            }

            template <typename A>
            size_t pool_total(bool sleeping_pool = false)
            {
                Pool<A>& pool = storage<A>().pool(sleeping_pool);

                return pool.total();
            }

            template <typename A>
            size_t pool_count(bool sleeping_pool = false)
            {
                Pool<A>& pool = storage<A>().pool(sleeping_pool);

                return pool.count();
            }
 
            auto ref(EntityId id) -> EntityRef
            {
                return m_entities.data[id].ref();
            }

            auto info(EntityId id) -> const EntityInfo& 
            {
                return m_entities.data[id].info;
            }
            
            auto debugger()
            {
                return Debugger{m_entities, m_storages};
            }

            template <typename A>
            auto ids(bool sleeping_pool = false) -> const std::vector<EntityId>&
            {
                return storage<A>().data(sleeping_pool).ids;
            }

            template <typename... Cs>
            auto filter()
            {
                auto f = [] <typename... As>(std::tuple<Storage<As>&...>)
                {
                    return std::tuple<As...>{};
                };

                return [this, &f] <size_t... Is>(std::index_sequence<Is...>)
                {
                    return f(std::tie(std::get<Is>(m_storages)...));
                }
                (Filter::Indices<Archetypes, Cs...>());
            }

            template <typename S>
            auto singleton() -> S&
            {   
                return std::get<S>(m_singletons);
            }

            template <typename E>
            auto listener() -> Listener<E>&
            {
                return std::get<Listener<E>>(m_listeners);
            }

            template <typename Q>
            auto query(bool sleeping_pool = false) -> Q&
            {
                auto& q = std::get<Q>(m_queries);
                
                if (!q.initialized())
                {
                    q.init(m_storages);
                }

                q.update(sleeping_pool);

                return q;
            }

            template <typename A, typename... Cs>
            auto view(EntityId id) -> View<Cs...>
            {
                auto& i = info(id);

                if (i.state == DEAD || !is_type<A>(id))
                {
                    return std::nullopt;
                }

                return storage<A>().template get<Cs...>(i.index, (i.state == SLEEPING || i.state == AWAKE));
            }

            template <typename A, typename... Cs>
            auto get(EntityId id) -> Data<Cs&...>
            {
                auto& i = info(id);

                if (!is_type<A>(id))
                {
                    throw std::invalid_argument("Cannot perform GET with an incorrect entity type.");
                }

                if (i.state == DEAD)
                {
                    throw std::invalid_argument("Cannot perform GET on a DEAD entity. Use VIEW or FIND instead.");             
                }

                return storage<A>().template get<Cs...>(i.index, (i.state == SLEEPING || i.state == AWAKE));
            }

            template <typename... Cs>
            auto find(EntityId id) -> View<Cs...>
            {
                View<Cs...> v;

                [this, &v, &id]<typename... As>(std::tuple<As...>)
                {
                    bool found = false;

                    auto f = [this, &v, &id, &found]<typename A>(Storage<A>& s)
                    {
                        if (found) return;

                        if (is_type<A>(id))
                        {
                            found = true;
                            
                            if constexpr (Filter::has_all_types<A, Cs...>::value == true)
                            {
                                v = view<A, Cs...>(id);
                            }
                        }
                    };  
                    ((f(std::get<Storage<As>>(m_storages))),...);
                }
                (filter<Cs...>());

                return v;
            }

            template <typename A, typename... Cs>
            auto iter(bool sleeping_pool = false) -> Iterator<Cs...>
            {
                return storage<A>().template iter<Cs...>(sleeping_pool);
            }
            
            template <typename A>
            auto create(A entity) -> EntityId
            {
                Storage<A>& s = storage<A>();

                auto [id, data] = m_entities.create
                ({
                    std::type_index(typeid(A)), 
                    s.living.count(), 
                    LIVE
                });

                s.living.add(id, entity);

                data.update = [this, id] () { apply<A>(id); };

                data.ref = [this, id] ()
                {
                    return [this, &id]<typename... Cs>(Data<Cs...>)
                    {
                        auto& i = info(id);

                        if (i.state == DEAD)
                        {
                            return EntityRef(std::type_index(typeid(A)));
                        }
                        else 
                        {
                            Data<Cs&...> e = storage<A>().template get<Cs...>
                            (i.index, (i.state == SLEEPING || i.state == AWAKE));

                            return EntityRef(e);
                        }
                    }
                    (A{});
                };
    
                if (run_callbacks) 
                {
                    listener<EntityCreated>().call({id});
                    call<A>();
                }

                return id;
            }
            
            template <typename A>
            void populate(A entity, size_t count)
            {
                for (size_t i = 0; i < count; i++)
                {
                    create(entity);
                }
            }

            template <typename... Cs, typename Callback>
            void for_each(Callback&& callback, bool sleeping_pool = false)
            {
                static_assert(std::is_invocable_v<Callback, EntityId, Data<Cs&...>>, "For each callback must take EntityId, Entity<Cs&...> as argument.");

                [this, &callback, &sleeping_pool]<typename... As>(std::tuple<As...>)
                {
                    auto f = [this, &callback, &sleeping_pool]<typename A>(Storage<A>& s)
                    {
                        Pool<A>& pool = s.pool(sleeping_pool);

                        for (EntityIndex index = 0; index < pool.count(); index++)
                        {
                            callback(pool.ids()[index], pool.template get<Cs...>(index));
                        }
                    };

                    (f(storage<As>()),...);
                }
                (filter<Cs...>());
            }
           
            template <typename A>
            void trim()
            {
                Storage<A>& s = storage<A>();
                s.living.trim();
                s.sleeping.trim();
            }

            void update()
            {
                m_entities.update();
            }

            void queue(EntityId id, EntityTask task)
            {
                auto callback = [this, &id] (EntityState req_state, EntityState res_state) 
                {
                    if (run_callbacks) listener<EntityUpdated>().call({id, req_state, res_state});
                };

                m_entities.queue(id, task, callback);
            };
        
            void execute(EntityId id, EntityTask task)
            {
                m_entities.execute(id, task);
            }
    };
};
