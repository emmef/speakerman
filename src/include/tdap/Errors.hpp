#ifndef SPEAKERMAN_ERRORS_HPP
#define SPEAKERMAN_ERRORS_HPP
/*
 * speakerman/Errors.hpp
 *
 * Added by michel on 2020-02-18
 * Copyright (C) 2015-2020 Michel Fleur.
 * Source https://github.com/emmef/speakerman
 * Email speakerman@emmef.org
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

#include <cstdint>

namespace tdap {

typedef __uint32_t error_t;

class Error {
  enum class Command { GET, SETNONZERO, SET };

  static error_t command(Command command, error_t setValue) noexcept {
    static thread_local error_t error_ = OK;

    error_t result = error_;
    if (command == Command::SETNONZERO) {
      if (setValue != OK) {
        error_ = setValue;
      }
    } else if (command == Command::SET) {
      error_ = setValue;
    }
    return result;
  }

public:

  [[nodiscard]] static error_t get() noexcept {
    return command(Command::GET, OK);
  }

  [[nodiscard]] static error_t getSet(error_t error) noexcept {
    return command(Command::SET, error);
  }

  [[nodiscard]] static error_t getReset() noexcept {
    return command(Command::SET, OK);
  }

  static void reset() noexcept { command(Command::SET, OK); }

  static void set(error_t error) noexcept { command(Command::SET, error); }

  static void setError(error_t error) noexcept {
    command(Command::SETNONZERO, error);
  }

  [[nodiscard]] static error_t getSetError(error_t error) noexcept {
    return command(Command::SETNONZERO, error);
  }

  static bool setReturn(error_t error) noexcept {
    set(error);
    return error == OK;
  }

  static bool setErrorReturn(error_t error) noexcept {
    setError(error);
    return error == OK;
  }

  static bool setOkReturn() noexcept {
    reset();
    return true;
  }

  template <typename T>
  static T setReturn(error_t error, const T &value) noexcept {
    set(error);
    return value;
  }

  template <typename T>
  static T setErrorReturn(error_t error, const T &value) noexcept {
    setError(error);
    return value;
  }

  template <typename T>
  static T setOkReturn(const T &value) noexcept {
    reset();
    return value;
  }

  static constexpr error_t OK = 0;
  static constexpr error_t STATE = 0x00010000;
  static constexpr error_t BUSY = STATE + 0x1001;
  static constexpr error_t NOTREADY = STATE + 0x1002;
  static constexpr error_t EMPTY = STATE + 0x2001;
  static constexpr error_t FULL = STATE + 0x2002;
  static constexpr error_t ACCESS = STATE + 0x3001;
  static constexpr error_t INV = 0x00020000;
  static constexpr error_t NILL = INV + 0x0001;
  static constexpr error_t ZERO = INV + 0x0002;
  static constexpr error_t BOUND = INV + 0x0003;
  static constexpr error_t NOTFOUND = INV + 0x0004;
};

} // namespace tdap

#endif // SPEAKERMAN_ERRORS_HPP
