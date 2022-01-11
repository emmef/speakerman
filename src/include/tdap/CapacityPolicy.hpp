#ifndef TDAP_M_CAPACITY_POLICY_HPP
#define TDAP_M_CAPACITY_POLICY_HPP
/*
 * tdap/CapacityPolicy.hpp
 *
 * Part of TdAP
 * Time-domain Audio Processing
 * Copyright (C) 2015 Michel Fleur.
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

#include <tdap/Value.hpp>

namespace tdap {

class CapacityPolicy {
public:
  static size_t defaultNewCapacity(size_t currentCapacity, size_t,
                                   size_t neededSize) {
    size_t capacity = Value<size_t>::max(
        Value<size_t>::max(currentCapacity, 1) * 3 / 2, neededSize, 16);
    if (capacity < neededSize) {
      throw std::invalid_argument("Size overflow in calculating new capacity");
    }
    return capacity;
  }

  virtual size_t calculateNewCapacity(size_t currentCapacity,
                                      size_t currentSize,
                                      size_t neededSize) const {
    return defaultNewCapacity(currentCapacity, currentSize, neededSize);
  }

  virtual ~CapacityPolicy() = default;

  template <typename T>
  void ensureCapacity(T *&data, size_t &capacity, size_t &count,
                      size_t neededCount, size_t maxCapacity) const {
    static_assert(std::is_trivially_copyable<T>::value,
                  "List must contain data that can be copied trivially");

    if (neededCount <= capacity) {
      return;
    }
    if (neededCount >= maxCapacity) {
      throw std::invalid_argument("Needed exceeds maximum capacity");
    }
    size_t newCapacity = calculateNewCapacity(capacity, count, neededCount);
    T *newData = new T[newCapacity];
    if (data) {
      memcpy(newData, data, count * sizeof(T));
      delete[] data;
      data = newData;
    } else {
      data = newData;
    }
    capacity = newCapacity;
  }
};

} // namespace tdap

#endif // TDAP_M_CAPACITY_POLICY_HPP
