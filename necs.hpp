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
        struct id_locked_types;

        template<>
        struct id_locked_types<> { using type = std::tuple<>; };

        template<typename T, typename... Rest>
        struct id_locked_types<T, Rest...> 
        {
            using Tail = typename id_locked_types<Rest...>::type;

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
                using type = typename id_locked_types<Ts...>::type;
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
            using id_lockedComponents = typename Filter::merge_types<As...>::type;

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
            (id_lockedComponents{});
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

    // TODO: stop using this, std::vector<C>& is good enough. 
    // TODO: remove reference wrappers and use pure references, iterators
    // should not be movable
    template <typename C>
    using IteratorVector = std::reference_wrapper<std::vector<C>>;

    template <typename... Cs>
    using IteratorData = std::tuple<IteratorVector<Cs>...>;
    
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

        // TODO: make it less janky, you don't need public here
        public:
            Iterator(IteratorVector<EntityId> _ids, IteratorData<Cs...> _data, size_t& _end_index)
                : ids(_ids), data(_data), end_index(_end_index) {}

            Iterator(const Iterator<Cs...>&) = default;
            Iterator(Iterator<Cs...>&&) = default;
            Iterator<Cs...>& operator=(const Iterator<Cs...>&) = default;
            Iterator<Cs...>& operator=(Iterator<Cs...>&&) = default;

            // Called to manually reset the iterator.
            void reset()
            {
                current = 0;
            }

            // Manual check if the iterator is done.
            // TDOO: change to is_done
            bool done()
            {
                return current == end_index.get();
            }

            // Manual check if the iterator contains any data.
            // TDOO: change to is_empty
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

    // ----------------------------------------------------------------------------
    // Pool
    // ---------------------------------------------------------------------------- 

    template <typename A>
    auto InitPoolData()
    {   
        return []<typename... Cs>(Data<Cs...>)
        {
            return std::tuple<std::vector<Cs>...>{};
        }
        (A{});
    }

    // A tuple of std::vector<C> for each component of the archetype.
    template <typename A>
    using PoolData = std::invoke_result_t<decltype(InitPoolData<A>)>;

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

    // ----------------------------------------------------------------------------
    // Query
    // ---------------------------------------------------------------------------- 

    // Struct for wrapping query components to return during queries.
    template <typename... Cs>
    struct For {};

    // Struct for wrapping query components to check for during queries.
    // Repeating the same types as in For is undefined behavior.
    template <typename... Cs>
    struct With {};

    // Struct for wrapping query components to exclude during queries.
    // This filters for archetypes that do not have to components in the template.
    // Repeating the same types as in For is undefined behavior.
    template <typename... Cs>
    struct Without {};

    template <typename ForP>
    auto InitQueryData()
    {
        return []<typename... Cs>(For<Cs...>)
        {
            return std::vector<Iterator<Cs...>>{};
        }
        (ForP{});
    };

    template <typename ForP> 
    using QueryData = std::invoke_result_t<decltype(InitQueryData<ForP>)>;

    /**
     * A pre-configured class containing references to all the component vectors in 
     * the system that match the template filter. These are stored in iterators.
     * 
     * Queries are meant to be chunk iterators and can return an iterator 
     * corresponding to a single pool at a time. They can be used to spread
     * iterations across multiple threads or for a flattened for each loop.
     * 
     * The references to component vectors will exist for the entire duration of 
     * the program, so they are safe to keep. 
     * 
     * @tparam Cs The components to iterate through.
     * 
     * 
     * TODO: Include a way of generating fresh queries on the fly.
     * TODO: Include With and Without filters.
     */
    template 
    <
        typename ForP = For<>, 
        typename WithP = With<>, 
        typename WithoutP = Without<>
    >
    class Query
    {
        QueryData<ForP> m_living;
        QueryData<ForP> m_sleeping;
        size_t m_current = 0;
        std::reference_wrapper<QueryData<ForP>> m_data = m_living;

        void advance()
        {
            m_current++;

            while (m_current < m_data.get().size())
            {
                auto& iterator = m_data.get()[m_current];
                iterator.reset();    
                
                if (iterator.empty())
                {
                    m_current++;
                }
                else 
                {
                    return;
                }
            }
        }

        template <typename... Cs>
        auto chunk(size_t index, For<Cs...>) -> Iterator<Cs...>&
        {
            return m_data.get()[index];
        }

    public:
        Query() {}

        /**
         * Populates the query with storages that match the filter. 
         * 
         * @tparam As... All the matching archetypes in the system. 
         * 
         * @param storages All the matched storages in the system.
         * 
         * @throws Query data is empty. At least 1 storage must match the filter, else the
         * query is redundant and should be removed.
         */
        template <typename... As>
        Query(Data<Storage<As>&...> storages) 
        {
            auto f = [this]<typename A, typename... Cs>(Storage<A>& storage, For<Cs...>)
            {
                m_living.push_back(storage.living.template iter<Cs...>());
                m_sleeping.push_back(storage.sleeping.template iter<Cs...>());
            };

            ((f(std::get<Storage<As>&>(storages), ForP{})),...);

            if (m_living.size() == 0 || m_sleeping.size() == 0) 
            {
                throw std::runtime_error
                ("@ Query::init: query data cannot be empty, the query is redundant. At least 1 storage must match the filter.");
            }

            m_data = m_living;
        }

        size_t iter_count()
        {
            return m_data.get().size();
        }

        auto& iter(size_t index) 
        {
            return chunk(index, ForP{});
        }
 
        // Sets pool to living (false) : sleeping (true)
        void toggle_pool(bool sleeping_pool)
        {
            m_current = 0;
            m_data = sleeping_pool ? m_sleeping : m_living;
        }

        bool operator!= (const Query<ForP, WithP, WithoutP>& other) const 
        {
            return m_current != other.m_current;
        }

        auto operator*()
        {
            auto& iterator = iter(m_current);

            return *iterator;
        }

        Query<ForP, WithP, WithoutP>& operator++() 
        {
            auto& iterator = iter(m_current);

            ++iterator;

            if (iterator.done()) advance();

            return *this;
        }

        Query<ForP, WithP, WithoutP>& begin() 
        {
            m_current = 0;

            auto& iterator = iter(m_current);

            iterator.reset();

            if (iterator.empty()) advance();

            return *this;
        }

        Query<ForP, WithP, WithoutP>& end() 
        {
            m_current = m_data.get().size();

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
            if (run_callbacks) 
            {
                call<EntityUpdated>({id, req_state, res_state});
                on_update<A>();
            }
        }

        template <typename Wis = Data<>, typename Wos = Data<>>
        auto match()
        {
            return [this] <size_t... Is>(std::index_sequence<Is...>)
            {
                return std::tie(std::get<Is>(m_storages)...);
            }
            (Filter::Match<Archetypes, Wis, Wos>{});
        }

        public:
            Registry()
            {
                [this] <typename... Qs>(Data<Qs...>& qs)
                {
                    auto f = [this]
                    <typename... Cs, typename... Ws, typename... Wos>
                    (Query<For<Cs...>, With<Ws...>, Without<Wos...>>& q)
                    {
                        auto storages = match<Data<Cs..., Ws...>, Data<Wos...>>();
                        q = Query<For<Cs...>, With<Ws...>, Without<Wos...>>(storages);
                    };

                    (f(std::get<Qs>(qs)),...);
                }
                (m_queries);
            }

            // TODO: change to toggle_callbacks function and make bool private
            bool run_callbacks = true; // Toggles whether internal callbacks should be called.

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
            template <typename Q>
            auto query(bool sleeping_pool = false) -> Q&
            {
                auto& q = std::get<Q>(m_queries);

                q.toggle_pool(sleeping_pool);

                return q;
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
    
                if (run_callbacks) 
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
             * TODO: allow for several callback overloads.
             */

            template <typename... Cs, typename Callback>
            void for_each(Callback&& callback, bool sleeping_pool = false)
            {
                static_assert(std::is_invocable_v<Callback, EntityId, Data<Cs&...>>, "For each callback must take EntityId, Entity<Cs&...> as argument.");

                [this, &callback, &sleeping_pool]<typename... As>(Data<Storage<As>&...> storages)
                {
                    auto f = [this, &callback, &sleeping_pool]<typename A>(Storage<A>& s)
                    {
                        Pool<A>& pool = s.pool(sleeping_pool);

                        for (EntityIndex index = 0; index < pool.count(); index++)
                        {
                            callback(pool.ids()[index], pool.template get<Cs...>(index));
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
                    if (run_callbacks) call<EntityUpdated>({id, req_state, res_state});
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
