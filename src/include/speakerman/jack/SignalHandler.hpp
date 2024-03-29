#ifndef SPEAKERMAN_JACK_M_SIGNAL_HANDLER_HPP
#define SPEAKERMAN_JACK_M_SIGNAL_HANDLER_HPP
/*
 * speakerman/jack/SignalHandler.hpp
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

#include <chrono>

namespace speakerman::jack {

class SignalHandler {
  int int_get_signal() const;

  bool int_is_set() const;

  int int_raise_signal(int signal) const;

  bool int_check_raised() const;

  SignalHandler();

public:
  static const SignalHandler &instance();

  static int get_signal();

  static bool is_set();

  static int raise_signal(int signal);

  static bool check_raised();
};

/**
 * This is NOT an exception and must be caught separately
 */
class signal_exception {
  friend class SignalHandler;

  static constexpr int LENGTH = 32;
  int signal_;
  char message_[LENGTH];

  signal_exception(int signal, bool user_raised);

public:
  const char *what() const;

  int signal() const { return signal_; }

  void handle() const;

  void handle(const char *description) const;
};

template <typename T>
void signal_aware_thread_method_data(void(thread_method)(T &data), T &data,
                                     const char *thread_description) {
  if (thread_method) {
    try {
      thread_method(data);
    } catch (const signal_exception &e) {
      e.handle(thread_description);
    }
  }
}

class CountedThreadGuard {
  size_t thread_id;

public:
  CountedThreadGuard(const char *thread_name);

  ~CountedThreadGuard();

  static bool await_finished(std::chrono::milliseconds timeout,
                             const char *wait_message);
};

class AwaitThreadFinishedAfterExit {
  std::chrono::milliseconds timeout_;
  const char *wait_message_;

public:
  AwaitThreadFinishedAfterExit(long timeout_millis, const char *wait_message)
      : timeout_(timeout_millis), wait_message_(wait_message) {}

  ~AwaitThreadFinishedAfterExit() {
    CountedThreadGuard::await_finished(timeout_, wait_message_);
  }
};

} // namespace speakerman::jack

#endif // SPEAKERMAN_JACK_M_SIGNAL_HANDLER_HPP
