#ifndef TDAP_M_DENORMAL_HPP
#define TDAP_M_DENORMAL_HPP
/*
 * tdap/Denormal.hpp
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

#include <cinttypes>
#include <cstddef>
#include <limits>
#include <type_traits>

#if defined(__SSE__) &&                                                        \
    (defined(__amd64__) || defined(__x86_64__) || defined(__i386__))
#include <xmmintrin.h>
#define SSE_INSTRUCTIONS_AVAILABLE 1
#else
#undef SSE_INSTRUCTIONS_AVAILABLE
#endif

namespace tdap {

// RAII FPU state class, sets FTZ and DAZ and rounding, no exceptions
// Adapted from code by mystran @ kvraudio
// http://www.kvraudio.com/forum/viewtopic.php?t=312228&postdays=0&postorder=asc&start=0

class ZFPUState {
private:
#ifdef SSE_INSTRUCTIONS_AVAILABLE
  unsigned int sse_control_store;
#endif
public:
  enum Rounding {
    kRoundNearest = 0,
    kRoundNegative,
    kRoundPositive,
    kRoundToZero,
  };

  ZFPUState(Rounding mode = kRoundToZero) {
#ifdef SSE_INSTRUCTIONS_AVAILABLE
    sse_control_store = _mm_getcsr();

    // bits: 15 = flush to zero | 6 = denormals are zero
    // bitwise-OR with exception masks 12:7 (exception flags 5:0)
    // rounding 14:13, 00 = nearest, 01 = neg, 10 = pos, 11 = to zero
    // The enum above is defined in the same order so just shift it up
    _mm_setcsr(0x8040 | 0x1f80 | ((unsigned int)mode << 13));
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
#endif
  }

  ~ZFPUState() {
#ifdef SSE_INSTRUCTIONS_AVAILABLE
    // clear exception flags, just in case (probably pointless)
    _mm_setcsr(sse_control_store & (~0x3f));
#endif
  }
};

namespace helpers_tdap {

template <typename FPTYPE, size_t selector> struct Normalize {
  static_assert(std::is_floating_point<FPTYPE>::value,
                "FPTYPE must be a floating-point type");

  static inline FPTYPE getFlushedToZero(FPTYPE &value) { return value; }

  static inline void flushToZero(FPTYPE) {}

  static constexpr bool normalizes = false;

  static constexpr size_t bits = 8 * sizeof(FPTYPE);

  static const char *method() {
    return "None: IEEE-559 compliant, but denormal definition for this size "
           "not known";
  }
};

/**
 * Specialization for non ieee-559/754 floating point formats.
 */
template <typename FPTYPE> struct Normalize<FPTYPE, 0> {
  static inline FPTYPE getFlushedToZero(FPTYPE &value) { return value; }

  static inline void flushToZero(FPTYPE) {}

  static constexpr bool normalizes = false;

  static constexpr size_t bits = 8 * sizeof(FPTYPE);

  static const char *method() { return "None: Not IEEE-559 compliant"; }
};

/**
 * Specialization for 32-bit single-precision floating
 */
template <typename FPTYPE> struct Normalize<FPTYPE, 4> {
  static inline FPTYPE getFlushedToZero(FPTYPE value) {
    union {
      FPTYPE f;
      int32_t i;
    } v;

    v.f = value;

    return v.i & 0x7f800000 ? value : 0.0f;
  }

  static inline void flushToZero(FPTYPE &value) {
    union {
      FPTYPE f;
      int32_t i;
    } v;

    v.f = value;
    if (v.i & 0x7f800000) {
      return;
    }
    value = 0.0;
  }

  static constexpr bool normalizes = true;

  static constexpr size_t bits = 8 * sizeof(FPTYPE);

  static const char *method() { return "IEEE-559 32-bit single precision"; }
};

template <typename FPTYPE> struct Normalize<FPTYPE, 8> {
  static inline FPTYPE getFlushedToZero(FPTYPE &value) {
    union {
      FPTYPE f;
      int64_t i;
    } v;

    v.f = value;

    return v.i & 0x7ff0000000000000L ? value : 0;
  }

  static inline void flushToZero(FPTYPE value) {
    union {
      FPTYPE f;
      int64_t i;
    } v;

    v.f = value;

    if (v.i & 0x7ff0000000000000L) {
      return;
    }
    value = 0.0;
  }

  static constexpr bool normalizes = false;

  static constexpr size_t bits = 8 * sizeof(FPTYPE);

  static const char *method() { return "IEEE-559 64-bit double precision"; }
};

} /* End of namespace helpers_tdap */

class Denormal {
  template <typename FPTYPE> struct Selector {
    static_assert(std::is_floating_point<FPTYPE>::value,
                  "FPTYPE must be a floating-point type");

    static constexpr size_t CLASS_SELECTOR =
        !std::numeric_limits<FPTYPE>::is_iec559 ? 0 : sizeof(FPTYPE);

    typedef helpers_tdap::Normalize<FPTYPE, CLASS_SELECTOR> Helper;
  };

public:
  template <typename F> static inline const char *method() {
    return Selector<F>::Helper::method();
  }

  template <typename F> static constexpr bool normalizes() {
    return Selector<F>::Helper::normalizes;
  }

  template <typename F> static constexpr bool bits() {
    return Selector<F>::Helper::bits;
  }

  template <typename T> static const T &flush(T &v) {
    Selector<T>::Helper::flushToZero(v);
    return v;
  }

  template <typename T> static const T &get_flushed(const T v) {
    return Selector<T>::getFlushedToZero(v);
  }
};

} // namespace tdap

#endif // TDAP_M_DENORMAL_HPP
