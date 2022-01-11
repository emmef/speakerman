#ifndef TDAP_M_POWER2_HPP
#define TDAP_M_POWER2_HPP
/*
 * tdap/Power2.hpp
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

#include <cstddef>

namespace tdap {

namespace helpers_tdap {

template <typename SIZE_T, size_t SIZE_OF_SIZE_T, bool constExpr>
struct FillBitsToRight {};

template <class SIZE_T> struct FillBitsToRight<SIZE_T, 1, false> {
  static inline SIZE_T fill(const SIZE_T x) {
    SIZE_T n = x;
    n = n | (n >> 1);
    n = n | (n >> 2);
    n = n | (n >> 4);
    return n;
  }
};

template <typename SIZE_T> struct FillBitsToRight<SIZE_T, 2, false> {
  static inline SIZE_T fill(const SIZE_T x) {
    SIZE_T n = x;
    n = n | (n >> 1);
    n = n | (n >> 2);
    n = n | (n >> 4);
    n = n | (n >> 8);
    return n;
  }
};

template <typename SIZE_T> struct FillBitsToRight<SIZE_T, 4, false> {
  static inline SIZE_T fill(const SIZE_T x) {
    SIZE_T n = x;
    n = n | (n >> 1);
    n = n | (n >> 2);
    n = n | (n >> 4);
    n = n | (n >> 8);
    n = n | (n >> 16);
    return n;
  }
};

template <typename SIZE_T> struct FillBitsToRight<SIZE_T, 8, false> {
  static inline SIZE_T fill(const SIZE_T x) {
    SIZE_T n = x;
    n = n | (n >> 1);
    n = n | (n >> 2);
    n = n | (n >> 4);
    n = n | (n >> 8);
    n = n | (n >> 16);
    n = n | (n >> 32);
    return n;
  }
};

template <typename SIZE_T> struct FillBitsToRight<SIZE_T, 16, false> {
  static inline SIZE_T fill(const SIZE_T x) {
    SIZE_T n = x;
    n = n | (n >> 1);
    n = n | (n >> 2);
    n = n | (n >> 4);
    n = n | (n >> 8);
    n = n | (n >> 16);
    n = n | (n >> 32);
    n = n | (n >> 64);
    return n;
  }
};

template <typename SIZE_T> class FillBitsToRight<SIZE_T, 0, true> {
  template <size_t N> static constexpr SIZE_T fillN(const SIZE_T n) {
    return N < 2 ? n : fillN<N / 2>(n) | (fillN<N / 2>(n) >> (N / 2));
  };

public:
  static constexpr SIZE_T fill(const SIZE_T n) {
    return fillN<8 * sizeof(SIZE_T)>(n);
  };
};

template <bool constExpr, typename SIZE_T = size_t>
class PowerOfTwoHelper
    : public FillBitsToRight<SIZE_T, constExpr ? 0 : sizeof(SIZE_T),
                             constExpr> {
  using Bits =
      FillBitsToRight<SIZE_T, constExpr ? 0 : sizeof(SIZE_T), constExpr>;

  static constexpr SIZE_T unchecked_aligned(SIZE_T value, SIZE_T alignment) {
    return ((value - 1) | (alignment - 1)) + 1;
  }

public:
  static constexpr bool minus_one(const SIZE_T value) {
    return Bits::fill(value) == value;
  }

  static constexpr bool fill(const SIZE_T value) { return Bits::fill(value); }

  static constexpr bool is(const SIZE_T value) {
    return value >= 2 ? minus_one(value - 1) : false;
  }

  /**
   * Returns value if it is a power of two or else the next power of two that is
   * greater.
   */
  static constexpr SIZE_T next(const SIZE_T value) {
    return Bits::fill(value - 1) + 1;
  }

  /**
   * Returns value if it is a power of two or else the next power of two that is
   * smaller.
   */
  static constexpr SIZE_T previous(const SIZE_T value) {
    return next(value / 2 + 1);
  }

  /**
   * Returns value if it is smaller than the power and else the power of two.
   */
  static constexpr SIZE_T within(const SIZE_T value, const SIZE_T powerOfTwo) {
    return (Bits::fill(value & ((powerOfTwo - 1) ^ -1)) | value) &
           (powerOfTwo - 1);
  }

  /**
   * Returns the value if it is aligned to power_of_two, the first higher
   * value that is aligned to power_of_two or zero if the provided power of two
   * is not actually a power of two.
   *
   * @param value Value to be aligned
   * @param power_of_two The power of two to align to
   * @return the aligned value
   */
  static constexpr SIZE_T aligned_with(const SIZE_T value,
                                       const SIZE_T power_of_two) {
    return value == 0 ? 0 : is(power_of_two) ? unchecked_aligned(value, power_of_two) : 0;
  }

  /**
   * Returns the pointer value if it is aligned to power_of_two, the first
   * higher pointer value that is aligned to power_of_two or NULL if the
   * provided power of two is not actually a power of two.
   *
   * @param pointer Pointer to be aligned
   * @param power_of_two The power of two to align to
   * @return the aligned value
   */
  template <typename T>
  static constexpr T *ptr_aligned_with(T *pointer, const SIZE_T power_of_two) {
    return static_cast<T *>(aligned_with(static_cast<SIZE_T>(pointer), power_of_two));
  }

};

} // namespace helpers_tdap

struct Power2 : public helpers_tdap::PowerOfTwoHelper<false> {
  using constant = helpers_tdap::PowerOfTwoHelper<true>;
};

} // namespace tdap

#endif // TDAP_M_POWER2_HPP
