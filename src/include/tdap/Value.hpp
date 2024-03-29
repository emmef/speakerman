#ifndef TDAP_M_VALUE_HPP
#define TDAP_M_VALUE_HPP
/*
 * tdap/Value.hpp
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

#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace tdap {

namespace helpers_tdap {
template <typename T, typename R> struct ValueTraits {
  static_assert(std::is_arithmetic<T>::value, "Value type must be arithmetic");
  static_assert(std::is_floating_point<R>::value,
                "Return type for some functions must be floating point");

  inline static constexpr T max(const T v1, const T v2) {
    return v1 < v2 ? v2 : v1;
  }

  inline static constexpr T max(const T v1, const T v2, const T v3) {
    return max(max(v1, v2), v3);
  }

  inline static constexpr T max(const T v1, const T v2, const T v3,
                                const T v4) {
    return max(max(v1, v2), max(v3, v4));
  }

  inline static constexpr T min(const T v1, const T v2) {
    return v1 < v2 ? v1 : v2;
  }

  inline static constexpr T min(const T v1, const T v2, const T v3) {
    return min(min(v1, v2), v3);
  }

  inline static constexpr T min(const T v1, const T v2, const T v3,
                                const T v4) {
    return min(min(v1, v2), min(v3, v4));
  }

  inline static constexpr T force_between(const T value, const T minimum,
                                          T const maximum) {
    return value >= minimum ? value <= maximum ? value : maximum : minimum;
  }

  inline static constexpr T is_between(const T value, const T minimum,
                                       T const maximum) {
    return value >= minimum && value <= maximum;
  }

  inline static T valid_between(const T value, const T minimum,
                                T const maximum) {
    if (value >= minimum && value <= maximum) {
      return value;
    }
    throw std::invalid_argument("Value not within expected boundaries");
  }

  inline static T valid_below(const T value, T const threshold) {
    if (value < threshold) {
      return value;
    }
    throw std::invalid_argument("Value not below threshold_");
  }

  inline static T valid_below_or_same(const T value, T const threshold) {
    if (value <= threshold) {
      return value;
    }
    throw std::invalid_argument("Value not below or equal to threshold_");
  }

  inline static const R clamp(const R x, const T a, const T b) {
    const R x1 = fabs(x - a);
    const R x2 = fabs(x - b);

    R result = x1 + a + b;
    result -= x2;
    result *= 0.5;

    return result;
  }

  inline static const R relative_distance(const T a, const T b) {
    const R absolute = fabs(a - b);
    const R averageSize = 0.5 * (fabs(a) + fabs(b));

    return absolute / averageSize;
  }

  inline static const R relative_distance_within(const T a, const T b,
                                                 const R epsilon) {
    return relative_distance(a, b) < epsilon;
  }
};

template <typename F, bool isFloat> struct values_helper {};

template <typename F> struct values_helper<F, true> : public ValueTraits<F, F> {
  static_assert(
      std::is_floating_point<F>::value,
      "Frequency type must be floating point for this specialization");

  using FloatReturn = F;
  using ValueTraits<F, F>::min;
  using ValueTraits<F, F>::max;
  using ValueTraits<F, F>::force_between;
  using ValueTraits<F, F>::valid_between;
  using ValueTraits<F, F>::valid_below;
  using ValueTraits<F, F>::valid_below_or_same;
  using ValueTraits<F, F>::is_between;
  using ValueTraits<F, F>::clamp;
  using ValueTraits<F, F>::relative_distance;
  using ValueTraits<F, F>::relative_distance_within;

  static constexpr F min_positive() { return std::numeric_limits<F>::min(); }

  static constexpr F max_exact() {
    return pow(std::numeric_limits<F>::radix, std::numeric_limits<F>::digits);
  }
};

template <typename F>
struct values_helper<F, false> : public ValueTraits<F, double> {
  using FloatReturn = double;
  using ValueTraits<F, double>::min;
  using ValueTraits<F, double>::max;
  using ValueTraits<F, double>::force_between;
  using ValueTraits<F, double>::valid_between;
  using ValueTraits<F, double>::valid_below;
  using ValueTraits<F, double>::valid_below_or_same;
  using ValueTraits<F, double>::is_between;
  using ValueTraits<F, double>::clamp;
  using ValueTraits<F, double>::relative_distance;
  using ValueTraits<F, double>::relative_distance_within;

  static constexpr F min_positive() { return static_cast<F>(1); }

  static constexpr F max_exact() { return std::numeric_limits<F>::max(); }
};

} // namespace helpers_tdap

template <typename T>
struct Value
    : public helpers_tdap::values_helper<T, std::is_floating_point<T>::value> {
  static T valid_positive(T value) {
    if (value > helpers_tdap::values_helper<
                    T, std::is_floating_point<T>::value>::min_positive()) {
      return value;
    }
    throw std::invalid_argument(
        "Value must be larger than minimum positive value");
  }
};

using Sizes = Value<size_t>;
using Floats = Value<double>;

struct Values {
  template <typename T> using Float = typename Value<T>::FloatReturn;

  template <typename T> inline static constexpr T max(const T v1, const T v2) {
    return Value<T>::max(v1, v2);
  }

  template <typename T>
  inline static constexpr T max(const T v1, const T v2, const T v3) {
    return Value<T>::max(v1, v2, v3);
  }

  template <typename T>
  inline static constexpr T max(const T v1, const T v2, const T v3,
                                const T v4) {
    return Value<T>::max(v1, v2, v3, v4);
  }

  template <typename T> inline static constexpr T min(const T v1, const T v2) {
    return Value<T>::min(v1, v2);
  }

  template <typename T>
  inline static constexpr T min(const T v1, const T v2, const T v3) {
    return Value<T>::min(v1, v2, v3);
  }

  template <typename T>
  inline static constexpr T min(const T v1, const T v2, const T v3,
                                const T v4) {
    return Value<T>::min(v1, v2, v3, v4);
  }

  template <typename T>
  inline static constexpr T force_between(const T value, const T minimum,
                                          T const maximum) {
    return Value<T>::force_between(value, minimum, maximum);
  }

  template <typename T>
  inline static constexpr T is_between(const T value, const T minimum,
                                       T const maximum) {
    return Value<T>::is_between(value, minimum, maximum);
  }

  template <typename T>
  inline static T valid_between(const T value, const T minimum,
                                T const maximum) {
    return Value<T>::valid_between(value, minimum, maximum);
  }

  template <typename T>
  inline static T valid_below(const T value, T const threshold) {
    return Value<T>::valid_below(value, threshold);
  }

  template <typename T>
  inline static T valid_below_or_same(const T value, T const threshold) {
    return Value<T>::valid_below_or_same(value, threshold);
  }

  template <typename T>
  inline static const Float<T> clamp(const Float<T> x, const T a, const T b) {
    return Value<T>::clamp(x, a, b);
  }

  template <typename T>
  inline static const Float<T> relative_distance(const T a, const T b) {
    return Value<T>::relative_distance(a, b);
  }

  template <typename T>
  inline static const Float<T>
  relative_distance_within(const T a, const T b, const Float<T> epsilon) {
    return relative_distance(a, b) < epsilon;
  }
};

namespace helper {
template <typename T, unsigned P> struct helper_constexpr_power {
  static constexpr const T value(const T value);
};

template <typename T> struct helper_constexpr_power<T, 0> {
  static constexpr const T value(const T) { return 1; }
};

template <typename T, unsigned P>
constexpr const T helper_constexpr_power<T, P>::value(const T v) {
  return v * helper_constexpr_power<T, P - 1>::value(v);
}
} // namespace helper

template <typename T, unsigned P> constexpr const T constexpr_power(T value) {
  return helper::helper_constexpr_power<T, P>::value(value);
}

} // namespace tdap

#endif // TDAP_M_VALUE_HPP
