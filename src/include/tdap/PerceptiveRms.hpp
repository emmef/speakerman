#ifndef TDAP_M_PERCEPTIVE_RMS_HPP
#define TDAP_M_PERCEPTIVE_RMS_HPP
/*
 * tdap/PerceptiveRms.hpp
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

#include <algorithm>
#include <cmath>
#include <iostream>
#include <type_traits>

#include <tdap/FixedSizeArray.hpp>
#include <tdap/Followers.hpp>
#include <tdap/Power2.hpp>
#include <tdap/TrueFloatingPointWindowAverage.hpp>

namespace tdap {
using namespace std;

/**
 * Defines a system that can calculate weighted measurements at different
 * integration times according to how loud they are perceived. This can be used
 * to construct perceptive RMS measurements.
 */
struct Perceptive {
  static constexpr double MIN_FAST_SECONDS = 0.0001;
  static constexpr double DEF_FAST_SECONDS = 0.0004;
  static constexpr double MAX_FAST_SECONDS = 0.01;
  static constexpr double PERCEPTIVE_SECONDS = 0.400;
  static constexpr double DEF_SLOW_SECONDS = 2.4;
  static constexpr double MAX_SLOW_SECONDS = 10.0000;
  static constexpr double PERCEPTIVE_WEIGHT_POWER = 0.25;

  static constexpr double MIN_STEP_FACTOR = 1.2;
  static constexpr double MAX_HOLD_SECONDS = 0.02;
  static constexpr double MAX_RELEASE_SECONDS = 0.04;

  struct Metrics {
    Metrics() = default;

    Metrics(size_t count, size_t perceptive, double slowSeconds,
            double fastSeconds)
        : count_(std::max(2ul, count)),
          perceptive_(std::min(perceptive, count_ - 2ul)),
          slowSeconds_(
              std::clamp(slowSeconds,
                         PERCEPTIVE_SECONDS * pow(MIN_STEP_FACTOR, perceptive_),
                         MAX_SLOW_SECONDS)),
          fastSeconds_(
              std::clamp(fastSeconds, MIN_FAST_SECONDS, MAX_FAST_SECONDS)) {}

    [[nodiscard]] size_t count() const noexcept { return count_; }

    [[nodiscard]] size_t perceptive() const noexcept { return perceptive_; }

    [[nodiscard]] size_t slowSteps() const noexcept { return perceptive_; }

    [[nodiscard]] size_t fastSteps() const noexcept {
      return fastest() - perceptive_;
    }

    [[nodiscard]] size_t slowest() const noexcept { return 0; }

    [[nodiscard]] size_t fastest() const noexcept { return count_ - 1; }

    [[nodiscard]] double fastSeconds() const noexcept { return fastSeconds_; }

    [[nodiscard]] double slowSeconds() const noexcept { return slowSeconds_; }

    [[nodiscard]] double holdSeconds() const noexcept {
      return std::min(fastSeconds_ * 3, MAX_HOLD_SECONDS);
    }

    [[nodiscard]] double attackSeconds() const noexcept {
      return 0.5 * fastSeconds_;
    }

    [[nodiscard]] double releaseSeconds() const noexcept {
      return std::min(fastSeconds_, MAX_RELEASE_SECONDS);
    }

    [[nodiscard]] double weight(size_t index) const {
      if (index <= perceptive_) {
        return 1.0;
      }
      IndexPolicy::method(index, count_);
      double exponent = double(index - perceptive_) / fastSteps();
      double base = fastSeconds_ / PERCEPTIVE_SECONDS;
      return std::max(0.25, pow(base, exponent * PERCEPTIVE_WEIGHT_POWER));
    }

    [[nodiscard]] double seconds(size_t index) const {
      if (index < perceptive_) {
        double exponent = double(index) / perceptive_;
        double base = PERCEPTIVE_SECONDS / slowSeconds_;
        return slowSeconds_ * pow(base, exponent);
      }
      IndexPolicy::method(index, count_);
      if (index > perceptive_) {
        double exponent = double(index - perceptive_) / fastSteps();
        double base = fastSeconds_ / PERCEPTIVE_SECONDS;
        return PERCEPTIVE_SECONDS * pow(base, exponent);
      }
      return PERCEPTIVE_SECONDS;
    }

