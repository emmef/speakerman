#ifndef TDAP_M_SAMPLES_HPP
#define TDAP_M_SAMPLES_HPP
/*
 * tdap/Samples.hpp
 *
 * Part of 'Speaker management system'
 *
 * Copyright (C) 2013-2017 Michel Fleur.
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

#include <tdap/FixedSizeArray.hpp>

namespace tdap {

template <typename T, size_t SIZE, size_t ALIGN = Count<T>::align()>
struct alignas(ALIGN) Samples : public FixedSizeArray<T, SIZE> {
  static_assert(std::is_arithmetic<T>::value && std::is_scalar<T>::value,
                "Samples should be arithmetic scalars");

  template <typename... A>
  Samples(const FixedSizeArrayTraits<T, SIZE, A...> &source) {
    copy(source);
  }

  Samples(const T value) {
    if (value == 0) {
      zero();
    } else {
      for (size_t i = 0; i < SIZE; i++) {
        _traitRefAt(i) = value;
      }
    }
  }

  Samples() {}
};

template <typename T, size_t ROWS, size_t COLUMNS, class S, bool rowsIsSize>
struct HelperVectorTraits {};

template <typename T, size_t ROWS, size_t COLUMNS, class S>
struct HelperVectorTraits<T, ROWS, COLUMNS, S, true> {
  static_assert(ROWS == COLUMNS, "Rows and colums are equal");

  void identity(T scale = 1.0) {
    static_cast<S *>(this)->zero();
    for (size_t i = 0; i < ROWS; i++) {
      static_cast<S *>(this)->operator[](i)[i] = scale;
    }
  }
};

template <typename T, size_t ROWS, size_t COLUMNS, class S>
struct HelperVectorTraits<T, ROWS, COLUMNS, S, false> {
  static_assert(ROWS != COLUMNS, "Rows and colums are not equal");
};

template <typename T, size_t ROWS, size_t COLUMNS,
          size_t ALIGN = Count<T>::align()>
struct SampleMatrix
    : FixedSizeArray<Samples<T, COLUMNS, ALIGN>, ROWS>,
      HelperVectorTraits<T, ROWS, COLUMNS, SampleMatrix, COLUMNS == ROWS> {
  SampleMatrix(const T value) { set_all(value); }
  SampleMatrix() : SampleMatrix(0) {}
  void set_all(T value) {
    for (size_t row = 0; row < ROWS; row++) {
      (*this)[row] = value;
    }
  }
  void operator*=(const T multiplyWith) {
    for (size_t row = 0; row < ROWS; row++) {
      (*this)[row] *= multiplyWith;
    }
  }
  void operator/=(const T divideBy) {
    for (size_t row = 0; row < ROWS; row++) {
      (*this)[row] *= divideBy;
    }
  }
  template <size_t ALGN>
  void operator+=(const SampleMatrix<T, ROWS, COLUMNS, ALGN> &plus) {
    for (size_t row = 0; row < ROWS; row++) {
      (*this)[row] += plus[row];
    }
  }
  template <size_t ALGN>
  void operator-=(const SampleMatrix<T, ROWS, COLUMNS, ALGN> &minus) {
    for (size_t row = 0; row < ROWS; row++) {
      (*this)[row] -= minus[row];
    }
  }
  SampleMatrix operator*(const T multiplyWith) const {
    SampleMatrix result = (*this);
    for (size_t row = 0; row < ROWS; row++) {
      result[row] *= multiplyWith;
    }
    return result;
  }
  SampleMatrix operator/(const T divideBy) const {
    SampleMatrix result = (*this);
    for (size_t row = 0; row < ROWS; row++) {
      result *= divideBy;
    }
    return result;
  }
  template <size_t ALGN>
  SampleMatrix<T, ROWS, COLUMNS, Value<size_t>::max(ALGN, ALIGN)>
  operator+(const SampleMatrix<T, ROWS, COLUMNS, ALGN> &plus) const {
    SampleMatrix result = (*this);
    for (size_t row = 0; row < ROWS; row++) {
      result[row] += plus[row];
    }
    return result;
  }
  template <size_t ALGN>
  SampleMatrix<T, ROWS, COLUMNS, Value<size_t>::max(ALGN, ALIGN)>
  operator-(const SampleMatrix<T, ROWS, COLUMNS, ALGN> &minus) const {
    SampleMatrix result = (*this);
    for (size_t row = 0; row < ROWS; row++) {
      result[row] -= minus[row];
    }
    return result;
  }
  template <size_t AL1, size_t AL2>
  void multiply_in(Samples<T, ROWS, AL1> &output,
                   const Samples<T, COLUMNS, AL2> &input) const {
    for (size_t row = 0; row < ROWS; row++) {
      output[row] = (*this)[row] * input;
    }
  }
  template <size_t N, size_t AL1, size_t AL2>
  void multiply_in(SampleMatrix<T, ROWS, N, AL1> &output,
                   const SampleMatrix<T, COLUMNS, N, AL2> &input) const {
    for (size_t row = 0; row < ROWS; row++) {
      for (size_t column = 0; column < N; column++) {
        T product = 0.0;
        for (size_t x = 0; x < COLUMNS; x++) {
          product += (*this)[row][x] * input[x][column];
        }
        output[row][column] = product;
      }
    }
  }
  template <size_t N, size_t AL1>
  SampleMatrix<T, ROWS, N, Value<size_t>::max(AL1, ALIGN)>
  multiply_in(const SampleMatrix<T, COLUMNS, N, AL1> &input) const {
    SampleMatrix<T, ROWS, N, Value<size_t>::max(AL1, ALIGN)> output;
    for (size_t row = 0; row < ROWS; row++) {
      for (size_t column = 0; column < N; column++) {
        T product = 0.0;
        for (size_t x = 0; x < COLUMNS; x++) {
          product += (*this)[row][x] * input[x][column];
        }
        output[row][column] = product;
      }
    }
    return output;
  }
};

} // namespace tdap

#endif // TDAP_M_SAMPLES_HPP
