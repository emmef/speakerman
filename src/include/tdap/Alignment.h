#ifndef TDAP_M_ALIGNMENT_H
#define TDAP_M_ALIGNMENT_H
/*
 * tdap/Alignment.h
 *
 * Added by michel on 2022-02-13
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

#include <tdap/Power2.hpp>
#include <cstddef>

namespace tdap {

#ifndef TDAP_M_ALIGNMENT_DEFAULT_VALUE
static constexpr size_t provideDefaultAlignment() {
  return sizeof(double) * 4;
}
#else
static constexpr size_t provideDefaultAlignment() {
  static_assert(Power2::constant::is(TDAP_M_ALIGNMENT_DEFAULT_VALUE));
  return TDAP_M_ALIGNMENT_DEFAULT_VALUE;
}
#endif

static constexpr size_t AlignmentDefaultBytes = provideDefaultAlignment();

static constexpr bool validAlignmentBytesGeneric(size_t alignment) {
  return Power2::is(alignment);
}

static constexpr bool validAlignmentBytesForConsecutiveArrayOf(size_t alignment, size_t sizeOf) {
  return sizeOf && Power2::constant::is(alignment) && ((alignment % sizeOf) == 0);
}

template<typename T>
static constexpr bool isAlignedWith(const T *ptr, size_t alignment) {
  return alignment && (static_cast<size_t>(ptr) % alignment) == 0;
}

} // namespace tdap

#endif // TDAP_M_ALIGNMENT_H
