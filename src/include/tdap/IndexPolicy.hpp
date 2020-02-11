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

  static inline size_t array(size_t index, size_t size) TDAP_ARRAY_INDEX_NOEXCEPT {
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

    static inline size_t array(size_t index, size_t high_value) TDAP_ARRAY_INDEX_NOEXCEPT {
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

} // namespace tdap

#endif /* TDAP_INDEX_POLICY_HEADER_GUARD */
