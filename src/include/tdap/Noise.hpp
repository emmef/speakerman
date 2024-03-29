#ifndef TDAP_M_NOISE_HPP
#define TDAP_M_NOISE_HPP
/*
 * tdap/Noise.hpp
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

#include <random>
#include <tdap/Integration.hpp>
#include <tdap/Value.hpp>

namespace tdap {

template <typename Sample, class Rnd> class RandomNoise {
  static_assert(std::is_floating_point<Sample>::value,
                "Sample must be a floating-point type");

  static constexpr double middle =
      0.5 * ((double)Rnd::min() + (double)Rnd::max());
  static constexpr double width = (double)Rnd::max() - (double)Rnd::min();
  static constexpr double unityMultiplier = 1.0 / width;

  Rnd random;
  const Sample add;
  const Sample multiply;

public:
  RandomNoise(Sample offset, Sample amplitude)
      : add(offset - unityMultiplier * middle * amplitude),
        multiply(unityMultiplier * amplitude) {}

  RandomNoise() : RandomNoise(0.0, 1e-10) {}

  Sample next() { return multiply * random() + add; }

  Sample operator()() { return next(); }
};

typedef RandomNoise<double, std::minstd_rand> DefaultNoise;
typedef RandomNoise<double, std::minstd_rand0> DefaultNoise0;

struct PinkNoise {
public:
  static constexpr int MAX_RANDOM_ROWS = 30;
  static constexpr int RANDOM_BITS = 24;
  static constexpr int RANDOM_SHIFT = 32 - RANDOM_BITS;

  template <class Rnd, int ACCURACY> class Generator {
    static_assert(ACCURACY >= 4 && ACCURACY <= MAX_RANDOM_ROWS,
                  "Invalid accuracy");

    static constexpr uint_fast32_t middle =
        0.5 * ((double)Rnd::min() + (double)Rnd::max());
    static constexpr double width = (double)Rnd::max() - (double)Rnd::min();
    static constexpr double unityMultiplier = 1.0 / width;

    Rnd white_;
    int32_t random_;
    int32_t rows_[MAX_RANDOM_ROWS];
    int32_t runningSum_; /* Used to optimize summing of generators. */
    size_t index_;       /* Incremented each sample. */
    size_t indexMask_;   /* Index wrapped by ANDing with this mask. */
    double scale_;
    double offset_;
    IntegrationCoefficients<double> dcCoefficients;

  public:
    Generator(double scale, size_t integrationSamples) {
      dcCoefficients.setCharacteristicSamples(integrationSamples);
      random_ = white_();
      index_ = 0;
      indexMask_ = (1 << ACCURACY) - 1;
      offset_ = 0;
      setScale(scale);
      /* Initialize rows. */
      for (int i = 0; i < ACCURACY; i++) {
        rows_[i] = 0;
      }
      runningSum_ = 0;
      /* Stabilize around zero */
      long positive = 0;
      long negative = 0;
      for (size_t i = 0; i < 19200000; i++) {
        double result = this->operator()();
        if (result < 0) {
          negative++;
        } else if (result > 0) {
          positive++;
        }
        double ratio = negative > 0 ? positive / negative : 0;
        if (ratio > 0.99 && ratio < 1.01) {
          std::cout << "Offset stabilized in " << i << std::endl;
          break;
        }
      }
    }

    void setScale(double scale) {
      double usedScale = std::max(1e-20, scale);
      /* Calculate maximum possible signed random value. Extra 1 for white noise
       * always added. */
      int32_t pmax = (ACCURACY + 1) * (1 << (RANDOM_BITS - 1));
      double newScale = usedScale / pmax;
      scale_ = newScale;
    }

    void setIntegrationSamples(size_t integrationSamples) {
      dcCoefficients.setCharacteristicSamples(integrationSamples);
    }

    double operator()() {
      /* Increment and mask index. */
      index_ = (index_ + 1) & indexMask_;

      /* If index is zero, don't update any random values. */
      if (index_ != 0) {
        /* Determine how many trailing zeros in PinkIndex. */
        /* This algorithm will hang if n==0 so test first. */
        size_t numZeros = 0;
        size_t n = index_;
        while ((n & 1) == 0) {
          n = n >> 1;
          numZeros++;
        }

        /* Replace the indexed ROWS random value.
         * Subtract and add back to RunningSum instead of adding all the random
         * values together. Only one changes each time.
         */
        runningSum_ -= rows_[numZeros];
        int32_t newRandom = white_() >> RANDOM_SHIFT;
        runningSum_ += newRandom;
        rows_[numZeros] = newRandom;
      }

      /* Add extra white noise value. */
      int32_t sum = runningSum_ + (white_() >> RANDOM_SHIFT);

      /* Scale to range */

      dcCoefficients.integrate((double)sum, offset_);
      return scale_ * (sum - offset_);
    }
  };

  typedef Generator<std::minstd_rand, PinkNoise::MAX_RANDOM_ROWS> Default;
};

template <typename Sample> struct AddedNoise {
  static constexpr double MINIMUM =
      std::is_floating_point<Sample>::value ? 1e-20 : 1;

  static constexpr double MAXIMUM =
      std::is_floating_point<Sample>::value
          ? pow(0.5, 8)
          : std::numeric_limits<Sample>::max() / 256;

  static constexpr double DEFAULT =
      std::is_floating_point<Sample>::value ? pow(0.5, 24) : 0.5;

  static double effective(double noise) {
    return Value<Sample>::between(noise, MINIMUM, MAXIMUM);
  }
};

} // namespace tdap

#endif // TDAP_M_NOISE_HPP
