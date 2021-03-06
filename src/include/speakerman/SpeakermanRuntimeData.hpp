/*
 * SpeakermanConfig.hpp
 *
 * Part of 'Speaker management system'
 *
 * Copyright (C) 2013-2014 Michel Fleur.
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

#ifndef SMS_SPEAKERMAN_SPEAKERMAN_RUNTIME_CONFIG_GUARD_H_
#define SMS_SPEAKERMAN_SPEAKERMAN_RUNTIME_CONFIG_GUARD_H_

#include <speakerman/SpeakermanConfig.hpp>
#include <tdap/IirBiquad.hpp>
#include <tdap/Integration.hpp>

namespace speakerman {
using namespace tdap;
struct SpeakerManLevels {
  static constexpr double getThreshold(double threshold) {
    return Values::force_between(threshold, GroupConfig::MIN_THRESHOLD,
                                 GroupConfig::MAX_THRESHOLD);
  }

  static constexpr double getLimiterThreshold(double threshold,
                                              double sloppyFactor) {
    return Values::min(1.0, 4 * getThreshold(threshold));
  }

  static constexpr double getRmsThreshold(double threshold,
                                          double relativeBandWeight) {
    return getThreshold(threshold) *
           Values::force_between(relativeBandWeight, 0.001, 0.999);
  }
};

template <typename T> class EqualizerFilterData {
  using Coefficients = FixedSizeIirCoefficients<T, 2>;
  Coefficients biquad1_;
  Coefficients biquad2_;
  size_t count_;

public:
  size_t count() const { return count_; }

  const Coefficients &biquad1() const { return biquad1_; }

  const Coefficients &biquad2() const { return biquad2_; }

  static EqualizerFilterData<T>
  createConfigured(size_t eqs, const EqualizerConfig *eq, double sampleRate) {
    EqualizerFilterData<T> result;
    result.configure(eqs, eq, sampleRate);
    return result;
  }

  void configure(size_t eqs, const EqualizerConfig *const eq,
                 double sampleRate) {
    count_ = eqs;
    if (eqs > 0) {
      auto w1 = biquad1_.wrap();
      BiQuad::setParametric(w1, sampleRate, eq[0].center, eq[0].gain,
                            eq[0].bandwidth);
      if (eqs > 1) {
        auto w2 = biquad2_.wrap();
        BiQuad::setParametric(w2, sampleRate, eq[1].center, eq[1].gain,
                              eq[1].bandwidth);
      }
    }
  }

  static EqualizerFilterData<T> createConfigured(const GroupConfig &config,
                                                 double sampleRate) {
    return createConfigured(config.eqs, config.eq, sampleRate);
  }

  static EqualizerFilterData<T> createConfigured(const SpeakermanConfig &config,
                                                 double sampleRate) {
    return createConfigured(config.eqs, config.eq, sampleRate);
  }

  void reset() {
    biquad1_.setTransparent();
    biquad2_.setTransparent();
    count_ = 0;
  }
};

template <typename T, size_t BANDS> class GroupRuntimeData {
  FixedSizeArray<T, SpeakermanConfig::MAX_GROUPS> volume_;
  size_t delay_;
  bool useSub_;
  bool mono_;
  T bandRmsScale_[BANDS];
  T limiterScale_;
  T limiterThreshold_;
  T signalMeasureFactor_;
  EqualizerFilterData<T> filterConfig_;

public:
  const FixedSizeArray<T, SpeakermanConfig::MAX_GROUPS> &volume() const {
    return volume_;
  }

  T bandRmsScale(size_t i) const {
    return bandRmsScale_[IndexPolicy::array(i, BANDS)];
  }

  T limiterScale() const { return limiterScale_; }

  T limiterThreshold() const { return limiterThreshold_; }

  T signalMeasureFactor() const { return signalMeasureFactor_; }

  size_t delay() const { return delay_; }

  bool useSub() const { return useSub_; }

  bool isMono() const { return mono_; }

  const EqualizerFilterData<T> &filterConfig() const { return filterConfig_; }

  void reset() {
    volume_.zero();
    delay_ = 0;
    limiterScale_ = 1;
    limiterThreshold_ = 1;
    for (size_t band = 0; band < BANDS; band++) {
      bandRmsScale_[band] = BANDS;
    }
    filterConfig_.reset();
  }

  void setFilterConfig(const EqualizerFilterData<T> &source) {
    filterConfig_ = source;
  }

  template <typename... A>
  void setLevels(const GroupConfig &conf, double threshold_scaling,
                 size_t channels, double sloppyFactor, size_t delay,
                 const ArrayTraits<A...> &relativeBandWeights) {
    for (size_t i = 0; i < SpeakermanConfig::MAX_GROUPS; i++) {
      double v = Values::force_between(conf.volume[i], GroupConfig::MIN_VOLUME,
                                       GroupConfig::MAX_VOLUME);
      volume_[i] = v < 1e-6 ? 0 : v;
    }
    delay_ = delay;
    useSub_ = conf.use_sub == 1;
    mono_ = conf.mono == 1;
    double threshold = Value<double>::min(conf.threshold * threshold_scaling,
                                          GroupConfig::MAX_THRESHOLD);
    limiterThreshold_ =
        SpeakerManLevels::getLimiterThreshold(threshold, sloppyFactor);
    limiterScale_ = 1.0 / limiterThreshold_;
    for (size_t band = 0; band < BANDS; band++) {
      bandRmsScale_[band] = 1.0 / SpeakerManLevels::getRmsThreshold(
                                      threshold, relativeBandWeights[band]);
    }
    signalMeasureFactor_ = 1.0 / (sqrt(channels) * threshold);
  }

  void adjustDelay(size_t delay) {
    delay_ = delay > delay_ ? 0 : delay_ - delay;
  }

  void init(const GroupRuntimeData<T, BANDS> &source) {
    *this = source;
    volume_.zero();
  }

  void approach(const GroupRuntimeData<T, BANDS> &target,
                const IntegrationCoefficients<T> &integrator) {
    for (size_t i = 0; i < SpeakermanConfig::MAX_GROUPS; i++) {
      integrator.integrate(target.volume_[i], volume_[i]);
    }
    integrator.integrate(target.limiterThreshold_, limiterThreshold_);
    integrator.integrate(target.limiterScale_, limiterScale_);
    for (size_t band = 0; band < BANDS; band++) {
      integrator.integrate(target.bandRmsScale_[band], bandRmsScale_[band]);
    }
  }
};

template <typename T, size_t GROUPS, size_t BANDS> class SpeakermanRuntimeData {
  FixedSizeArray<GroupRuntimeData<T, BANDS>, GROUPS> groupConfig_;
  T subLimiterScale_;
  T subLimiterThreshold_;
  T subRmsThreshold_;
  T subRmsScale_;
  size_t subDelay_;
  T noiseScale_;
  IntegrationCoefficients<T> controlSpeed_;
  EqualizerFilterData<T> filterConfig_;

  void compensateDelays() {
    size_t minDelay = subDelay_;
    for (size_t group = 0; group < GROUPS; group++) {
      minDelay = Values::min(minDelay, groupConfig_[group].delay());
    }
    subDelay_ -= minDelay;
    for (size_t group = 0; group < GROUPS; group++) {
      groupConfig_[group].adjustDelay(minDelay);
    }
  }

public:
  GroupRuntimeData<T, BANDS> &groupConfig(size_t i) { return groupConfig_[i]; }

  const GroupRuntimeData<T, BANDS> &groupConfig(size_t i) const {
    return groupConfig_[i];
  }

  T subLimiterScale() const { return subLimiterScale_; }

  T subLimiterThreshold() const { return subLimiterThreshold_; }

  T subRmsThreshold() const { return subRmsThreshold_; }

  T subRmsScale() const { return subRmsScale_; }

  size_t subDelay() const { return subDelay_; }

  T noiseScale() const { return noiseScale_; }

  static constexpr size_t groups() { return GROUPS; }

  static constexpr size_t bands() { return BANDS; }

  const EqualizerFilterData<T> &filterConfig() const { return filterConfig_; }

  void setFilterConfig(const EqualizerFilterData<T> &source) {
    filterConfig_ = source;
  }

  void reset() {
    subLimiterThreshold_ = 1;
    subLimiterScale_ = 1;
    subRmsThreshold_ = 1;
    subRmsScale_ = 1;
    subDelay_ = 0;
    noiseScale_ = 1e-5;
    for (size_t group = 0; group < GROUPS; group++) {
      groupConfig_[group].reset();
    }
    controlSpeed_.setCharacteristicSamples(5000);
    filterConfig_.reset();
  }

  void init(const SpeakermanRuntimeData<T, GROUPS, BANDS> &source) {
    *this = source;
    for (size_t group = 0; group < GROUPS; group++) {
      groupConfig_[group].init(source.groupConfig(group));
    }
  }

  void approach(const SpeakermanRuntimeData<T, GROUPS, BANDS> &target) {
    controlSpeed_.integrate(target.subLimiterThreshold_, subLimiterThreshold_);
    controlSpeed_.integrate(target.subLimiterScale_, subLimiterScale_);
    controlSpeed_.integrate(target.subRmsThreshold_, subRmsThreshold_);
    controlSpeed_.integrate(target.subRmsScale_, subRmsScale_);

    for (size_t group = 0; group < GROUPS; group++) {
      groupConfig_[group].approach(target.groupConfig_[group], controlSpeed_);
    }
  }

  template <typename... A>
  void configure(const SpeakermanConfig &config, double sampleRate,
                 const ArrayTraits<A...> &bandWeights,
                 double fastestPeakWeight) {
    if (config.groups != GROUPS) {
      throw std::invalid_argument("Cannot change number of groups run-time");
    }
    double subBaseThreshold = GroupConfig::MAX_THRESHOLD;
    double peakWeight = Values::force_between(fastestPeakWeight, 0.1, 1.0);
    double max_group_threshold = 0;

    for (size_t group = 0; group < config.groups; group++) {
      speakerman::GroupConfig sourceConf = config.group[group];

      GroupRuntimeData<T, BANDS> &targetConf = groupConfig_[group];
      targetConf.setFilterConfig(
          EqualizerFilterData<T>::createConfigured(sourceConf, sampleRate));

      double groupThreshold =
          Value<double>::min(sourceConf.threshold * config.threshold_scaling,
                             GroupConfig::MAX_THRESHOLD);
      max_group_threshold =
          Value<double>::max(max_group_threshold, groupThreshold);

      size_t delay =
          0.5 + sampleRate * Values::force_between(sourceConf.delay,
                                                   GroupConfig::MIN_DELAY,
                                                   GroupConfig::MAX_DELAY);
      targetConf.setLevels(sourceConf, config.threshold_scaling,
                           config.groupChannels, fastestPeakWeight, delay,
                           bandWeights);

      subBaseThreshold = Values::min(subBaseThreshold, groupThreshold);
    }
    if (config.generateNoise) {
      noiseScale_ = 20.0;
      std::cout << "Generating testing noise" << std::endl;
    } else {
      noiseScale_ = subBaseThreshold * 1e-6;
    }

    double threshold =
        Values::force_between(config.relativeSubThreshold,
                              SpeakermanConfig::MIN_REL_SUB_THRESHOLD,
                              SpeakermanConfig::MAX_REL_SUB_THRESHOLD) *
        subBaseThreshold;
    subLimiterThreshold_ =
        SpeakerManLevels::getLimiterThreshold(threshold, peakWeight);
    subLimiterScale_ = 1.0 / subLimiterThreshold_;
    subRmsThreshold_ =
        SpeakerManLevels::getRmsThreshold(threshold, bandWeights[0]);
    subRmsScale_ = 1.0 / subRmsThreshold_;
    subDelay_ =
        0.5 + sampleRate * Values::force_between(
                               config.subDelay, SpeakermanConfig::MIN_SUB_DELAY,
                               SpeakermanConfig::MAX_SUB_DELAY);
    controlSpeed_.setCharacteristicSamples(0.25 * sampleRate);
    setFilterConfig(
        EqualizerFilterData<T>::createConfigured(config, sampleRate));

    compensateDelays();
  }

  void dump() const {
    std::cout << "Runtime Configuration dump" << std::endl;
    std::cout << " sub-limiter: scale=" << subLimiterScale()
              << "; threshold_=" << subLimiterThreshold() << std::endl;
    std::cout << " sub-RMS: scale=" << subRmsScale()
              << "; threshold_=" << subRmsThreshold() << std::endl;
    std::cout << " sub-delay=" << subDelay() << std::endl;
    for (size_t group = 0; group < GROUPS; group++) {
      const GroupRuntimeData<T, bands()> &grpConfig = groupConfig(group);
      std::cout << " group " << group << std::endl;
      std::cout << "  volume="
                << "[";
      for (size_t i = 0; i < GROUPS; i++) {
        if (i > 0) {
          std::cout << " ";
        }
        std::cout << grpConfig.volume()[i];
      }
      std::cout << "]" << std::endl;
      std::cout << "  delay=" << grpConfig.delay() << std::endl;
      std::cout << "  use-sub=" << grpConfig.useSub() << std::endl;
      std::cout << "  mono=" << grpConfig.isMono() << std::endl;
      std::cout << "  equalizers=" << grpConfig.filterConfig().count()
                << std::endl;
      std::cout << "  limiter: scale=" << grpConfig.limiterScale()
                << "; threshold_=" << grpConfig.limiterThreshold() << std::endl;
      for (size_t band = 0; band < bands(); band++) {
        std::cout << "   band " << band
                  << " RMS: scale=" << grpConfig.bandRmsScale(band)
                  << std::endl;
      }
    }
  }
};

template <typename T, size_t CHANNELS_PER_GROUP> class EqualizerFilter {
  using BqFilter = BiquadFilter<T, CHANNELS_PER_GROUP>;
  BqFilter filter1;
  BqFilter filter2;
  MultiFilter<T> *filter_;

  struct SingleBiQuad : public MultiFilter<T> {
    BqFilter &f;

    SingleBiQuad(BqFilter &ref) : f(ref) {}

    virtual size_t channels() const { return CHANNELS_PER_GROUP; }

    virtual T filter(size_t channel, T input) {
      return f.filter(channel, input);
    }

    virtual void reset() { f.reset(); }

    virtual ~SingleBiQuad() = default;
  } singleBiQuad;

  struct DoubleBiQuad : public MultiFilter<T> {
    BqFilter &f1;
    BqFilter &f2;

  public:
    DoubleBiQuad(BqFilter &ref1, BqFilter &ref2) : f1(ref1), f2(ref2) {}

    virtual size_t channels() const { return CHANNELS_PER_GROUP; }

    virtual T filter(size_t channel, T input) {
      return f2.filter(channel, f1.filter(channel, input));
    }

    virtual void reset() {
      f1.reset();
      f2.reset();
    }

    virtual ~DoubleBiQuad() = default;
  } doubleBiQuad;

  static IdentityMultiFilter<T> *noFilter() {
    static IdentityMultiFilter<T> filter;
    return &filter;
  }

  template <typename S>
  MultiFilter<T> *configuredFilter(EqualizerFilterData<S> config) {
    if (config.count() == 0) {
      return noFilter();
    }
    filter1.coefficients_ = config.biquad1();
    if (config.count() == 1) {
      return &singleBiQuad;
    }
    filter2.coefficients_ = config.biquad2();
    return &doubleBiQuad;
  }

public:
  EqualizerFilter()
      : filter_(noFilter()), singleBiQuad(filter1),
        doubleBiQuad(filter1, filter2) {}

  template <typename S> void configure(EqualizerFilterData<S> config) {
    filter_ = configuredFilter(config);
  }

  MultiFilter<T> *filter() { return filter_; }
};

template <typename T, size_t GROUPS, size_t BANDS, size_t CHANNELS_PER_GROUP>
class SpeakermanRuntimeConfigurable {
  using Data = SpeakermanRuntimeData<T, GROUPS, BANDS>;

  Data active_;
  Data middle_;
  Data userSet_;

public:
  const Data &data() const { return active_; }

  const Data &userSet() const { return userSet_; }

  size_t groups() const { return GROUPS; }

  size_t channelsPerGroup() const { return CHANNELS_PER_GROUP; }

  SpeakermanRuntimeConfigurable() {
    active_.reset();
    middle_.reset();
    userSet_.reset();
  }

  void modify(const Data &source) {
    userSet_ = source;
    for (size_t group = 0; group < GROUPS; group++) {
      active_.groupConfig(group).setFilterConfig(
          source.groupConfig(group).filterConfig());
    }
  }

  void init(const Data &source) {
    userSet_ = source;
    middle_.init(userSet_);
    active_.init(middle_);
  }

  void approach() {
    middle_.approach(userSet_);
    active_.approach(middle_);
  }
};

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_SPEAKERMAN_RUNTIME_CONFIG_GUARD_H_ */
