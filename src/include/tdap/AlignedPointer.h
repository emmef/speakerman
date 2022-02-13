#ifndef TDAP_M_ALIGNED_POINTER_H
#define TDAP_M_ALIGNED_POINTER_H
/*
 * tdap/AlignedPointer.h
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

#include <memory>
#include <stdexcept>
#include <tdap/Alignment.h>
#include <tdap/IndexPolicy.hpp>

namespace tdap {

/**
 * Provides access to a pointer to type \c Type, that is memory aligned on \c
 * Alignment bytes. This class does not own the pointer in anyway and does
 * not change its value: it is required to be aligned at construction of this
 * class.
 *
 * @tparam Type The type that the pointer points to.
 * @tparam Elements The number of elements the pointer points to.
 * @tparam Alignment The number of bytes to align the pointer position on.
 */
template <typename Type, size_t Elements = 1,
          size_t Alignment = AlignmentDefaultBytes>
class AlignedPointer {
  static_assert(Elements > 0);
  static_assert(validAlignmentBytesForConsecutiveArrayOf(Alignment,
                                                         sizeof(Type)));

  static Type *alignedPointer(Type *pointer) {
    if (isAlignedWith(pointer, Alignment)) {
      return pointer;
    }
    throw std::invalid_argument(
        "AlignedPointer: pointer not aligned accordingly.");
  }

public:
  typedef Type valueType;
  typedef valueType *pointer;
  typedef const valueType *constPointer;
  typedef valueType &reference;
  typedef const valueType &constReference;
  typedef valueType *iterator;
  typedef const valueType *constIterator;
  typedef std::size_t sizeType;
  typedef std::ptrdiff_t differenceType;
  static constexpr size_t alignBytes = Alignment;
  static constexpr size_t alignedElements = Alignment / sizeof(Type);

  AlignedPointer(pointer ptr) : p(alignedPointer(ptr)) {}
  AlignedPointer(const AlignedPointer &source) = default;

  template<size_t E, size_t A>
  requires(E >= Elements && A >= Alignment) //
  AlignedPointer(const AlignedPointer<Type, E, A> &source) : p(source.begin()) {}

  inline reference operator*() { return *std::assume_aligned<Alignment>(p); }
  inline constReference operator*() const {
    return *std::assume_aligned<Alignment>(p);
  }

  inline pointer begin() { return std::assume_aligned<Alignment>(p); }
  inline constPointer cbegin() const {
    return std::assume_aligned<Alignment>(p);
  }

  inline pointer end() { return p + Elements; }
  inline constPointer cend() const { return p + Elements; }

  inline pointer get() { return begin(); }
  inline constPointer get() const { return cbegin(); }

  inline operator pointer() { return begin(); }
  inline operator constPointer() const { return cbegin(); }

  inline reference operator[](size_t i) {
    return p[IndexPolicy::array(i, Elements)];
  }
  inline constReference operator[](size_t i) const {
    return p[IndexPolicy::array(i, Elements)];
  }

  inline pointer operator+(sizeType i) {
    return p + IndexPolicy::array(i, Elements);
  }
  inline constPointer operator+(sizeType i) const {
    return p + IndexPolicy::array(i, Elements);
  }

  inline reference at(size_t i) { return p[IndexPolicy::method(i, Elements)]; }
  inline constReference at(size_t i) const {
    return p[IndexPolicy::method(i, Elements)];
  }

  inline void set(Type *pointer) { p = alignedPointer(pointer); }
  inline AlignedPointer &operator=(const AlignedPointer &source) {
    p = source.p;
    return *this;
  }

  inline differenceType operator-(constPointer ptr) { return ptr - p; }

  template <size_t E, size_t A>
  inline differenceType operator-(const AlignedPointer<Type, E, A> &ptr) {
    return ptr.cbegin() - p;
  }

private:
  pointer p;
};

} // namespace tdap

#endif // TDAP_M_ALIGNED_POINTER_H
