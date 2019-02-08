/*
 * tdap/Allocation.hpp
 *
 * Part of TdAP
 * Time-domain Audio Processing
 * Copyright (C) 2015-2016 Michel Fleur.
 * Source https://bitbucket.org/emmef/tdap
 * Email  tdap@emmef.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TDAP_ALLOCATION_HEADER_GUARD
#define TDAP_ALLOCATION_HEADER_GUARD

#include <cstdlib>
#include <mutex>
#include <type_traits>

namespace tdap {

    typedef void consecutive_block_handle_t;

    class ConsecutiveAllocationOwner;


    /**
     * Manages consecutive allocation of memory for new operators
     */
    struct consecutive_alloc
    {
        /**
         * Creates a handle to the consecutive allocation structure, that will
         * allocate memory consecutively for a maximum of block_size bytes.
         *
         * The handle should be used by queries, free and by the guard types
         * within ConsecutiveAllocationEnabledGuard.
         *
         * @param block_size maximum size of consecutive allocation, rounded
         *        up to nearest page
         * @return a handle to pass to various
         * @see ConsecutiveAllocationGuard::Enable
         * @see ConsecutiveAllocationGuard::Disable
         * @see ConsecutiveAllocationGuard::CheckedDisable
         * @see #free()
         */
        static consecutive_block_handle_t* construct_with_size(size_t block_size);


        /**
         * Free the consecutive block indicated by handle.
         *
         * IMPORTANT:
         * This MUST only be called when all corresponding delete operators
         * of consecutively allocated new operators have been called. Otherwise,
         * behavior is undefined. It is safer to use an
         * ConsecutiveAllocationOwner or ConsecutiveClassAllocator where you
         * manage the lifecycle via constructing and destroying that object.
         *
         * @param handle handle to free.
         */
        static void free(consecutive_block_handle_t * handle);

        static size_t get_block_size_for(const consecutive_block_handle_t * handle);
        static size_t get_allocated_bytes_for(
                const consecutive_block_handle_t *handle);
        static bool is_consecutive_for(const consecutive_block_handle_t * handle);

        static ssize_t get_block_size_();
        static ssize_t get_allocated_bytes();
        static bool is_consecutive();

        friend class ConsecutiveAllocationGuard;
        friend class ConsecutiveAllocationOwner;

        struct Guard
        {
        public:
            virtual ~Guard() {};
        };

        class Enable {
            bool execute_;
        public:
            Enable(consecutive_block_handle_t * handle) : execute_(true)
            {
                consecutive_alloc::enter(handle);
            }
            Enable(Enable &&source) : execute_(source.execute_)
            {
                source.execute_ = false;
            }
            ~Enable()
            {
                if (execute_) {
                    consecutive_alloc::leave();
                }
            }
        private:
            operator Enable &();
            operator const Enable &();
            Enable &operator*();
            Enable &operator->();
        };

        class Disable
        {
            bool execute_;
            operator Disable &();
            operator const Disable &();
            Disable &operator*();
            Disable &operator->();
        public:
            Disable() : execute_(consecutive_alloc::disable_consecutive_allocation())
            {
            }
            Disable(Disable &&source) : execute_(source.execute_)
            {
                source.execute_ = false;
            }
            ~Disable()
            {
                if (execute_) {
                    consecutive_alloc::reenable_consecutive_allocation();
                }
            }
        };

    private:
        static void enter(consecutive_block_handle_t * handle);
        static void leave();
        static bool disable_consecutive_allocation();
        static bool reenable_consecutive_allocation();
        static void set_owner(consecutive_block_handle_t* handle,
                ConsecutiveAllocationOwner *owner);
        static void disown(consecutive_block_handle_t *handle,
                                  ConsecutiveAllocationOwner *owner);
        static bool lock_memory(consecutive_block_handle_t * handle);
        static bool unlock_memory(consecutive_block_handle_t * handle);
        static bool reset(consecutive_block_handle_t * handle, ConsecutiveAllocationOwner *owner);
    };

    class ConsecutiveAllocationOwner
    {
        consecutive_block_handle_t * const handle_;

    protected:
        class FreeGuard
        {
            consecutive_block_handle_t * const handle_;
        public:
            FreeGuard(consecutive_block_handle_t * const handle)
            : handle_(handle) {}

            ~FreeGuard() {
                consecutive_alloc::free(handle_);
            }
        };

        bool reset_allocation()
        {
            return consecutive_alloc::reset(handle_, this);
        }
    public:
        
        ConsecutiveAllocationOwner(size_t block_size) : handle_(consecutive_alloc::construct_with_size(block_size))
        {
            consecutive_alloc::set_owner(handle_, this);
        }

        ~ConsecutiveAllocationOwner()
        {
            if (handle_ != nullptr)
            {
                FreeGuard free(handle_);
                consecutive_alloc::disown(handle_, this);
            }
        }

        consecutive_alloc::Enable enable()
        {
            return consecutive_alloc::Enable(handle_);
        }

        bool same_handle(consecutive_block_handle_t * handle)
        {
            return handle == handle_;
        }

        bool lock_memory()
        {
            return consecutive_alloc::lock_memory(handle_);
        }

        bool unlock_memory()
        {
            return consecutive_alloc::unlock_memory(handle_);
        }

        size_t get_block_size()
        {
            return consecutive_alloc::get_block_size_for(handle_);
        };

        size_t get_allocated_bytes()
        {
            return consecutive_alloc::get_allocated_bytes_for(handle_);
        }

        bool is_consecutive()
        {
            return consecutive_alloc::is_consecutive_for(handle_);
        }


    };

    template<class Object, bool trivially_constructible>
    struct TrivialConstructibleCreator;

    template<class Object>
    struct TrivialConstructibleCreator<Object, true>
    {
        static Object * create() { return new Object; }
    };
    template<class Object>
    struct TrivialConstructibleCreator<Object, false>
    {
        static Object * create() { return nullptr; }
    };

    template <class Object, bool enable_trivial_construction = false>
    class ConsecutiveAllocatedObjectOwner : public ConsecutiveAllocationOwner
    {
        Object * object_;
        std::mutex mutex_;

        consecutive_alloc::Enable discard_old_reset_enable()
        {
            Object *old_object = object_;
            if (old_object != nullptr) {
                object_ = nullptr;
                delete old_object;
            }
            reset_allocation();
            consecutive_alloc::Enable guard = enable();
            return guard;
        }


        template <class Sub>
        static Sub *create_trivial_instance()
        {
            static_assert(std::is_base_of<Object,Sub>::value || std::is_same<Object,Sub>::value, "");
            return TrivialConstructibleCreator<Sub, std::is_trivially_constructible<Sub>::value && enable_trivial_construction>::create();
        }

        template <class Sub>
        Sub *assign_and_return(Sub *return_value)
        {
            static_assert(std::is_base_of<Object,Sub>::value || std::is_same<Object,Sub>::value, "");
            object_ = return_value;
            return return_value;
        }

    public:
        Object &get()
        {
            std::unique_lock<std::mutex> lock(mutex_);
            return *object_;
        }
        const Object &get() const
        {
            std::unique_lock<std::mutex> lock(mutex_);
            return *object_;
        }
        operator Object &() const
        {
            return *get();
        }
        operator const Object &() const
        {
            return *get();
        }

        ConsecutiveAllocatedObjectOwner(size_t block_size) : ConsecutiveAllocationOwner(block_size) {
            consecutive_alloc::Enable guard = enable();
            object_ = create_trivial_instance<Object>();
        }
        template <class... A>
        ConsecutiveAllocatedObjectOwner(size_t block_size, A ...parameters) {
            consecutive_alloc::Enable guard = enable();
            object_ = new Object(parameters...);
        }

        template <class Sub, class... A>
        Sub *replace(A ...parameters) {
            static_assert(std::is_base_of<Object,Sub>::value || std::is_same<Object,Sub>::value, "");

            std::unique_lock<std::mutex> lock(mutex_);
            consecutive_alloc::Enable guard = discard_old_reset_enable();
            return assign_and_return(new Sub(parameters...));
        }

        template <class Sub, class... A>
        Sub * generate(Sub *(*function)(A ...p), A ...parameters) {
            static_assert(std::is_base_of<Object,Sub>::value || std::is_same<Object,Sub>::value, "");
            std::unique_lock<std::mutex> lock(mutex_);
            consecutive_alloc::Enable guard = discard_old_reset_enable();
            return assign_and_return(function(parameters...));
        }

        template<class Sub>
        Sub * replace() {
            static_assert(std::is_base_of<Object,Sub>::value || std::is_same<Object,Sub>::value, "");
            std::unique_lock<std::mutex> lock(mutex_);
            consecutive_alloc::Enable guard = discard_old_reset_enable();
            return assign_and_return(create_trivial_instance<Sub>());
        }

        template<class Sub>
        Sub * generate(Sub *(*function)) {
            static_assert(std::is_base_of<Object,Sub>::value || std::is_same<Object,Sub>::value, "");
            std::unique_lock<std::mutex> lock(mutex_);
            consecutive_alloc::Enable guard = discard_old_reset_enable();
            return assign_and_return(function());
        }

        void set_null()
        {
            generate(TrivialConstructibleCreator<Object,false>::create);
        }

        ~ConsecutiveAllocatedObjectOwner()
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (object_ != nullptr) {
                delete object_;
                object_ = nullptr;
            }
        }
    };

    
    
    
} /* End of name space tdap */

#endif /* TDAP_ALLOCATION_HEADER_GUARD */
