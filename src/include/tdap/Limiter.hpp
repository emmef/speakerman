#ifndef SPEAKERMAN_LIMITER_HPP
#define SPEAKERMAN_LIMITER_HPP
/*
 * tdap/Limiter.hpp
 *
 * Added by michel on 2020-02-09
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

#include <tdap/Followers.hpp>

namespace tdap {

  template<typename T>
  class Limiter {
  public:
    virtual void setPredictionAndThreshold(size_t prediction,
            T threshold,
            T sampleRate) = 0;

    virtual size_t latency() const noexcept = 0;

    virtual T getGain(T sample) noexcept = 0;

    virtual ~Limiter() = default;
  };

  template<typename T>
  class CheapLimiter : public Limiter<T> {
    IntegrationCoefficients<T> release_;
    IntegrationCoefficients<T> attack_;
    T hold_ = 0;
    T integrated1_ = 0;
    T integrated2_ = 0;
    T threshold_ = 1.0;
    T adjustedPeakFactor_ = 1.0;
    size_t holdCount_= 0;
    bool inRelease = false;
    size_t latency_ = 0;

    static constexpr double predictionFactor = 0.30;

  public:
    void setPredictionAndThreshold(size_t prediction,
            T threshold,
            T sampleRate) override {
      double release = sampleRate * 0.04;
      double thresholdFactor = 1.0 - exp(-1.0 / predictionFactor);
      latency_ = prediction;
      attack_.setCharacteristicSamples(predictionFactor * prediction);
      release_.setCharacteristicSamples(release * M_SQRT1_2);
      threshold_ = threshold;
      integrated1_ = integrated2_ = threshold_;
      adjustedPeakFactor_ = 1.0 / thresholdFactor;
    }

    size_t latency() const noexcept override {
      return latency_;
    }

    T getGain(T sample) noexcept override
    {
      T peak = std::max(sample, threshold_);
      if (peak >= hold_) {
        hold_ = peak * adjustedPeakFactor_;
        holdCount_ = latency_ + 1;
        integrated1_ += attack_.inputMultiplier() * (hold_ -
                integrated1_);
        integrated2_ = integrated1_;
      }
      else if (holdCount_) {
        holdCount_--;
        integrated1_ += attack_.inputMultiplier() * (hold_ -
                integrated1_);
        integrated2_ = integrated1_;
      }
      else {
        hold_ = peak;
        integrated1_ += release_.inputMultiplier() * (hold_ -
                integrated1_);
        integrated2_ += release_.inputMultiplier() * (integrated1_ -
                integrated2_);
      }
      return threshold_ / integrated2_;
    }
  };

  template<typename T>
  class ZeroPredictionHardAttackLimiter : public Limiter<T> {
    IntegrationCoefficients<T> release_;
    T integrated1_ = 0;
    T integrated2_ = 0;
    T threshold_ = 1.0;

    static constexpr double predictionFactor = 0.30;

  public:
    void setPredictionAndThreshold( size_t prediction,
            T threshold,
            T sampleRate) override {
      double release = sampleRate * 0.04;
      release_.setCharacteristicSamples(release * M_SQRT1_2);
      threshold_ = threshold;
      integrated1_ = integrated2_ = threshold_;
    }

    size_t latency() const noexcept override {
      return 0;
    }

    T getGain(T sample) noexcept override
    {
      T peak = std::max(sample, threshold_);
      if (peak >= integrated1_) {
        integrated2_ = integrated1_ = peak;
      }
      else {
        integrated1_ += release_.inputMultiplier() * (peak -
                integrated1_);
        integrated2_ += release_.inputMultiplier() * (integrated1_ -
                integrated2_);
      }
      return threshold_ / integrated2_;
    }
  };

  template<typename T>
  class TriangularLimiter : public Limiter<T>
  {
    static constexpr double ATTACK_SMOOTHFACTOR = 0.1;
    static constexpr double RELEASE_SMOOTHFACTOR = 0.3;
    static constexpr double TOTAL_TIME_FACTOR = 1.0 + ATTACK_SMOOTHFACTOR;
    static constexpr double ADJUST_THRESHOLD = 0.99999;

    TriangularFollower<T> follower_;
    IntegrationCoefficients<T> release_;
    T integrated_ = 0;
    T adjustedThreshold_;
    size_t latency_ = 0;
  public:
    TriangularLimiter() : follower_(1000) {}

    void setPredictionAndThreshold(size_t prediction, T threshold, T sampleRate) override
    {
      size_t attack = prediction;
      latency_ = prediction;
      size_t release = Floats::clamp(
              prediction * 10,
              sampleRate * 0.010,
              sampleRate * 0.020);
      adjustedThreshold_ = threshold * ADJUST_THRESHOLD;
      follower_.setTimeConstantAndSamples(attack, release, adjustedThreshold_);
      release_.setCharacteristicSamples(release * RELEASE_SMOOTHFACTOR);
      integrated_ = adjustedThreshold_;
    }

    size_t latency() const noexcept override {
      return latency_;
    }

    T getGain(T input) noexcept override
    {
      T followed = follower_.follow(input);
      T integrationFactor;
      if (followed > integrated_) {
        integrationFactor = 1.0;//attack_.inputMultiplier();
      }
      else  {
        integrationFactor = release_.inputMultiplier();
      }
      integrated_ += integrationFactor * (followed - integrated_);
      return adjustedThreshold_ / integrated_;
    }
  };
} // namespace tdap

#endif //SPEAKERMAN_LIMITER_HPP
