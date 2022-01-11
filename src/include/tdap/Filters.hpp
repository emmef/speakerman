#ifndef TDAP_M_FILTERS_HPP
#define TDAP_M_FILTERS_HPP
/*
 * tdap/Filters.hpp
 *
 * Part of TdAP
 * Time-domain Audio Processing
 * Copyright (C) 2015-2016 Michel Fleur.
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
#include <math.h>
#include <tdap/ArrayTraits.hpp>
#include <tdap/Count.hpp>
#include <tdap/Value.hpp>
#include <type_traits>

namespace tdap {

struct AllowedFilterError {
  static constexpr double MINIMUM = constexpr_power<double, 42>(0.5);
  static constexpr double DEFAULT = constexpr_power<double, 23>(0.5);
  static constexpr double MAXIMUM = constexpr_power<double, 8>(0.5);

  static inline double effective(double allowedFilterError) {
    return Value<double>::force_between(allowedFilterError, MINIMUM, MAXIMUM);
  }
};

template <typename Sample> struct Filter;

template <typename Sample> struct IdentityFilter : public Filter<Sample> {
  virtual Sample filter(Sample input) { return input; }

  virtual void reset() { /* No state */
  }
};

template <typename Sample> struct Filter {
  static_assert(std::is_arithmetic<Sample>::value,
                "Sample type must be arithmetic");

  virtual Sample filter(Sample input) = 0;

  virtual void reset() = 0;

  virtual ~Filter() = default;

  static Filter<Sample> &identity() {
    static IdentityFilter<Sample> filter;
    return filter;
  }
};

template <typename Sample> struct MultiFilter;

template <typename Sample>
struct IdentityMultiFilter : public MultiFilter<Sample> {
  virtual size_t channels() const { return Count<Sample>::max(); }

  virtual Sample filter(size_t, Sample input) { return input; }

  virtual void reset() { /* No state */
  }
};

template <typename Sample> struct MultiFilter {
  static_assert(std::is_arithmetic<Sample>::value,
                "Sample type must be arithmetic");

  virtual size_t channels() const = 0;

  virtual Sample filter(size_t channel, Sample input) = 0;

  virtual void reset() = 0;

  virtual ~MultiFilter() = default;

  static MultiFilter<Sample> &identity() {
    static IdentityMultiFilter<Sample> filter;
    return filter;
  }
};

/**
 * Returns the length in samples after which the impulse response of the
 * provided filter can be "neglected".
 * <p>
 * The impulse response can be neglected if a whole window of samples adds
 * less energy than epsilon times the total energy of the impulse response
 * so far. To retain accuracy, measurements are done in "buckets" of a
 * specified size. The moving window contains a specified number of buckets.
 * The returned value is always a multiple of the bucketSize and at least one
 * bucket greater than the window size.
 * <p>
 * The measurement continues until a maximum number of buckets has been
 * measured. If the energy level did not drop below the negligible level at
 * that time, the function returns 0.
 */
template <typename Sample>
size_t effectiveLength(Filter<Sample> &filter, size_t bucketSize,
                       size_t bucketsPerWindow, double epsilon,
                       size_t maxBuckets) {
  size_t size = bucketSize > 0 ? bucketSize : 1;
  size_t windowBuckets =
      Value<size_t>::force_between(bucketsPerWindow, 1, 10000);
  size_t usedMaxBuckets = Value<size_t>::force_between(
      maxBuckets, windowBuckets + 1, Count<Sample>::max());

  double usedEpsilon = Value<double>::force_between(epsilon, 1e-24, 1);

  size_t windowPointer = 0;
  std::unique_ptr<double> window_ptr(new double[windowBuckets]);
  double* window = window_ptr.get();
  double totalSum = 0.0;
  double bucketSum = 0.0;
  Sample input = std::is_floating_point<Sample>::value
                     ? static_cast<Sample>(1)
                     : std::numeric_limits<Sample>::max();

  for (size_t bucket = 0; bucket < usedMaxBuckets; bucket++) {
    // Determine root mean square value for bucket. As all buckets
    // have the same size, we do not re-scale for the number of
    // samples in the bucket.
    bucketSum = 0.0;
    for (size_t i = 0; i < size; i++) {
      double value = filter.filter(input);
      input = static_cast<Sample>(0);
      bucketSum += value * value;
    }

    // Add bucket sum to (rotating) window and total sum
    window[windowPointer] = bucketSum;
    windowPointer = (windowPointer + 1) % windowBuckets;

    totalSum += bucketSum;

    if (bucket >= windowBuckets) {
      // Calculate total window energy
      double windowSumSqr = 0.0;
      for (size_t i = 0; i < windowBuckets; i++) {
        windowSumSqr += window[i];
      }
      Sample windowSum = sqrt(windowSumSqr);
      // compare with total times epsilon. If the Sample type is
      // discrete, the effective length can be shorter if the
      // window energy is below the quantization error.
      if (windowSum < sqrt(totalSum) * usedEpsilon) {
        return size * (bucket + 1 - windowBuckets);
      }
    }
  }

  return 0;
}

} // namespace tdap

#endif // TDAP_M_FILTERS_HPP
