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
    // ----------------------------------------------------------------------------
    // Filter
    // ----------------------------------------------------------------------------

    /**
     * Internal namespace for tuple helpers. 
     * Contains structs and methods for filtering tuples, constructing new ones
     * and creating index sequences.
     */
    namespace Filter
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
                std::is_same_v<T, Head>, 
                std::true_type, 
                has_type<T, std::tuple<Tail...>>
            > {};

        template<typename T, typename... Ts>
        struct has_all_types : std::conjunction<has_type<Ts, T>...> {};

        template<typename T, typename... Ts>
        struct has_any_type : std::disjunction<has_type<Ts, T>...> {};

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

        template <
            typename Tuple, 
            std::size_t I, 
            std::size_t... Is, 
            typename... WithTs, 
            typename... WithoutTs>
        struct match_index
        <
            Tuple, std::index_sequence<I, Is...>, 
            std::tuple<WithTs...>, 
            std::tuple<WithoutTs...>
        > 
        {
            using Tail = typename match_index
            <
                Tuple, 
                std::index_sequence<Is...>, 
                std::tuple<WithTs...>, 
                std::tuple<WithoutTs...>
            >::type;

            using CurrentTuple = std::tuple_element_t<I, Tuple>;

            static constexpr bool match =
                has_all_types<CurrentTuple, WithTs...>::value &&
                !has_any_type<CurrentTuple, WithoutTs...>::value;

            using type = std::conditional_t
            <
                match,
                typename Filter::concat_index<I, Tail>::type,
                Tail
            >;
        };

        template <typename Tuple, typename With, typename Without>
        struct match_index<Tuple, std::index_sequence<>, With, Without> {
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

        template <typename Tuple, typename With = std::tuple<>, typename Without = std::tuple<>>
        using Match = match_index<Tuple, std::make_index_sequence<std::tuple_size_v<Tuple>>, With, Without>::type;
    };
    
    // ----------------------------------------------------------------------------
    // Data & View
    // ----------------------------------------------------------------------------

    /**
     * Tuple alias for convenience.
     * Used to create archetypes and registry templates with.
     */
    template <typename... Ts>
    using Data = std::tuple<Ts...>; 

    template <typename T, template <typename...> class OuterWrapper, template <typename> class InnerWrapper>
    struct WrapData;
    
    template <
        template <typename...> class OuterWrapper,
        template <typename> class InnerWrapper,
        typename... Ts
    >
    struct WrapData<Data<Ts...>, OuterWrapper, InnerWrapper> {
        using type = OuterWrapper<InnerWrapper<Ts>...>;
    };

    /**
     * Optional data alias.
     * Used in VIEW functions for safety.
     */    
    template <typename... Cs> 
    using View = std::optional<Data<Cs&...>>;


    // ----------------------------------------------------------------------------
    // Entity
    // ---------------------------------------------------------------------------- 

    // TODO: make aliases more generic and move them up 

    /**
     * A unique index associated with an entity. It will remain contant across 
     * the entity's lifetime. Dead entities have their ids reused if they aren't
     * id_locked (made life-time unique).
     */
    using EntityId = size_t;

    /**
     * The resulting type info object obtained from std::type_index(typeid(A)).
     * Used to check the entity's archetype at runtime when all you have is the id.
     */
    using EntityType = std::type_index;

    /**
     * The entity's index inside of its pool. This is different to its id, 
     * as it can change at any point. 
     */
    using EntityIndex = size_t;

    /**
     * The action to perform on an entity, either right away or delayed.
     */
    enum EntityTask
    {
        KILL, // The entity should be marked as KILLED if LIVE.
        SNOOZE, // The entity should be marked as SNOOZED if LIVE.
        WAKE // The entity should be marked as AWAKE if SLEEPING.
    };

    /**
     * The current state of the entity. It carries location info (pool) and tracks
     * what tasks are allowed for this entity.
     *
     * KILLED, SNOOZED and AWAKE are pending states that signal that an entity is 
     * about to change locations on next update (either pool or index). These states 
     * cannot be changed until they are processed. LIVE and SLEEPING are stable 
     * states that allow for entities to be updated via an EntityTask.
     * 
     * LIVE, SNOOZED and KILLED entities are located in the living pool of their 
     * storage. SLEEPING and AWAKE entities are located in the sleeping pool.
     * 
     * DEAD entities are removed from the system and are not updated. Their 
     * information can still be accessed if the storage hasn't overwritten it or 
     * that memory has not been freed, but this is undefined behavior.
     */
    enum EntityState
    {
        LIVE, // The entity is ready to be used and can be killed or snoozed.
        KILLED, // The entity has been marked for DEAD.
        DEAD, // The entity cannot be used or read anymore.
        SNOOZED, // The entity has been marked for SLEEP.
        SLEEPING, // The entity is put to sleep, it can still be accessed, iterated and changed, but not killed.
        AWAKE, // The entity is marked for LIVE.
    };

    // The total number of EntityStates.
    const size_t STATE_COUNT = 6;

    /**
     * A struct containing some location info on an entity. 
     * TODO: trash this and use vectors for each value in Entities.
     */
    struct EntityInfo
    {
        EntityType type = std::type_index(typeid(int));
        EntityIndex index = -1;
        EntityState state = LIVE;
        bool id_locked = false;
    };

    /**
     * A type-erased reference to the entire entity at this id. 
     * Its wll be empty if the entity is DEAD.
     * 
     * TODO: remove this, memory overhead for no reason.
     * Better to use find.
     */
    struct EntityRef 
    {
        std::any data;
        EntityType type;

        EntityRef(EntityType _type)
            : type(_type) {}

        template <typename... Cs>
        EntityRef(Data<Cs&...> _data)
            : data(_data), type(std::type_index(typeid(Data<Cs...>))) {}

        // Archetype check.
        template <typename A>
        bool is_type() 
        {
            return type == std::type_index(typeid(A));
        }

        // Value check.
        bool is_empty()
        {
            return !data.has_value();
        } 

        // Extracts entity. Check if valid before you do this.
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

    /**
     * A struct containing an info struct and utility methods. 
     * TODO: trash this and use vectors for each value in Entities.
     */
    struct EntityData
    {
        EntityInfo info; // Location data on the entity inside of its storage.
        std::function<void()> update = {};
        std::function<EntityRef()> ref = {};
    };

    /**
     * Entity metadata manager class. 
     * Contains type-erased data and manages EntityId allocations.
     * 
     * It is always called on internally and is not exposed. 
     * 
     * TODO: change dead to to_reuse, as that is more descriptive.
     * TODO: move logic to registry to respect the centralized design as much as 
     * possible. It doesn't need to be separated.
     */
    struct Entities 
    {
        size_t to_update_end = 0; 
        std::vector<EntityId> to_update; // Ids to update by the registry.
        std::vector<EntityId> dead; // Erased ids that can be reused.
        std::vector<EntityData> data; // Vector of all the entities currently in the system, at their EntityId index.

        std::array<size_t, STATE_COUNT> counter; // Holds the amount of entites available by state.
        
        /**
         * Ties an id and metadata to a new entity. An id will be reused if available.
         * 
         * @param info The new entity's location info. 
         * 
         * @returns The new id and metadata object. 
         */
        auto create(EntityInfo info) -> std::pair<EntityId, EntityData&>
        {
            //TODO: change this to info.state
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


        // Updates all queued entities.
        void update()
        {
            for (size_t i = 0; i < to_update_end; i++)
            {
                EntityId id = to_update[i];

                data[id].update();
            }

            to_update_end = 0;
        }

        /**
         * Queues an entity to be updated. 
         * 
         * @tparam Callback the type of the callback to be invoked.
         * 
         * @param id The entity to queue.
         * @param task The type of task to queue for. 
         * @param callback The event callback to call on success.
         * 
         * TODO: Static assert on callback.
         */
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
    
        /**
         * Executes a state change for an entity instantly. 
         * 
         * @param id The entity to change.
         * @param task The type of task to execute.
         */
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


    // ----------------------------------------------------------------------------
    // Listener & Event
    // ---------------------------------------------------------------------------- 

    /**
     * Single-callback event listener class.
     * Contains functions for subscribing, unsubscribing and calling events.
     * The listener is always called on internally and is not exposed.
     * 
     * @tparam E The event to listen to.
     * 
     * TODO: make into struct and move logic into registry.
     * TODO: separate function initialized and subscription booleans.
     */
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

    // Empty, built-in templated event for A and C, fired whenever data moves around in the registry.
    template <typename T>
    struct DataUpdated {};
    // Built-in event fired on entity creation. Contains the new entity's id.
    struct EntityCreated { EntityId id; };
    // Built-in event fired on state changes. Contains the id, the previous and new states.
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

    // ----------------------------------------------------------------------------
    // Extraction
    // ---------------------------------------------------------------------------- 

    /**
     * Type returned by iterators and queries.
     * Pair consisting of an EntityId copy and a tuple of component references.
     * 
     * @tparam Cs... The extracted components.
     */
    template <typename... Cs>
    using Extraction = std::pair<EntityId, Data<Cs&...>>;

    // ----------------------------------------------------------------------------
    // Iterator
    // ---------------------------------------------------------------------------- 
    
    /**
     * A pool-level iterator.
     * 
     * It contains references to the desired component vectors. 
     * Since storages & their pools exist for the entire duration of the program, 
     * it can safely hold references to the storage's data indefinitely.
     * 
     * @tparam Cs... The components to iterate through.
     */
    template <typename... Cs>
    struct Iterator
    {  
        template <typename C>
        using IteratorVector = std::vector<C>*;
    
        using IteratorData = std::tuple<IteratorVector<Cs>...>;
    
        private: 
            std::vector<EntityId>* m_ids = nullptr;
            IteratorData m_data;
            size_t* m_end = nullptr;
        
            EntityIndex m_current = 0;
        
            template <typename C>
            auto extract_one() -> C&
            {
                return (*std::get<IteratorVector<C>>(m_data))[m_current];
            }
        
            auto extract_all() -> Data<Cs&...>
            {
                return std::tie(extract_one<Cs>()...);
            }
        
        public: 
            Iterator() = default;
        
            Iterator(std::vector<EntityId>* _ids, IteratorData _data, size_t* _end)
                : m_ids(_ids), m_data(_data), m_end(_end) {}
                        
            bool done() const { return m_current == *m_end; }
        
            bool empty() const { return *m_end == 0; }
            
            auto operator*() -> Extraction<Cs...>
            {
                return {(*m_ids)[m_current], extract_all()};
            }
        
            bool operator!=(const Iterator<Cs...>& other) const 
            {
                return m_current != other.m_current;
            }
        
            bool operator==(const Iterator<Cs...>& other) const 
            {
                return m_current == other.m_current;
            }
        
            Iterator<Cs...>& operator++() 
            {
                ++m_current;
                return *this;
            }
        
            Iterator<Cs...>& begin() 
            {
                m_current = 0;
                return *this;
            }
        
            Iterator<Cs...>& end() 
            {
                m_current = *m_end;
                return *this;
            }
    };

    // ----------------------------------------------------------------------------
    // Pool
    // ---------------------------------------------------------------------------- 

    /**
     * Base archetype storage class.
     * Contains functions for adding, removing and retrieving component data,
     * as well as an iterator and utility functions.
     * 
     * The pool is always called on internally and is not exposed.
     * 
     * @tparam A The archetype of the pool.
     */
    template <typename A>
    class Pool
    {
        // A tuple of std::vector<C> for each component of the archetype.
        using PoolData = WrapData<A, Data, std::vector>::type;

        EntityIndex m_end = 0;
        EntityIndex m_total = 0;
        PoolData m_data;
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
                auto ids_ptr = &m_ids;
                auto end_ptr = &m_end;
            
                auto data = std::make_tuple(&vector<Cs>()...);
            
                return Iterator<Cs...>(ids_ptr, data, end_ptr);
            }
    };

    // ----------------------------------------------------------------------------
    // Storage
    // ---------------------------------------------------------------------------- 

    /**
     * Top-level archetype container. 
     * 
     * Each storage contains a living and sleeping pool, as well as functions for 
     * interacting with them. It also has some functions for applying updates to 
     * an entity's state.
     * 
     * @tparam A The archetype of this storage.
     * 
     * TODO: Move apply logic to registry's apply function, it doesn't need to be here. 
     * The iter and get functions can also be removed and replaced with a private pool function in 
     * the registry. The storage can just be a glorified double pool container.
     */
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
    using Storages = WrapData<As, Data, Storage>::type;

    // ----------------------------------------------------------------------------
    // Query
    // ---------------------------------------------------------------------------- 

    template <typename... Cs>
    class Query
    {
        struct Call 
        {
            std::function<Iterator<Cs...>()> iter = {};
        };

        std::vector<Call> m_data;
        Iterator<Cs...> m_iterator;
        size_t m_current = 0;
        bool m_sleeping_pool;

        void advance()
        {
            m_current++;

            while (m_current < m_data.size())
            {
                m_iterator = m_data[m_current].iter();
                
                if (m_iterator.empty())
                {
                    m_current++;
                }
                else 
                {
                    return;
                }
            }
        }

        public: 
            template <typename... As>
            Query(Data<Storage<As>&...> storages, bool sleeping_pool) 
            {
                m_sleeping_pool = sleeping_pool;

                auto f = [this]<typename A>(Storage<A>& storage)
                {
                    Call call; 

                    call.iter = [this, &storage] ()
                    {
                        return storage.template iter<Cs...>(m_sleeping_pool);
                    };

                    m_data.push_back(call);
                };

                ((f(std::get<Storage<As>&>(storages))),...);

                if (m_data.size() == 0)
                {
                    throw std::runtime_error("@Query::Query - no archetypes match. This query is redundant.");
                }
            };

            size_t size()
            {
                return m_data.size();
            }

            auto iter(size_t index) -> Iterator<Cs...>
            {
                return m_data[index].iter();
            }

            template <typename Callback>
            void for_each(Callback&& callback, bool sleeping_pool = false)
            {
                static_assert(std::is_invocable_v<Callback, Extraction<Cs...>>, "For each callback must take Extraction<Cs...> as argument.");

                for (size_t i = 0; i < size(); i++)
                {
                    for (auto e : iter(i))
                    {
                        callback(e);
                    }
                }
            }

            bool operator!= (const Query<Cs...>& other) const 
            {
                return m_current != other.m_current;
            }
    
            auto operator*() -> Extraction<Cs...>
            {    
                return *m_iterator;
            }
    
            Query<Cs...>& operator++() 
            {    
                ++m_iterator;
    
                if (m_iterator.done()) advance();
    
                return *this;
            }

            Query<Cs...>& begin() 
            {
                m_current = 0;
    
                m_iterator = m_data[m_current].iter();
        
                if (m_iterator.empty()) advance();
    
                return *this;
            }
    
            Query<Cs...>& end() 
            {
                m_current = m_data.size();
    
                return *this;
            }    
    };  

    // ----------------------------------------------------------------------------
    // Debug
    // ---------------------------------------------------------------------------- 

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

    // ----------------------------------------------------------------------------
    // Registry
    // ---------------------------------------------------------------------------- 

    /**
     * The main API / entry point for interacting with system data.
     * Contains functions for creating, removing & querying entities.
     */
    template 
    <
        typename Archetypes, 
        typename Events, 
        typename Singletons
    >
    class Registry
    {
        Entities m_entities;
        Storages<Archetypes> m_storages;
        Listeners<Archetypes, Events> m_listeners;
        Singletons m_singletons;

        bool m_run_callbacks = true;
        
        template <typename A>
        auto storage() -> Storage<A>&
        {
            return std::get<Storage<A>>(m_storages);
        }

        template <typename E>
        auto listener() -> Listener<E>&
        {
            return std::get<Listener<E>>(m_listeners);
        }

        template <typename A>
        void on_update()
        {
            call<DataUpdated<A>>({});

            [this]<typename... Cs>(Data<Cs...>)
            {
                ((call<DataUpdated<Cs>>({})),...);
            }
            (A{});
        }

        template <typename A>
        void apply(EntityId id)
        {
            auto& s = storage<A>();
            auto& [_, index, state, id_locked] = m_entities.data[id].info;
            
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
                    if (!id_locked)
                    {
                        m_entities.dead.push_back(id);
                    }
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

            if (m_run_callbacks) 
            {
                call<EntityUpdated>({id, req_state, res_state});
                on_update<A>();
            }
        }

        template <typename Ws = Data<>, typename Wos = Data<>>
        auto match()
        {
            return [this] <size_t... Is>(std::index_sequence<Is...>)
            {
                return std::tie(std::get<Is>(m_storages)...);
            }
            (Filter::Match<Archetypes, Ws, Wos>{});
        }

        public:
            // Toggles whether internal callbacks should be called.
            void toggle_callbacks(bool value)
            {
                m_run_callbacks = value;
            };

            /**
             * Performs a runtime typecheck on the entity.
             * 
             * @tparam A The archetype to check.
             * 
             * @param id The entity to check.
             * 
             * @returns True is the type matches the type in the entity 
             * metadata, false if not.
             */
            template <typename A>
            bool is_type(EntityId id) 
            {
                auto& i = info(id);
                return i.type == std::type_index(typeid(A));
            }

            /**
             * TODO: remove, useless, use is_state
             */
            bool is_dead(EntityId id)
            {
                auto& i = info(id);
                return i.state == DEAD;
            }

            /**
             * Performs a state check on the entity.
             * 
             * @param id The entity to check.
             * @param state The state to check.
             * 
             * @returns True if state matches the state in the entity 
             * metadata, false if not.
             */
            bool is_state(EntityId id, EntityState state)
            {
                auto& i = info(id);
                return i.state == state;
            }

            /**
             * Performs a check on the pool.
             * 
             * @tparam A The archetype pool to check.
             * 
             * @param sleeping_pool False by default, checks sleeping pool if 
             * true, living pool if false.
             * 
             * @returns True if the pool's non-dead entities are 0, false
             * if not.
             */
            template <typename A>
            bool is_empty(bool sleeping_pool = false)
            {
                //TODO: fix bug or change pool_count & pool_total to count and total
                return count<A>(sleeping_pool) == 0;
            }

            /**
             * Gets the number of total entities of any state currently in the system.
             */
            size_t total()
            {
                return m_entities.data.size();
            }
            
            /**
             * Gets the number of total entities in a state currently in the system.
             * 
             * @param state The state to check.
             * 
             * TODO: change name to total
             */
            size_t count(EntityState state)
            {
                return m_entities.counter[state];
            }

            /**
             * Gets the total capacity of a pool, iterable and dead.
             * 
             * @tparam A The archetype pool to check.
             * 
             * @param sleeping_pool False by default, checks sleeping pool if 
             * true, living pool if false.
             * 
             * TODO: change name to total
             */
            template <typename A>
            size_t pool_total(bool sleeping_pool = false)
            {
                Pool<A>& pool = storage<A>().pool(sleeping_pool);

                return pool.total();
            }

            /**
             * Gets the iterable number of entities in a pool.
             * 
             * @tparam A The archetype pool to check.
             * 
             * @param sleeping_pool False by default, checks sleeping pool if 
             * true, living pool if false.
             * 
             * TODO: change name to total
             */
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

            /**
             * Gets the entity's location info.
             * 
             * TODO: Separate into specific functions for index, state, id_locked and type. 
             */
            auto info(EntityId id) -> const EntityInfo& 
            {
                if (id >= total())
                {
                    std::cout << "Invalid EntityId: " << id;
                    throw std::invalid_argument("Invalid EntityId");
                }

                return m_entities.data[id].info;
            }
            
            /**
             * TODO: update documentation at some point.
             */
            auto debugger()
            {
                return Debugger{m_entities, m_storages};
            }

            /**
             * TODO: check if necessary.
             */
            template <typename A>
            auto ids(bool sleeping_pool = false) -> const std::vector<EntityId>&
            {
                return storage<A>().pool(sleeping_pool).ids();
            }

            /**
             * Gets a singleton.
             * 
             * @tparam S The singleton's type.
             * 
             * @returns A reference to the singleton.
             */
            template <typename S>
            auto singleton() -> S&
            {   
                return std::get<S>(m_singletons);
            }

            /**
             * Gets a single query.
             * Initializes and updates the query, prepares it for the
             * correct pool.
             * 
             * @tparam Q The query to retrieve. This must be a Query class.
             * 
             * @param sleeping_pool False by default, updates the query
             * for the sleeping pool if true, living pool if false. 
             * 
             * @returns A reference to the query. Copy the query if
             * a nested query is desired.
             */
            // template <typename Q>
            // auto query(bool sleeping_pool = false) -> Q&
            // {
            //     auto& q = std::get<Q>(m_queries);

            //     q.toggle_pool(sleeping_pool);

            //     return q;
            // }

            template <typename... Cs>
            auto query(bool sleeping_pool = false) -> Query<Cs...>
            {
                return Query<Cs...>(match<Data<Cs...>>(), sleeping_pool);
            }

            template <typename With, typename... Cs>
            auto query_with(bool sleeping_pool = false) -> Query<Cs...>
            {
                return [this, &sleeping_pool]<typename... Ws>(Data<Ws...>)
                {
                    return Query<Cs...>(match<Data<Cs..., Ws...>>(), sleeping_pool);
                }
                (With{});
            }

            template <typename Without, typename... Cs>
            auto query_without(bool sleeping_pool = false) -> Query<Cs...>
            {
                return Query<Cs...>(match<Data<Cs...>, Without>(), sleeping_pool);
            }

            template <typename With, typename Without, typename... Cs>
            auto query_with_without(bool sleeping_pool = false) -> Query<Cs...>
            {
                return [this, &sleeping_pool]<typename... Ws>(Data<Ws...>)
                {
                    return Query<Cs...>(match<Data<Cs..., Ws...>, Without>(), sleeping_pool);
                }
                (With{});            
            }

            /**
             * A safe, single-access component lookup.
             * 
             * @tparam A The archetype to check. 
             * @tparam Cs... The components to filter for. 
             * 
             * @param id The entity to view.
             * 
             * @returns A View: nullopt if the Archetype is incorrect or the entity is dead, 
             * a tuple of component references if correct.
             */
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

            /**
             * An unsafe, single-access component lookup. Panics.
             * 
             * @tparam A The archetype to check. 
             * @tparam Cs... The components to filter for. 
             * 
             * @param id The entity to get.
             * 
             * @throws The Archetype does not match the type in the metadata.
             * @throws The entity is dead.
             * 
             * @returns A tuple of component references.
             */
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

            /**
             * A super safe lookup. 
             * 
             * Filters for and iterates over all the matching archetypes to find
             * the desired components.
             * 
             * @tparam Cs... The components to filter for. 
             * 
             * @param id The entity to find.
             * 
             * @returns A View: nullopt if the Archetype is incorrect or the entity is dead, 
             * a tuple of component references if correct.
             */
            template <typename... Cs>
            auto find(EntityId id) -> View<Cs...>
            {
                View<Cs...> v;

                [this, &v, &id]<typename... As>(Data<Storage<As>&...> storages)
                {
                    bool found = false;

                    auto f = [this, &v, &id, &found]<typename A>(Storage<A>& s)
                    {
                        if (found) return;

                        if (is_type<A>(id))
                        {
                            found = true;

                            auto& i = info(id);

                            if (i.state != DEAD)
                            {
                                v = s.template get<Cs...>(i.index, (i.state == SLEEPING || i.state == AWAKE));
                            }
                        }
                    };  

                    ((f(std::get<Storage<As>&>(storages))),...);
                }
                (match<Data<Cs...>>());

                return v;
            }

            /**
             * Constructs an iterator for a single pool.
             * 
             * @tparam A The archetype storage to check. 
             * @tparam Cs... The components to filter for. 
             * 
             * @param sleeping_pool False by default, iterates the
             * sleeping pool if true, living pool if false.              
             *  
             * @returns The pool's iterator.
             */
            template <typename A, typename... Cs>
            auto iter(bool sleeping_pool = false) -> Iterator<Cs...>
            {
                return storage<A>().template iter<Cs...>(sleeping_pool);
            }
            
            /**
             * Creates an entity.
             * Create operations change memory instantly for their affected pool.
             * 
             * @tparam A The archetype of the entity passed in.
             * 
             * @param entity The entity to add.
             * @param id_locked Should this entity's id be prevented from being reused.
             * 
             * @returns The new entity's id.
             */
            template <typename A>
            auto create(A entity, bool id_locked = false) -> EntityId
            {
                Storage<A>& s = storage<A>();

                auto [id, data] = m_entities.create
                ({
                    std::type_index(typeid(A)), 
                    s.living.count(), 
                    LIVE,
                    id_locked
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
    
                if (m_run_callbacks) 
                {
                    call<EntityCreated>({id});
                    on_update<A>();
                }

                return id;
            }
            
            /**
             * Populates the registry with copies of an entity.
             * Calls create under the hood and therefore instantly modifies memory.
             * 
             * @tparam A The archetype of the entity passed in.
             * 
             * @param entity The entity to add.
             * @param count The amount of entities to create.
             */
            template <typename A>
            void populate(A entity, size_t count)
            {
                for (size_t i = 0; i < count; i++)
                {
                    create(entity);
                }
            }

            /**
             * A dynamic alternative to queries.
             * Iterates through all the storages in the system that match the components.
             * 
             * @tparam Cs... The components to filter for. 
             * @tparam Callback Must be invocable<EntityId, Data<Cs&...>.
             * 
             * @param callback The callback to execute for each entity.
             * @param sleeping_pool False by default, iterates the
             * sleeping pool if true, living pool if false.   
             * 
             * @throws Callback is not invocable<EntityId, Data<Cs&...>.
             *              
             */
            template <typename... Cs, typename Callback>
            void for_each(Callback&& callback, bool sleeping_pool = false)
            {
                static_assert(std::is_invocable_v<Callback, Extraction<Cs...>>, "For each callback must take Extraction<Cs...> as argument.");

                [this, &callback, &sleeping_pool]<typename... As>(Data<Storage<As>&...> storages)
                {
                    auto f = [this, &callback, &sleeping_pool]<typename A>(Storage<A>& s)
                    {
                        for (auto extraction : s.template iter<Cs...>(sleeping_pool)) 
                        {
                            callback(extraction);
                        }
                    };

                    (f(std::get<Storage<As>&>(storages)),...);
                }
                (match<Data<Cs...>>());
            }
           
            // Trims dead memory from the end of each pool.
            template <typename A>
            void trim()
            {
                Storage<A>& s = storage<A>();
                s.living.trim();
                s.sleeping.trim();
            }

            /**
             * Adds a callback to an event listener.
             * 
             * @tparam E The event of the listener. 
             * @tparam Callback invocable<E>. 
             * 
             * @param callback The function to call for this event.
             * 
             * @throws Callback is not invocable<E>. 
             */
            template <typename E, typename Callback>
            void subscribe(Callback&& callback)
            {
                listener<E>().subscribe(callback);
            }

            /**
             * Marks a callback as not ready.
             * 
             * @tparam E The event of the listener.
             */
            template <typename E>
            void unsubscribe()
            {
                listener<E>().unsubscribe();
            }

            /**
             * Calls the listener callback for this event.
             * 
             * @tparam E The event of the listener.
             * 
             * @param event The event to pass to the callback.
             */
            template <typename E>
            void call(E event)
            {
                listener<E>().call(event);
            }

            // Updates all queued entities in the system.
            void update()
            {
                m_entities.update();
            }

            /**
             * Queues an entity to be updated. 
             * This is used when changes are made during iterations to
             * preserve the order of entities.
             * 
             * @param id The entity to queue.
             * @param task The type of task to queue for. 
             *              
             * */
            void queue(EntityId id, EntityTask task)
            {
                auto callback = [this, &id] (EntityState req_state, EntityState res_state) 
                {
                    if (m_run_callbacks) call<EntityUpdated>({id, req_state, res_state});
                };

                m_entities.queue(id, task, callback);
            };
        

            /**
             * Executes a state change for an entity instantly. 
             * This will reallocate memory and swap the entity around. Avoid using
             * during iterations.
             * 
             * @param id The entity to change.
             * @param task The type of task to execute.
             */
            void execute(EntityId id, EntityTask task)
            {
                m_entities.execute(id, task);
            }
    };
};
