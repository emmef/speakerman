/*
 * Allocation.hpp
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
#include <stdexcept>
#include <thread>
#include <mutex>
#include <iostream>
#include <memory>

#include <condition_variable>
#include <tdap/Allocation.hpp>
#include <tdap/MemoryFence.hpp>
#include <sys/mman.h>


namespace tdap {

    using namespace std;
    using Mutex = mutex;
    using Lock = unique_lock<Mutex>;
    static const thread::id no_thread;

    class Handle
    {
        enum class State { ENABLED, CLOSED };

        static thread_local Handle *thread_handle_;
        static thread_local int disable_consecutive_allocation_;
        static Handle *linked_;
        static Mutex linked_mutex_;

        struct Block
        {
            char * data_start_;
            char * alloc_start_;
            Block(size_t block_size)
            {
                char * result = static_cast<char *>(malloc(block_size));
                if (result == nullptr) {
                    throw std::bad_alloc();
                }
                size_t alignment = sizeof(max_align_t);
                char *null_ptr = (char *)nullptr;

                if ((result - null_ptr) % alignment == 0) {
                    alloc_start_ = data_start_ = result;
                    return;
                }
                char * new_result = static_cast<char *>(realloc(result, block_size +
                                                                        alignment));
                if (new_result == nullptr) {
                    free(result);
                    throw std::bad_alloc();
                }
                data_start_ = new_result;
                alloc_start_ = null_ptr + alignment * (1 + ((data_start_ - null_ptr) - 1) / alignment);
//                cout << "Allocated aligned block: data=" << (void*)data_start_ << "; alloc_start=" << (void*)alloc_start_ << endl;
            }
        };


        const Block data_;
        char * const alloc_end_;
        char * next_alloc_;
        ConsecutiveAllocationOwner *owner_;
        State state_;
        bool locked_memory_;
        int allocations_;
        Handle *prev_;
        Handle *next_;

        Mutex mutex_;
        thread::id thread_id_;
        condition_variable variable_;

        Lock lock()
        {
            return Lock(mutex_);
        }

        static void * default_alloc(size_t size, bool aligned)
        {
            void *result;
            if (size && aligned) {
                result = aligned_alloc(size, size);
            }
            else {
                result = malloc(size);
            }
            if (result == nullptr) {
                throw std::bad_alloc();
            }
//            cout << "default_alloc(" << size << ", " << aligned << "): " << result << endl;
            return result;
        }

        static size_t roundup(size_t value, size_t alignment)
        {
            return alignment * (1 + (value - 1) / alignment);
        }

        char * get_this_and_next_alloc(size_t size, bool aligned, char *&next_alloc) const
        {
            size_t fundamental_alignment = sizeof(max_align_t);
            size_t rounded_size = roundup(size, fundamental_alignment);
            if (aligned) {
                size_t alloc_start_value = data_.alloc_start_ - (char *) nullptr;
                size_t alloc_value = roundup(next_alloc_ - (char *) nullptr, rounded_size) - alloc_start_value;
                char * result = data_.alloc_start_ + alloc_value;
                next_alloc = result + rounded_size;
            }
            next_alloc = next_alloc_ + rounded_size;
            return next_alloc_;
        }

        void * allocate(size_t size, bool aligned)
        {
            Lock lock(mutex_);
            char *next_alloc;
            char *this_alloc = get_this_and_next_alloc(size, aligned, next_alloc);
            bool shouldAllocate = state_ == State::ENABLED && thread_id_ == this_thread::get_id();
            if (!shouldAllocate) {
                return default_alloc(size, aligned);
            }
            if (next_alloc <= alloc_end_) {
                next_alloc_ = next_alloc;
//                cout << "consecutive_alloc(" << size << ", " << aligned << "): " << (void *)this_alloc << endl;
                allocations_++;
                return this_alloc;
            }
            void *result = default_alloc(size, aligned);
            if (next_alloc_ < alloc_end_) {
                cerr << "consecutive_alloc() created split; next_alloc=" << (next_alloc_ - data_.alloc_start_) << ";alloc_end=" << (alloc_end_ - data_.alloc_start_) << endl;
            }
            next_alloc_ = next_alloc;
            return result;
        }

        void free_ptr(void * data)
        {
            if (belongs_to(data)) {
                Lock lock(mutex_);
                if (state_ == State::CLOSED) {
                    cerr << "ERROR: deleted object was allocated by already closed consecutive_allocation_handle!" << endl;
                }
//                cout << "consecutive_free(" << data << ")" << endl;
                allocations_--;
                return;
            }
//            cout << "default_free(" << data << ")" << endl;
//            cerr << "handle::default_free(" << data << ")" << endl;
            free(data);
        }

        bool belongs_to(const void *data) const
        {
            return data >= data_.alloc_start_ && data < alloc_end_;
        }

        static void link_handle(Handle *me)
        {
            Lock lock(linked_mutex_);
//            cerr << "Handle::link_handle(" << me << ")" << endl;
            if (linked_ == nullptr) {
                linked_ = me;
                me->prev_ = nullptr;
                me->next_ = nullptr;
                return;
            }
            Handle *walk = linked_;
            linked_ = me;
            walk->prev_ = me;
            me->next_ = walk;
            me->prev_ = nullptr;
        }

        static void unlink_handle(Handle *me)
        {
            Lock lock(linked_mutex_);
//            cerr << "Handle::unlink_handle(" << me << ")" << endl;
            if (me->prev_ == nullptr) {
                // first in the chain
                if (me != linked_) {
                    throw std::runtime_error("consecutive_allocation::Handle::Wrong linking");
                }
                linked_ = me->next_;
            }
            if (me->prev_ != nullptr) {
                me->prev_->next_ = me->next_;
            }
            if (me->next_ != nullptr) {
                me->next_->prev_ = me->prev_;
            }
        }

        static Handle * belongs_to_any(void * ptr)
        {
            Lock lock(linked_mutex_);
            Handle *walk = linked_;
            while (walk != nullptr) {
                if (walk->belongs_to(ptr)) {
//                    cerr << walk << ": belongs_to_any(" << ptr << "): " << walk << ")" << endl;
                    return walk;
                }
//                cerr << walk << ": belongs_to_any(" << ptr << ") (next=" << walk->next_ << ")" << endl;
                walk = walk->next_;
            }
            return nullptr;
        }
        
    public:

        static Handle * thread_handle() { return thread_handle_; }


        Handle(size_t block_size) :
            data_(block_size),
            alloc_end_(data_.alloc_start_ + block_size),
            next_alloc_(data_.alloc_start_),
            owner_(nullptr),
            state_(State::ENABLED),
            locked_memory_(false),
            allocations_(0)
        {
            if (data_.alloc_start_ == nullptr) {
                throw std::bad_alloc();
            }
            if ((data_.alloc_start_ - (char *)nullptr) % sizeof(max_align_t) != 0) {
                cerr << "Alignment criterium failed: align=" << sizeof(max_align_t) << "; alloc_start~=" << ((data_.alloc_start_ - (char *)nullptr) % sizeof(max_align_t)) << endl;
                throw runtime_error("Alignment criterium failed");
            }
//            cout << "Created handle " << this << "; data=" << (void *)data_.data_start_ << "; alloc=" << (void *)data_.alloc_start_ << "; end=" << (void *)alloc_end_ << "; size=" << block_size << endl;
            link_handle(this);
        }

        ~Handle()
        {
            unlink_handle(this);
        }

        size_t get_block_size() const
        {
            MemoryFence fence;
            unsigned long result = alloc_end_ - data_.alloc_start_;
//            cout << this << ": block size=" << result << endl;
            return result;
        }

        size_t get_allocated_bytes() const
        {
            MemoryFence fence;
            return next_alloc_ - data_.alloc_start_;
        }

        bool is_consecutive() const
        {
            MemoryFence fence;
            return next_alloc_ <= alloc_end_;
        }

        void unsafe_check_closed_or_throw(const char *message)
        {
            if (state_ != State::CLOSED) {
                return;
            }
            if (message != nullptr) {
                throw runtime_error(message);
            }
            throw runtime_error(
                    "Error in function with (consecutive_block_handle_t *): already closed");
        }

        void set_owner(ConsecutiveAllocationOwner *owner)
        {
            if (owner == nullptr) {
                throw runtime_error("consecutive_alloc::set_owner(consecutive_block_handle_t* handle, ConsecutiveAllocationOwner *owner): owner=nullptr");
            }
            Lock handle_lock(mutex_);
            if (owner_ != nullptr) {
                throw runtime_error("consecutive_alloc::set_owner(consecutive_block_handle_t* handle, ConsecutiveAllocationOwner *owner): handle already owned");
            }
            if (!owner->same_handle(this)) {
                throw runtime_error("consecutive_alloc::set_owner(consecutive_block_handle_t* handle, ConsecutiveAllocationOwner *owner): owner already owns other handle");
            }
            owner_ = owner;
            variable_.notify_all();
        }

        void disown(ConsecutiveAllocationOwner *owner)
        {
            if (owner == nullptr) {
                throw runtime_error("consecutive_alloc::disown(consecutive_block_handle_t* handle, ConsecutiveAllocationOwner *owner): owner=nullptr");
            }
            Lock handle_lock(mutex_);
            unsafe_check_closed_or_throw(
                    "enter(consecutive_block_handle_t * handle): already closed");
            if (owner_ != owner) {
                throw runtime_error("consecutive_alloc::disown(consecutive_block_handle_t* handle, ConsecutiveAllocationOwner *owner): handle not owned by this owner");
            }
            if (!owner->same_handle(this)) {
                throw runtime_error("consecutive_alloc::disown(consecutive_block_handle_t* handle, ConsecutiveAllocationOwner *owner): owner does not own handle");
            }
            owner_ = nullptr;
            variable_.notify_all();
        }

        void enter()
        {
            Lock lock(mutex_);
            unsafe_check_closed_or_throw(
                    "consecutive_alloc::enter(consecutive_block_handle_t * handle): already closed");
            if (thread_id_ != no_thread) {
                throw runtime_error("consecutive_alloc::enter(consecutive_block_handle_t * handle): handle already in use");
            }
            thread_id_ = this_thread::get_id();
            thread_handle_ = this;
        }

        void leave()
        {
            Lock lock(mutex_);
            if (thread_id_ != this_thread::get_id()) {
                throw runtime_error("INTERNAL ERROR: consecutive_alloc::leave(): other handle busy in this thread");
            }
            thread_id_ = no_thread;
            thread_handle_ = nullptr;
        }

        static bool reenable_consecutive_allocation()
        {
            disable_consecutive_allocation_--;
            return disable_consecutive_allocation_ == 0;
        }

        static bool disable_consecutive_allocation()
        {
            disable_consecutive_allocation_++;
            return disable_consecutive_allocation_ == 1;
        }

        static void * allocate_static(size_t size, bool aligned)
        {
            if (disable_consecutive_allocation_ > 0 || thread_handle_ == nullptr) {
                return default_alloc(size, aligned);
            }
            return thread_handle_->allocate(size, aligned);
        }

        static void free_static(void *data)
        {
            Handle *handle = belongs_to_any(data);
            if (handle != nullptr) {
                handle->free_ptr(data);
            }
            else {
                free(data);
            }
        }

        void close()
        {
            Lock lock(mutex_);
            if (state_ == State::CLOSED) {
                return;
            }
            long count = 0;
            while (owner_ != nullptr || thread_id_ != no_thread) {
                count++;
                count %= 100;
                if (count == 1) {
                    cerr << "consecutive_alloc::free(consecutive_block_handle_t * handle): waits until handle unowned" << endl;
                }
                variable_.wait_for(lock, std::chrono::milliseconds(100));
            }
            if (state_ == State::CLOSED) {
                return;
            }
//            cout << "Closing consecutive allocation..." << endl;
            if (locked_memory_) {
                cerr << "consecutive_alloc::free(consecutive_block_handle_t * handle)): Memory was still locked" << endl;
                munlock(data_.alloc_start_, alloc_end_ - data_.alloc_start_);
                locked_memory_ = false;
            }
            free(data_.data_start_);
//            cerr << this << ": closed" << endl;
            state_ = State::CLOSED;
        }

        bool reset(ConsecutiveAllocationOwner *owner)
        {
            Lock lock(mutex_);
            unsafe_check_closed_or_throw("consecutive_alloc::reset(consecutive_block_handle_t* handle, ConsecutiveAllocationOwner *owner): already closed");
            if (owner_ != owner) {
                throw runtime_error("consecutive_alloc::reset(consecutive_block_handle_t* handle, ConsecutiveAllocationOwner *owner): handle not owned by this owner");
            }
            if (!owner->same_handle(this)) {
                throw runtime_error("consecutive_alloc::reset(consecutive_block_handle_t* handle, ConsecutiveAllocationOwner *owner): owner does not own handle");
            }
            long count = 0;
            while (thread_id_ != no_thread) {
                count++;
                count %= 100;
                if (count == 1) {
                    cerr << "consecutive_alloc::reset(consecutive_block_handle_t * handle): waits until unused" << endl;
                }
                variable_.wait_for(lock, std::chrono::milliseconds(100));
            }
            if (allocations_ != 0) {
                return false;
            }
            next_alloc_ = data_.alloc_start_;
            return true;
        }

        bool lock_memory()
        {
            Lock lock(mutex_);
            unsafe_check_closed_or_throw("consecutive_alloc::lock_memory(consecutive_block_handle_t * handle): already closed");
            if (locked_memory_) {
                throw runtime_error("consecutive_alloc::lock_memory(consecutive_block_handle_t * handle): memory already locked");
            }
            unsigned long length = alloc_end_ - data_.alloc_start_;
            if (mlock(data_.alloc_start_, length) == 0) {
                cout << "Locked memory [" << (void *)data_.alloc_start_ << " - " << (void *)alloc_end_ << "] (" << length << " bytes)" << endl;
                locked_memory_ = true;
                return true;
            }
            return false;
        }

        bool unlock_memory()
        {
            Lock lock(mutex_);
            unsafe_check_closed_or_throw("\"consecutive_alloc::unlock_memory(consecutive_block_handle_t * handle): already closed");
            if (!locked_memory_) {
                throw runtime_error("\"consecutive_alloc::unlock_memory(consecutive_block_handle_t * handle): memory not locked");
            }
            unsigned long length = alloc_end_ - data_.alloc_start_;
            if (munlock(data_.alloc_start_, length) == 0) {
                cout << "Unlocked memory [" << (void *)data_.alloc_start_ << " - " << (void *)alloc_end_ << "] (" << length << " bytes)" << endl;
                locked_memory_ = false;
                return true;
            }
            return false;
        }

        class CloseGuard
        {
            Handle * const handle_;
        public:
            CloseGuard(Handle * handle) : handle_(handle) {}
            ~CloseGuard() {
                if (handle_ != nullptr) {
                    delete handle_;
                }
            }
        };

    };

    thread_local Handle *Handle::thread_handle_ = nullptr;
    thread_local int Handle::disable_consecutive_allocation_ = 0;
    Handle *Handle::linked_ = nullptr;
    Mutex Handle::linked_mutex_;

    static Handle *not_null_or_throw_with_message(
            consecutive_block_handle_t *handle, const char * message)
    {
        if (handle != nullptr) {
            return static_cast<Handle *>(handle);
        }
        if (message != nullptr) {
            throw invalid_argument(message);
        }
        throw invalid_argument(
                "Error in function with (consecutive_block_handle_t *): handle=nullptr");
    }

    static const Handle *not_null_or_throw_with_message(
            const consecutive_block_handle_t *handle, const char * message)
    {
        if (handle != nullptr) {
            return static_cast<const Handle *>(handle);
        }
        if (message != nullptr) {
            throw invalid_argument(message);
        }
        throw invalid_argument(
                "Error in function with (consecutive_block_handle_t *): handle=nullptr");
    }

    consecutive_block_handle_t* consecutive_alloc::construct_with_size(size_t block_size)
    {
        Handle *handle = new Handle(block_size);
//        cout << endl << "- new handle=" << handle << "; for size " << block_size << endl;
        return handle;
    }

    size_t consecutive_alloc::get_block_size_for(const consecutive_block_handle_t * handle)
    {
        return not_null_or_throw_with_message(
                handle,
                "get_block_size_for(const consecutive_block_handle_t * handle): handle=nullptr")->get_block_size();
    }

    size_t consecutive_alloc::get_allocated_bytes_for(
            const consecutive_block_handle_t *handle)
    {
        return not_null_or_throw_with_message(
                handle,
                "get_allocated_bytes(const consecutive_block_handle_t * handle): handle=nullptr")->get_allocated_bytes();
    }

    bool consecutive_alloc::is_consecutive_for(const consecutive_block_handle_t * handle)
    {
        return not_null_or_throw_with_message(
                handle,
                "get_allocated_bytes(const consecutive_block_handle_t * handle): handle=nullptr")->is_consecutive();
    }

    ssize_t consecutive_alloc::get_block_size_()
    {
        return Handle::thread_handle() != nullptr ? get_block_size_for(Handle::thread_handle()) : -1;
    }

    ssize_t consecutive_alloc::get_allocated_bytes()
    {
        return Handle::thread_handle() != nullptr ? get_allocated_bytes_for(Handle::thread_handle()) : -1;
    }

    bool consecutive_alloc::is_consecutive()
    {
        return Handle::thread_handle() != nullptr && is_consecutive_for(Handle::thread_handle());
    }

    void consecutive_alloc::free(consecutive_block_handle_t * handle)
    {
        Handle *h = not_null_or_throw_with_message(handle,
                                                   "free(consecutive_block_handle_t * handle): handle=nullptr");
        Handle::CloseGuard guard(h);
        h->close();
    }

    bool consecutive_alloc::reset(consecutive_block_handle_t * handle, ConsecutiveAllocationOwner *owner)
    {
        Handle *h = not_null_or_throw_with_message(handle,
                                                   "free(consecutive_block_handle_t * handle): handle=nullptr");
        return h->reset(owner);
    }

    void consecutive_alloc::enter(consecutive_block_handle_t * handle)
    {
        Handle *h = not_null_or_throw_with_message(handle,
                "enter(consecutive_block_handle_t * handle): handle=nullptr");

        if (Handle::thread_handle() != nullptr) {
            throw runtime_error(
                    "enter(consecutive_block_handle_t * handle): current thread already uses consecutive allocation");
        }

        h->enter();
    }

    void consecutive_alloc::leave()
    {
        if (Handle::thread_handle() == nullptr) {
            throw runtime_error(
                    "leave(consecutive_block_handle_t * handle): current thread not using consecutive allocation");
        }
        Handle::thread_handle()->leave();
    }

    bool consecutive_alloc::disable_consecutive_allocation()
    {
        return Handle::disable_consecutive_allocation();
    }

    bool consecutive_alloc::reenable_consecutive_allocation()
    {
        return Handle::reenable_consecutive_allocation();
    }

    void consecutive_alloc::set_owner(consecutive_block_handle_t* handle, ConsecutiveAllocationOwner *owner)
    {
        Handle *h = not_null_or_throw_with_message(handle,
                                                   "set_owner(consecutive_block_handle_t * handle, ConsecutiveAllocationOwner *owner): handle=nullptr");
        h->set_owner(owner);
    }

    void consecutive_alloc::disown(consecutive_block_handle_t *handle,
                                      ConsecutiveAllocationOwner *owner)
    {
        Handle *h = not_null_or_throw_with_message(handle,
                                                   "disown(consecutive_block_handle_t * handle, ConsecutiveAllocationOwner *owner): handle=nullptr");
        h->disown(owner);
    }

    bool consecutive_alloc::lock_memory(consecutive_block_handle_t *handle)
    {
        return not_null_or_throw_with_message(
                handle,
                "consecutive_alloc::lock_memory(const consecutive_block_handle_t * handle): handle=nullptr")->lock_memory();
    }

    bool consecutive_alloc::unlock_memory(consecutive_block_handle_t *handle)
    {
        return not_null_or_throw_with_message(
                handle,
                "consecutive_alloc::lock_memory(const consecutive_block_handle_t * handle): handle=nullptr")->unlock_memory();
    }

} /* End of namespace speakerman */

void* operator new  ( std::size_t count )
{
    return tdap::Handle::allocate_static(count, false);
}

void* operator new  ( std::size_t count, std::align_val_t )
{
    return tdap::Handle::allocate_static(count, true);
}

void operator delete  ( void* ptr ) noexcept
{
    tdap::Handle::free_static(ptr);
}

void operator delete  ( void* ptr, std::align_val_t ) noexcept
{
    tdap::Handle::free_static(ptr);
}
