/*
 * SignalHandler.cpp
 *
 * Part of 'Speaker management system'
 *
 * Copyright (C) 2013-2014 Michel Fleur.
 * https://github.com/emmef/simpledsp
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

#include <speakerman/jack/SignalHandler.hpp>
#include <atomic>
#include <csignal>
#include <cstdio>
#include <thread>
#include <iostream>
#include <cstring>
#include <condition_variable>
#include <tdap/MemoryFence.hpp>
#include <tdap/Guards.hpp>

namespace speakerman {

    using namespace std;
    using namespace tdap;

    static int signal_number = -1;
    static bool user_raised = false;

    static atomic<long signed> guarded_threads(0);
    static atomic<unsigned> thread_numbers(1);

    static size_t next_thread_number()
    {
        size_t result = thread_numbers.fetch_add(1);
        while (result == 0) {
            thread_numbers.fetch_add(1);
        }
    }

    class thread_entry
    {
        static constexpr size_t NAME_SIZE = 127;

        char name_[NAME_SIZE + 1];
        size_t id_;

    public:
        thread_entry() : id_(0)
        {
            name_[0] = 0;
        }

        void use_entry(const char *thread_name, size_t id)
        {
            id_ = id;
            if (id == 0) {
                thread_numbers.fetch_add(1);
            }
            if (thread_name != nullptr && thread_name[0] != 0) {
                std::strncpy(name_, thread_name, NAME_SIZE);
                name_[NAME_SIZE] = 0;
            }
            else {
                snprintf(name_, NAME_SIZE + 1, "Thread[%zu]", id);
            }
        }

        void deactivate_entry()
        {
            id_ = 0;
            name_[0] = 0;
        }

        bool is_acive() const
        {
            return id_ > 0;
        }

        size_t id() const { return id_; }

        void write_to_stream(std::ostream &stream, const char *message)
        {
            stream << message << " thread[" << id_ << "]: " << name_ << endl;
        }

        const char * name() const { return name_; }
    };

    template<class Mutex>
    class FakeLock
    {
        MemoryFence fence;
    public:
        FakeLock(Mutex &mutex) {}
        ~FakeLock() {}
    };

    class thread_entries
    {
        static constexpr size_t MAX_THREAD_ENTRIES = 128;

        size_t active_thread_count = 0;
        size_t thread_numbers = 0;

        thread_entry entry_[MAX_THREAD_ENTRIES];
        using Mutex = std::mutex;
        using Lock = unique_lock<Mutex>;
        Mutex mutex_;

        size_t next_thread_number()
        {
            size_t number = ++thread_numbers;
            while (number == 0 || is_active_thread_number(number)) {
                number = ++thread_numbers;
            }
            return number;
        }

        bool is_active_thread_number(size_t number) const
        {
            if (number == 0) {
                return true;
            }
            size_t active_count = 0;
            for (size_t i = 0; i < MAX_THREAD_ENTRIES && active_count < active_thread_count; i++) {
                size_t id = entry_[i].id();
                if (id == number) {
                    return true;
                }
                else if (id != 0) {
                    active_count++;
                }
            }

            return false;
        }
    public:

        size_t add_thread(const char *name)
        {
            Lock _(mutex_);
            if (active_thread_count < MAX_THREAD_ENTRIES) {
                size_t id = next_thread_number();
                for (size_t slot_number = 0; slot_number < MAX_THREAD_ENTRIES; slot_number++) {
                    if (!entry_[slot_number].is_acive()) {
                        entry_[slot_number].use_entry(name, id);
                        active_thread_count++;
                        entry_[slot_number].write_to_stream(cout, "Start");
                        return id;
                    }
                }
            }
            else {
                throw std::runtime_error("Too many managed threads");
            }
        }

        bool remove_thread(size_t id) {
            Lock _(mutex_);

            for (size_t slot_number = 0; slot_number < MAX_THREAD_ENTRIES; slot_number++) {
                if (entry_[slot_number].id() == id) {
                    entry_[slot_number].write_to_stream(cout, "Exit");
                    entry_[slot_number].deactivate_entry();
                    active_thread_count--;
                    return true;
                }
            }
            return false;
        }

        bool await(std::chrono::milliseconds timeout)
        {
            auto count = timeout.count() / 100;
            if (count < 10) {
                count = 10;
            }
            std::chrono::milliseconds sleep_duration(count);
            auto start = std::chrono::steady_clock::now();
            auto now = start;
            while ((now - start) < timeout) {
                {
                    Lock _(mutex_);

                    if (active_thread_count == 0) {
                        return true;
                    }
                }
                this_thread::sleep_for(sleep_duration);
                now = std::chrono::steady_clock::now();
            }
            return false;
        }

        bool await_and_report(std::chrono::milliseconds timeout, const char * start_wait_message)
        {
            if (start_wait_message) {
                cout << start_wait_message << endl;
            }
            if (await(timeout)) {
                return true;
            }
            {
                Lock _(mutex_);
                cerr << "Timeout: following threads still active:" << endl;
                size_t active_count = 0;
                for (size_t i = 0; i < MAX_THREAD_ENTRIES &&
                                   active_count < active_thread_count; i++) {
                    if (entry_[i].is_acive()) {
                        entry_[i].write_to_stream(cerr, "Busy");
                    }
                }
            }
        }

    };

    static thread_entries THREAD_ENTITIES;


    static void set_signal_internal(int signum, bool is_user_raised)
    {
        signal_number = signum;
        user_raised = is_user_raised;
        MemoryFence::release();
        thread::id x = this_thread::get_id();
    }

    extern "C" {
    static void signal_callback_handler(int signum)
    {
        set_signal_internal(signum, false);
    }
    }

    static bool ensure_handlers()
    {
        static atomic_flag flag;

        if (!flag.test_and_set()) {
            signal(SIGINT, signal_callback_handler);
            signal(SIGTERM, signal_callback_handler);
            signal(SIGABRT, signal_callback_handler);
        }
        return true;
    }

    signal_exception::signal_exception(int signal, bool user_raised) :
            signal_(signal)
    {
        if (user_raised) {
            snprintf(message_, LENGTH, "User raised signal %i", signal_ & 0xffff);
        }
        else {
            snprintf(message_, LENGTH, "Caught signal %i", signal_ & 0xffff);
        }
    }

    const char *signal_exception::what() const
    {
        return message_;
    }

    void signal_exception::handle() const
    {
        handle(nullptr);
    }

    void signal_exception::handle(const char *description) const
    {
        if (description) {
            fprintf(stderr, "Thread interrupted (\"%s\"): %s\n", description, what());
        }
        else {
            fprintf(stderr, "Thread interrupted: %s\n", what());
        }
        fflush(stderr);
    }

    SignalHandler::SignalHandler()
    {
        ensure_handlers();
    }

    const SignalHandler &SignalHandler::instance()
    {
        static SignalHandler instance;
        return instance;
    }

    int SignalHandler::int_get_signal() const
    {
        MemoryFence::acquire();
        return signal_number;
    }

    bool SignalHandler::int_is_set() const
    {
        return int_get_signal() != -1;
    }

    int SignalHandler::int_raise_signal(int signal) const
    {
        int previous = int_get_signal();
        if (signal > 0) {
            set_signal_internal(signal, true);
        }
        return previous;
    }

    bool SignalHandler::int_check_raised() const
    {
        bool result = int_is_set();
        if (result) {
            throw signal_exception(signal_number, user_raised);
        }
        return result;
    }

    int SignalHandler::get_signal()
    {
        return instance().int_get_signal();
    }

    bool SignalHandler::is_set()
    {
        return instance().int_is_set();
    }

    int SignalHandler::raise_signal(int signal)
    {
        return instance().int_raise_signal(signal);
    }

    bool SignalHandler::check_raised()
    {
        return instance().int_check_raised();
    }


    CountedThreadGuard::CountedThreadGuard(const char *thread_name)
    {
        thread_id = THREAD_ENTITIES.add_thread(thread_name);
    }

    CountedThreadGuard::~CountedThreadGuard()
    {
        THREAD_ENTITIES.remove_thread(thread_id);
    }

    bool CountedThreadGuard::await_finished(std::chrono::milliseconds timeout, const char *wait_message)
    {
        THREAD_ENTITIES.await_and_report(timeout, wait_message);
    }


} /* End of namespace speakerman */
