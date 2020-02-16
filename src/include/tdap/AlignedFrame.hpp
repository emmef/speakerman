#ifndef SPEAKERMAN_ALIGNEDFRAME_HPP
#define SPEAKERMAN_ALIGNEDFRAME_HPP
/*
 * speakerman/AlignedFrame.hpp
 *
 * Added by michel on 2020-02-13
 * Copyright (C) 2015-2020 Michel Fleur.
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

#include <tdap/IndexPolicy.hpp>
#include <tdap/Power2.hpp>

namespace tdap {

template <typename T, size_t ALIGNMENT> struct Alignment {
  static constexpr size_t elements = Power2::constant::next(ALIGNMENT);
  static constexpr int bytes = Power2::constant::next(sizeof(T) * elements);
};

template <typename T, size_t CHNLS, size_t ALIGN_SAMPLES = 4>
struct AlignedFrame {
  static_assert(CHNLS > 1 && CHNLS < 1024, "Channels not between 1 and 1024");
  static_assert(Power2::constant::is(ALIGN_SAMPLES), "ALIGN_SAMPLES os not a power of 2.");
  static constexpr size_t ALIGN_BYTES = ALIGN_SAMPLES * sizeof(T);
  static constexpr size_t FRAMESIZE =
      Power2::constant::aligned_with(CHNLS, ALIGN_SAMPLES);
  static constexpr size_t CHANNELS = CHNLS;

  alignas(ALIGN_BYTES) T data[FRAMESIZE];

  tdap_nodiscard tdap_force_inline T &
  operator[](size_t i) TDAP_ARRAY_INDEX_NOEXCEPT {
    return data[IndexPolicy::array(i, CHNLS)];
  }

  tdap_nodiscard tdap_force_inline const T &
  operator[](size_t i) const TDAP_ARRAY_INDEX_NOEXCEPT {
    return data[IndexPolicy::array(i, CHNLS)];
  }

  AlignedFrame() noexcept = default;

  AlignedFrame(AlignedFrame &&value) noexcept {
    for (size_t i = 0; i < CHNLS; i++) {
      data[i] = value.data[i];
    }
  }

  AlignedFrame(const AlignedFrame &&value) noexcept {
    for (size_t i = 0; i < CHNLS; i++) {
      data[i] = value.data[i];
    }
  }

  AlignedFrame(const AlignedFrame &value) noexcept {
    for (size_t i = 0; i < CHNLS; i++) {
      data[i] = value.data[i];
    }
  }

  explicit AlignedFrame(T value) noexcept {
    for (size_t i = 0; i < CHNLS; i++) {
      data[i] = value;
    }
  }

  AlignedFrame &operator=(const AlignedFrame &value) noexcept {
    if (this == &value) {
      return *this;
    }
    for (size_t i = 0; i < CHNLS; i++) {
      data[i] = value.data[i];
    }
    return *this;
  }

  AlignedFrame &operator=(T value) noexcept {
    for (size_t i = 0; i < CHNLS; i++) {
      data[i] = value;
    }
    return *this;
  }

  AlignedFrame zero() noexcept {
    for (size_t i = 0; i < CHNLS; i++) {
      data[i] = 0;
    }
    return *this;
  }

  AlignedFrame &operator+=(T value) noexcept {
    for (size_t i = 0; i < CHNLS; i++) {
      data[i] += value;
    }
    return *this;
  }

  AlignedFrame &operator-=(T value) noexcept {
    for (size_t i = 0; i < CHNLS; i++) {
      data[i] -= value;
    }
    return *this;
  }

  AlignedFrame &operator*=(T value) noexcept {
    for (size_t i = 0; i < CHNLS; i++) {
      data[i] *= value;
    }
    return *this;
  }

  AlignedFrame &operator/=(T value) noexcept {
    for (size_t i = 0; i < CHNLS; i++) {
      data[i] /= value;
    }
    return *this;
  }

  AlignedFrame &operator+=(const AlignedFrame &value) noexcept {
    for (size_t i = 0; i < CHNLS; i++) {
      data[i] += value.data[i];
    }
    return *this;
  }

  AlignedFrame &operator-=(const AlignedFrame &value) noexcept {
    for (size_t i = 0; i < CHNLS; i++) {
      data[i] -= value.data[i];
    }
    return *this;
  }


  [[nodiscard]] T dot() const noexcept {
    T y = 0;
    for (size_t i = 0; i < CHNLS; i++) {
      y += data[i] * data[i];
    }
    return y;
  }

  AlignedFrame operator+(T value) const noexcept {
    AlignedFrame result = *this;
    result += value;
    return result;
  }

  AlignedFrame operator-(T value) const noexcept {
    AlignedFrame result = *this;
    result -= value;
    return result;
  }

  AlignedFrame operator*(T value) const noexcept {
    AlignedFrame result = *this;
    result *= value;
    return result;
  }

  AlignedFrame operator/(T value) const noexcept {
    AlignedFrame result = *this;
    result /= value;
    return result;
  }
};

// Scalar operations
template <typename T, size_t C, size_t A>
AlignedFrame<T, C, A> operator*(T value,
                                const AlignedFrame<T, C, A> &f) noexcept {
  AlignedFrame<T, C, A> result = f;
  result *= value;
  return result;
}

template <typename T, size_t C, size_t A>
AlignedFrame<T, C, A> operator+(T value,
                                const AlignedFrame<T, C, A> &f) noexcept {
  AlignedFrame<T, C, A> result = f;
  result += value;
  return result;
}

//
template <typename T, size_t C, size_t A>
AlignedFrame<T, C, A> operator+(const AlignedFrame<T, C, A> &f1,
                                AlignedFrame<T, C, A> &&f2) noexcept {
  f2 += f1;
  return f2;
}

template <typename T, size_t C, size_t A>
AlignedFrame<T, C, A> operator+(AlignedFrame<T, C, A> &&f1,
                                const AlignedFrame<T, C, A> &f2) noexcept {
  f1 += f2;
  return f1;
}

template <typename T, size_t C, size_t A>
AlignedFrame<T, C, A> operator+(AlignedFrame<T, C, A> &&f1,
                                AlignedFrame<T, C, A> &&f2) noexcept {
  f1 += f2;
  return f1;
}

template <typename T, size_t C, size_t A>
AlignedFrame<T, C, A> operator-(const AlignedFrame<T, C, A> &f1,
                                AlignedFrame<T, C, A> &&f2) noexcept {
  f2 -= f1;
  return f2;
}

template <typename T, size_t C, size_t A>
AlignedFrame<T, C, A> operator-(AlignedFrame<T, C, A> &&f1,
                                const AlignedFrame<T, C, A> &f2) noexcept {
  f1 -= f2;
  return f1;
}

template <typename T, size_t C, size_t A>
AlignedFrame<T, C, A> operator-(AlignedFrame<T, C, A> &&f1,
                                AlignedFrame<T, C, A> &&f2) noexcept {
  f1 -= f2;
  return f1;
}


} // namespace tdap

#endif // SPEAKERMAN_ALIGNEDFRAME_HPP
