#ifndef TDAP_M_COUNT_HPP
#define TDAP_M_COUNT_HPP
/*
 * tdap/Count.hpp
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
#include <limits>
#include <stdexcept>

namespace tdap {

static constexpr size_t max_size_t() {
  return std::numeric_limits<size_t>::max();
}

template <size_t S> struct CountOfSize {
  static_assert(S > 0, "Element size must be greater than zero");

  static constexpr size_t max() { return max_size_t() / S; }

  static constexpr bool valid(size_t cnt) { return cnt <= max(); }

  static constexpr bool valid_positive(size_t cnt) {
    return cnt > 0 && valid(cnt);
  }

  /**
   * Returns the product of the counts if that product is less than or equal
   * to max() and zero otherwise.
   */
  static constexpr size_t product(size_t cnt1, size_t cnt2) {
    return cnt1 * cnt1 > 0 && max() / cnt1 >= cnt2 ? cnt1 * cnt2 : 0;
  }

  /**
   * Returns the product of the counts if that product is less than or equal
   * to max() and zero otherwise.
   */
  static constexpr size_t product(size_t cnt1, size_t cnt2, size_t cnt3) {
    return product(cnt1, product(cnt2, cnt3));
  }

  /**
   * Returns the product of the counts if that product is less than or equal
   * to max() and zero otherwise.
   */
  static constexpr size_t product(size_t cnt1, size_t cnt2, size_t cnt3,
                                  size_t cnt4) {
    return product(cnt1, product(cnt2, cnt3, cnt4));
  }

  /**
   * Returns the sum of the counts if that sum is less than or equal
   * to max() and zero otherwise.
   */
  static constexpr size_t sum(size_t cnt1, size_t cnt2) {
    return cnt1 <= max() && cnt2 <= max() && (max() - cnt1) >= cnt2
               ? cnt1 + cnt2
               : 0;
  }

  /**
   * Returns the sum of the counts if that sum is less than or equal
   * to max() and zero otherwise.
   */
  static constexpr size_t sum(size_t cnt1, size_t cnt2, size_t cnt3) {
    return sum(cnt1, sum(cnt2, cnt3));
  }

  /**
   * Returns the sum of the counts if that sum is less than or equal
   * to max() and zero otherwise.
   */
  static constexpr size_t sum(size_t cnt1, size_t cnt2, size_t cnt3,
                              size_t cnt4) {
    return sum(cnt1, sum(cnt2, cnt3, cnt4));
  }

  /**
   * Returns whether the sum of the counts is less than or equal to max()
   */
  static constexpr size_t is_valid_sum(size_t cnt1, size_t cnt2) {
    return cnt1 <= max() && cnt2 <= max() && cnt1 + cnt2 <= max();
  }

  /**
   * Returns the sum of the counts if that sum is less than or equal
   * to max() and zero otherwise.
   */
  static constexpr size_t is_valid_sum(size_t cnt1, size_t cnt2, size_t cnt3) {
    return is_valid_sum(cnt1, cnt2) && is_valid_sum(cnt1 + cnt2, cnt3);
  }

  /**
   * Returns the sum of the counts if that sum is less than or equal
   * to max() and zero otherwise.
   */
  static constexpr size_t is_valid_sum(size_t cnt1, size_t cnt2, size_t cnt3,
                                       size_t cnt4) {
    return is_valid_sum(cnt1, cnt2) && is_valid_sum(cnt3, cnt4) &&
           is_valid_sum(cnt1 + cnt2, cnt3 + cnt4);
  }

  static constexpr size_t aligned_with(size_t value, size_t alignment) {
    return alignment != 0 ? value % alignment != 0
                                ? value + alignment - (value % alignment)
                                : value
                          : 0;
  }
};

template <typename E> class Count : public CountOfSize<sizeof(E)> {
public:
  static constexpr size_t align() {
    return std::is_floating_point<E>::value ? alignof(max_align_t) : alignof(E);
  }
};

static constexpr int constexpr_string_length(const char *str) {
  return *str ? 1 + constexpr_string_length(str + 1) + 1 : 0;
}

} // namespace tdap

#endif // TDAP_M_COUNT_HPP
