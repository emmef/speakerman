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

#include <algorithm>
#include <tdap/Followers.hpp>

namespace tdap {

template <typename T> class Limiter {
public:
  virtual void setPredictionAndThreshold(size_t prediction, T threshold,
                                         T sampleRate) = 0;

  virtual size_t latency() const noexcept = 0;

  virtual T getGain(T sample) noexcept = 0;

  virtual ~Limiter() = default;
};

template <typename T> class CheapLimiter : public Limiter<T> {
  IntegrationCoefficients<T> release_;
  IntegrationCoefficients<T> attack_;
  T hold_ = 0;
  T integrated1_ = 0;
  T integrated2_ = 0;
  T threshold_ = 1.0;
  T adjustedPeakFactor_ = 1.0;
  size_t holdCount_ = 0;
  bool inRelease = false;
  size_t latency_ = 0;

  static constexpr double predictionFactor = 0.30;

public:
  void setPredictionAndThreshold(size_t prediction, T threshold,
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

  size_t latency() const noexcept override { return latency_; }

  T getGain(T sample) noexcept override {
    T peak = std::max(sample, threshold_);
    if (peak >= hold_) {
      hold_ = peak * adjustedPeakFactor_;
      holdCount_ = latency_ + 1;
      integrated1_ += attack_.inputMultiplier() * (hold_ - integrated1_);
      integrated2_ = integrated1_;
    } else if (holdCount_) {
      holdCount_--;
      integrated1_ += attack_.inputMultiplier() * (hold_ - integrated1_);
      integrated2_ = integrated1_;
    } else {
      hold_ = peak;
      integrated1_ += release_.inputMultiplier() * (hold_ - integrated1_);
      integrated2_ +=
          release_.inputMultiplier() * (integrated1_ - integrated2_);
    }
    return threshold_ / integrated2_;
  }
};

template <typename T>
class FastLookAheadLimiter : public Limiter<T> {
  FastSmoothHoldFollower<T> follower;
public:
  void setPredictionAndThreshold(size_t prediction, T threshold,
                                 T sampleRate) override {
    T predictionSeconds = 1.0 * prediction / sampleRate;
    follower.setPredictionAndThreshold(
        predictionSeconds,
        threshold,
        sampleRate,
        std::clamp(predictionSeconds * 5, 0.003, 0.02),
        threshold);
  }

  size_t latency() const noexcept override { return follower.latency(); }

  T getGain(T sample) noexcept override {
    return follower.getGain(sample);
  }
};

template <typename T>
class ZeroPredictionHardAttackLimiter : public Limiter<T> {
  IntegrationCoefficients<T> release_;
  T integrated1_ = 0;
  T integrated2_ = 0;
  T threshold_ = 1.0;

  static constexpr double predictionFactor = 0.30;

public:
  void setPredictionAndThreshold(size_t prediction, T threshold,
                                 T sampleRate) override {
    size_t release =
        Floats::clamp(prediction * 8, sampleRate * 0.010, sampleRate * 0.020);
    release_.setCharacteristicSamples(release * M_SQRT1_2);
    threshold_ = threshold;
    integrated1_ = integrated2_ = threshold_;
    std::cout << "HARD no prediction limiter" << std::endl;
  }

  size_t latency() const noexcept override { return 0; }

  T getGain(T sample) noexcept override {
    T peak = std::max(sample, threshold_);
    if (peak >= integrated1_) {
      integrated2_ = integrated1_ = peak;
    } else {
      integrated1_ += release_.inputMultiplier() * (peak - integrated1_);
      integrated2_ +=
          release_.inputMultiplier() * (integrated1_ - integrated2_);
    }
    return threshold_ / integrated2_;
  }
};

template <typename T> class TriangularLimiter : public Limiter<T> {
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

  void setPredictionAndThreshold(size_t prediction, T threshold,
                                 T sampleRate) override {
    size_t attack = prediction;
    latency_ = prediction;
    size_t release =
        Floats::clamp(prediction * 8, sampleRate * 0.010, sampleRate * 0.020);
    adjustedThreshold_ = threshold * ADJUST_THRESHOLD;
    follower_.setTimeConstantAndSamples(attack, release, adjustedThreshold_);
    release_.setCharacteristicSamples(release * RELEASE_SMOOTHFACTOR);
    integrated_ = adjustedThreshold_;
    std::cout << "TriangularLimiter" << std::endl;
  }

  size_t latency() const noexcept override { return latency_; }

  T getGain(T input) noexcept override {
    T followed = follower_.follow(input);
    T integrationFactor;
    if (followed > integrated_) {
      integrationFactor = 1.0; // attack_.inputMultiplier();
    } else {
      integrationFactor = release_.inputMultiplier();
    }
    integrated_ += integrationFactor * (followed - integrated_);
    return adjustedThreshold_ / integrated_;
  }
};

template <typename sample_t> class PredictiveSmoothEnvelopeLimiter {

  Array<sample_t> attack_envelope_;
  Array<sample_t> release_envelope_;
  Array<sample_t> peaks_;

  sample_t threshold_;
  sample_t smoothness_;
  sample_t current_peak_;

  size_t release_count_;
  size_t current_sample;
  size_t attack_samples_;
  size_t release_samples_;

  static void create_smooth_semi_exponential_envelope(sample_t *envelope,
                                                      const size_t length,
                                                      size_t periods) {
    const sample_t periodExponent = exp(-1.0 * periods);
    size_t i;
    for (i = 0; i < length; i++) {
      envelope[i] = limiter_envelope_value(i, length, periods, periodExponent);
    }
  }

  template <typename... A>
  static void create_smooth_semi_exponential_envelope(
      ArrayTraits<sample_t, A...> envelope,
      const size_t length, size_t periods) {
    create_smooth_semi_exponential_envelope(envelope + 0, length, periods);
  }

  static inline double limiter_envelope_value(const size_t i,
                                              const size_t length,
                                              const double periods,
                                              const double periodExponent) {
    const double angle = M_PI * (i + 1) / length;
    const double ePower = 0.5 * periods * (cos(angle) - 1.0);
    const double envelope =
        (exp(ePower) - periodExponent) / (1.0 - periodExponent);

    return envelope;
  }

  void generate_envelopes_reset(bool recalculate_attack_envelope = true,
                                bool recalculate_release_envelope = true) {
    if (recalculate_attack_envelope) {
      PredictiveSmoothEnvelopeLimiter::create_smooth_semi_exponential_envelope(
          attack_envelope_ + 0, attack_samples_, smoothness_);
    }
    if (recalculate_release_envelope) {
      PredictiveSmoothEnvelopeLimiter::create_smooth_semi_exponential_envelope(
          release_envelope_ + 0, release_samples_, smoothness_);
    }
    release_count_ = 0;
    current_peak_ = 0;
    current_sample = 0;
  }

  inline const sample_t getAmpAndMoveToNextSample(const sample_t newValue) {
    const sample_t pk = peaks_[current_sample];
    peaks_[current_sample] = newValue;
    current_sample = (current_sample + attack_samples_ - 1) % attack_samples_;
    const sample_t threshold = threshold_;
    return threshold / (threshold + pk);
  }

public:
  PredictiveSmoothEnvelopeLimiter(sample_t threshold, sample_t smoothness,
                                  const size_t max_attack_samples,
                                  const size_t max_release_samples)
      : threshold_(threshold), smoothness_(smoothness),
        attack_envelope_(max_attack_samples),
        release_envelope_(max_release_samples), peaks_(max_attack_samples),
        release_count_(0), current_peak_(0), current_sample(0),
        attack_samples_(max_attack_samples),
        release_samples_(max_release_samples) {
    generate_envelopes_reset();
  }

  bool reconfigure(size_t attack_samples, size_t release_samples,
                   sample_t threshold, sample_t smoothness) {
    if (attack_samples == 0 || attack_samples > attack_envelope_.capacity()) {
      return false;
    }
    if (release_samples == 0 ||
        release_samples > release_envelope_.capacity()) {
      return false;
    }
    sample_t new_threshold = Value<sample_t>::force_between(threshold, 0.01, 1);
    sample_t new_smoothness = Value<sample_t>::force_between(smoothness, 1, 4);
    bool recalculate_attack_envelope =
        attack_samples != attack_samples_ || new_smoothness != smoothness_;
    bool recalculate_release_envelope =
        release_samples != release_samples_ || new_smoothness != smoothness_;
    attack_samples_ = attack_samples;
    release_samples_ = release_samples;
    threshold_ = threshold;
    smoothness_ = smoothness;
    generate_envelopes_reset(recalculate_attack_envelope,
                             recalculate_release_envelope);
    return true;
  }

  bool set_smoothness(sample_t smoothness) {
    return reconfigure(attack_samples_, release_samples_, threshold_,
                       smoothness);
  }

  bool set_attack_samples(size_t samples) {
    return reconfigure(samples, release_samples_, threshold_, smoothness_);
  }

  bool set_release_samples(size_t samples) {
    return reconfigure(attack_samples_, samples, threshold_, smoothness_);
  }

  bool set_threshold(double threshold) {
    return reconfigure(attack_samples_, release_samples_, threshold,
                       smoothness_);
  }

  const sample_t
  limiter_submit_peak_return_amplification(sample_t samplePeakValue) {
    static int cnt = 0;
    const size_t prediction = attack_envelope_.size();

    const sample_t relativeValue = samplePeakValue - threshold_;
    const int withinReleasePeriod = release_count_ < release_envelope_.size();
    const sample_t releaseCurveValue =
        withinReleasePeriod ? current_peak_ * release_envelope_[release_count_]
                            : 0.0;

    if (relativeValue < releaseCurveValue) {
      /*
       * The signal is below either the threshold_ or the projected release
       * curve of the last highest peak. We can just "follow" the release curve.
       */
      if (withinReleasePeriod) {
        release_count_++;
      }
      cnt++;
      return getAmpAndMoveToNextSample(releaseCurveValue);
    }
    /**
     * Alas! We can forget about the last peak.
     * We will have alter the prediction values so that the current,
     * new peak, will be "predicted".
     */
    release_count_ = 0;
    current_peak_ = relativeValue;
    /**
     * We will try to project the default attack-predicition curve,
     * (which is the relativeValue (peak) with the nicely smooth
     * attackEnvelope) into the "future".
     * As soon as this projection hits (is not greater than) a previously
     * predicted value, we proceed to the next step.
     */
    const size_t max_t = attack_samples_ - 1;
    size_t tClash; // the hitting point
    size_t t;
    for (tClash = 0, t = current_sample; tClash < prediction; tClash++) {
      t = t < max_t ? t + 1 : 0;
      const sample_t existingValue = peaks_[t];
      const sample_t projectedValue = attack_envelope_[tClash] * relativeValue;
      if (projectedValue <= existingValue) {
        break;
      }
    }

    /**
     * We have a clash. We will now blend the peak with the
     * previously predicted curve, using the attackEnvelope
     * as blend-factor. If tClash is smaller than the complete
     * prediction-length, the attackEnvelope will be compressed
     * to fit exactly up to that clash point.
     * Due to the properties of the attackEnvelope it can be
     * mathematically proven that the newly produced curve is
     * always larger than the previous one in the clash range and
     * will blend smoothly with the existing curve.
     */
    size_t i;
    for (i = 0, t = current_sample; i < tClash; i++) {
      t = t < max_t ? t + 1 : 0;
      // get the compressed attack_envelope_ value
      const sample_t blendFactor =
          attack_envelope_[i * (prediction - 1) / tClash];
      // blend the peak value with the previously calculated peak
      peaks_[t] = relativeValue * blendFactor + (1.0 - blendFactor) * peaks_[t];
    }

    return getAmpAndMoveToNextSample(relativeValue);
  }
};

} // namespace tdap

#endif // SPEAKERMAN_LIMITER_HPP
