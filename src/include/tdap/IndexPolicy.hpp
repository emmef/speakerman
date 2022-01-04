/*
 * tdap/IndexPolicy.hpp
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

#ifndef TDAP_INDEX_POLICY_HEADER_GUARD
#define TDAP_INDEX_POLICY_HEADER_GUARD

#include <stdexcept>

namespace tdap {

#ifndef TDAP_INDEX_POLICY_METHODS_CHECKED
// By default, indexing methods are checked
#define TDAP_METHOD_INDEX_CHECK true
#elif TDAP_INDEX_POLICY_METHODS_CHECKED == 0
#undef TDAP_METHOD_INDEX_CHECK
#else
#define TDAP_METHOD_INDEX_CHECK true
#endif

#ifndef TDAP_METHOD_INDEX_CHECK
#define TDAP_METHOD_INDEX_NOEXCEPT noexcept
#else
#define TDAP_METHOD_INDEX_NOEXCEPT
#endif

#ifndef TDAP_INDEX_POLICY_ARRAY_CHECKED
// By default, arrays are not checked
#undef TDAP_ARRAY_INDEX_CHECK
#elif TDAP_INDEX_POLICY_ARRAY_CHECKED == 0
#undef TDAP_ARRAY_INDEX_CHECK
#else
#define TDAP_ARRAY_INDEX_CHECK true
#endif

#ifndef TDAP_ARRAY_INDEX_CHECK
#define TDAP_ARRAY_INDEX_NOEXCEPT noexcept
#else
#define TDAP_ARRAY_INDEX_NOEXCEPT
#endif

struct IndexPolicy {
  static inline size_t force(size_t index, size_t size) {
    if (index < size) {
      return index;
    }
    throw std::out_of_range("Index out of range");
  }

  static inline size_t array(size_t index, [[maybe_unused]] size_t size) TDAP_ARRAY_INDEX_NOEXCEPT {
#ifdef TDAP_ARRAY_INDEX_CHECK
    return force(index, size);
#else
    return index;
#endif
  }

  static inline size_t method(size_t index, size_t size) TDAP_METHOD_INDEX_NOEXCEPT {
#ifdef TDAP_METHOD_INDEX_CHECK
    return force(index, size);
#else
    return index;
#endif
  }

  struct NotGreater {
    static inline size_t force(size_t index, size_t high_value) {
      if (index <= high_value) {
        return index;
      }
      throw std::out_of_range("Index out of range");
    }

    static inline size_t array(size_t index, [[maybe_unused]] size_t high_value) TDAP_ARRAY_INDEX_NOEXCEPT {
#ifdef TDAP_ARRAY_INDEX_CHECK
      return force(index, high_value);
#else
      return index;
#endif
    }

    static inline size_t method(size_t index, size_t high_value) TDAP_METHOD_INDEX_NOEXCEPT {
#ifdef TDAP_METHOD_INDEX_CHECK
      return force(index, high_value);
#else
      return index;
#endif
    }
  };
};

#if __cplusplus >= 201703L
#define tdap_nodiscard [[nodiscard]]
#if defined(__clang__) || defined(__GNUC__)
#define tdap_force_inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define tdap_force_inline __forceinline
#endif
#else
#define tdap_nodiscard
#define tdap_force_inline inline
#endif

/**
 * The C++20 standard is going to include a template that makes the compiler
 * assume that a certain pointer is aligned to a certain number of bytes. This
 * is described in
 * http://open-std.org/JTC1/SC22/WG21/docs/papers/2018/p1007r2.pdf. This
 * document contains an example to implement this assumption, using using
 * current C++17 compilers.
 */
#if __cplusplus <= 201703L
/*
 * It is *assumed* here that C++20 will use something higher than 201703. If
 * this assumption proves false that will be amended.
 */
template <std::size_t N, typename T>
tdap_nodiscard tdap_force_inline constexpr T *assume_aligned(T *ptr) {
#if defined(__clang__) || (defined(__GNUC__) && !defined(__ICC))
  return reinterpret_cast<T *>(__builtin_assume_aligned(ptr, N));

#elif defined(_MSC_VER)
  if ((reinterpret_cast<std::uintptr_t>(ptr) & ((1 << N) - 1)) == 0)
    return ptr;
  else
    __assume(0);
#elif defined(__ICC)
  if (simpledsp::algorithm::Power2::constant::is(N)) {
    __assume_aligned(ptr, N);
  }
  return ptr;
#else
  // Unknown compiler — do nothing
  return ptr;
#endif
}

#endif //  __cplusplus <= 201703L

} // namespace tdap

#endif /* TDAP_INDEX_POLICY_HEADER_GUARD */
