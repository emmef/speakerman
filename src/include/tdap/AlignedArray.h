#ifndef TDAP_M_ALIGNED_ARRAY_H
#define TDAP_M_ALIGNED_ARRAY_H
/*
 * tdap/AlignedArray.h
 *
 * Added by michel on 2022-01-11
 * Copyright (C) 2015-2022 Michel Fleur.
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

#include <tdap/Alignment.h>
#include <tdap/Power2.hpp>
#include <algorithm>
#include <array>

namespace tdap {


template <typename T, size_t S, size_t A = AlignmentDefaultBytes>
struct alignas(A) AlignedArray : public std::array<T, S> {
  static_assert(validAlignmentBytesForConsecutiveArrayOf<T>(A));
  static constexpr size_t alignBytes = A;
  static constexpr size_t alignedElements = alignBytes / sizeof(T);

  AlignedArray() {
    std::fill(this->begin(), this->end(), 0);
  }

  AlignedArray(const AlignedArray &value) {
    std::copy(value.begin(), value.end(), this->begin());
  }

  AlignedArray(AlignedArray &&value) noexcept {
    std::copy(value.begin(), value.end(), this->begin());
  }

  explicit AlignedArray(const std::array<T, S> &value) {
    std::copy(value.begin(), value.end(), this->begin());
  }

  AlignedArray &operator = (const AlignedArray &source) {
    std::copy(source.begin(), source.end(), this->begin());
    return *this;
  }

  AlignedArray(std::initializer_list<T> elements)  {
    ssize_t count = elements.size();
    if (count == 0) {
      std::fill(this->begin(), this->end(), 0);
    }
    else if (count == 1) {
      std::fill(this->begin(), this->end(), *elements.begin());
    }
    else {
      T value;
      size_t i = 0;
      for (auto p = elements.begin(); i < S && p < elements.end(); i++, p++) {
        value = *p;
        this->operator[](i) = value;
      }
      for (; i < S; i++) {
        this->operator[](i) = value;
      }
    }
  }
};


} // namespace tdap

#endif // TDAP_M_ALIGNED_ARRAY_H
