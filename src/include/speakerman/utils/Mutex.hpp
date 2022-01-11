#ifndef SPEAKERMAN_UTILS_M_MUTEX_HPP
#define SPEAKERMAN_UTILS_M_MUTEX_HPP
/*
 * speakerman/utils/Mutex.hpp
 *
 * Part of 'Speaker management system'
 *
 * Copyright (C) 2013 Michel Fleur.
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

#include <mutex>

namespace speakerman::utils {

class Mutex;

class Guard {
  std::recursive_mutex *mutex;

  friend class Mutex;

  Guard(std::recursive_mutex *m) : mutex(m) { m->lock(); };

  Guard &operator&();

public:
  Guard(Guard &&original) : mutex(original.mutex) {
    original.mutex = nullptr;
    // Mutex already locked
  }

  ~Guard() {
    if (mutex) {
      std::recursive_mutex *m = mutex;
      mutex = nullptr;
      m->unlock();
    }
  }
};

class Mutex {
  std::recursive_mutex mutex;

  friend class Guard;

public:
  Guard guard() { return Guard(&mutex); }
};

// Your definitions

} // namespace speakerman::utils

#endif // SPEAKERMAN_UTILS_M_MUTEX_HPP