    [[nodiscard]] static Metrics createWithEvenSteps(double slowSeconds,
                                                     double fastSeconds,
                                                     size_t maxLevels) {
      size_t validMaxLevels = std::max(2ul, maxLevels);
      if (validMaxLevels == 2) {
        return {validMaxLevels, 0, slowSeconds, fastSeconds};
      }
      double slow = min(MAX_SLOW_SECONDS, slowSeconds);
      double maxSlowSteps =
          (log(slow) - log(PERCEPTIVE_SECONDS)) / log(MIN_STEP_FACTOR);
      double fast = clamp(fastSeconds, MIN_FAST_SECONDS, MAX_FAST_SECONDS);
      double maxFastSteps =
          (log(PERCEPTIVE_SECONDS) - log(fast)) / log(MIN_STEP_FACTOR);
      size_t slowSteps = maxSlowSteps;
      size_t fastSteps = maxFastSteps;
      if (slowSteps == 0) {
        return {validMaxLevels, 0, PERCEPTIVE_SECONDS, fast};
      }
      if (slowSteps + fastSteps + 1 <= validMaxLevels) {
        return {slowSteps + fastSteps + 1, slowSteps, slow, fast};
      }
      size_t maxSteps = validMaxLevels - 1;
      double scaleFactor = maxSteps / (maxSlowSteps + maxFastSteps);
      maxSlowSteps = maxSlowSteps * scaleFactor;
      maxFastSteps = maxFastSteps * scaleFactor;
      bool hasSlowSteps = slowSteps > 0;
      slowSteps = maxSlowSteps;
      fastSteps = maxFastSteps;
      size_t extraSteps = maxSteps - slowSteps - fastSteps;
      if (extraSteps == 0) {
        if (hasSlowSteps && slowSteps == 0) {
          slowSteps++;
        }
        return {validMaxLevels, slowSteps, slowSeconds, fastSeconds};
      } else if (extraSteps > 1 && (maxFastSteps - maxSlowSteps <= 1.0)) {
        return {validMaxLevels, slowSteps + 1, slowSeconds, fastSeconds};
      }
      if (hasSlowSteps && slowSteps == 0) {
        slowSteps++;
      }
      return {validMaxLevels, slowSteps, slowSeconds, fastSeconds};
    }

  private:
    size_t count_ = 3;
    size_t perceptive_ = 1;
    double slowSeconds_ = DEF_SLOW_SECONDS;
    double fastSeconds_ = DEF_FAST_SECONDS;
  };
};

} // namespace tdap

static std::ostream &operator<<(std::ostream &stream,
                                const tdap::Perceptive::Metrics &metrics) {
  stream << "Perceptive::Metrics:" << std::endl;
  static constexpr size_t LENGTH = 32;
  char format[LENGTH];
  for (size_t i = 0; i < metrics.count(); i++) {
    snprintf(format, LENGTH, " %2zu.%2s%7.4lfs weight %3zu%%", i,
             ((i == metrics.perceptive()) ? "*" : " "), metrics.seconds(i),
             size_t(metrics.weight(i) * 100));
    stream << format << std::endl;
  }
  return stream;
}

namespace tdap {

template <typename S>
SmoothDetection<S> static createDetector(size_t sampleRate, const Perceptive::Metrics &metrics) {
  S predictionSamples = 0.5 + sampleRate * std::min(0.001, metrics.holdSeconds());
  SmoothDetection<S> follower;
  follower.setAttackAndHold(metrics.attackSeconds() * sampleRate, predictionSamples);
  follower.setReleaseSamples(0.5 + metrics.releaseSeconds() * sampleRate);
  return follower;
}


template <typename S, size_t MAX_WINDOW_SAMPLES, size_t LEVELS>
class PerceptiveRms {
  static_assert(Values::is_between(LEVELS, (size_t)3, (size_t)32),
                "Levels must be between 3 and 32");

  TrueFloatingPointWeightedMovingAverageSet<S> rms_;

  SmoothDetection<S> follower_;

public:
  PerceptiveRms()
      : rms_(MAX_WINDOW_SAMPLES, MAX_WINDOW_SAMPLES * 10, LEVELS, 0)  {};

  void configure(size_t sample_rate, const Perceptive::Metrics &metrics,
                 S initial_value = 0.0) {
    rms_.setUsedWindows(metrics.count());
    for (size_t i = 0; i < metrics.count(); i++) {
      double weight = metrics.weight(i);
      rms_.setWindowSizeAndScale(i, 0.5 + sample_rate * metrics.seconds(i),
                                 weight * weight);
    }
    rms_.setAverages(initial_value);
    follower_ = createDetector<S>(sample_rate, metrics);
    follower_.setOutput(initial_value);
  }

  S add_square_get_detection(S square, S minimum = 0) {
    S value = sqrt(rms_.addInputGetMax(square, minimum));
    return follower_.apply(value);
  }

  size_t getLatency() const { return follower_.getHoldSamples(); }
};

} // namespace tdap

#endif // TDAP_M_PERCEPTIVE_RMS_HPP
