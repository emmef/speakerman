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
#include <iostream>
#include <tdap/MemoryFence.hpp>

namespace speakerman {

    using namespace std;
    using namespace tdap;

    static int signal_number = -1;
    static bool user_raised = false;
    static atomic<long signed> guarded_threads(0);


    static void set_signal_internal(int signum, bool is_user_raised)
    {
        signal_number = signum;
        user_raised = is_user_raised;
        MemoryFence::release();
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


    CountedThreadGuard::CountedThreadGuard()
    {
        guarded_threads.fetch_add(1);
    }

    CountedThreadGuard::~CountedThreadGuard()
    {
        guarded_threads.fetch_sub(1);
    }

    bool CountedThreadGuard::await_finished(std::chrono::milliseconds timeout)
    {
        auto count = timeout.count() / 100;
        if (count < 10) {
            count = 10;
        }
        std::chrono::milliseconds sleep_duration(count);
        auto start = std::chrono::steady_clock::now();
        auto now = start;
        while ((now - start) < timeout) {
            if (guarded_threads == 0) {
                return true;
            }
            now = std::chrono::steady_clock::now();
        }
        return false;
    }

    CountedThreadGuard::Await::Await(long millis, const char *wait_message,
          const char *fail_message) : timeout_(millis),
                                      wait_message_(wait_message),
                                      fail_message_(fail_message)
    {}

    CountedThreadGuard::Await::~Await()
    {
        if (wait_message_) {
            std::cout << wait_message_ << endl;
        }
        if (!CountedThreadGuard::await_finished(timeout_) && fail_message_) {
            std::cerr << fail_message_ << endl;
        };
    }

} /* End of namespace speakerman */
