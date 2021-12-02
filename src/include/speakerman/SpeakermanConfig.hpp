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

#ifndef SMS_SPEAKERMAN_SPEAKERMANCONFIG_GUARD_H_
#define SMS_SPEAKERMAN_SPEAKERMANCONFIG_GUARD_H_

#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <speakerman/utils/Config.hpp>
#include <tdap/IndexPolicy.hpp>
#include <tdap/Value.hpp>

namespace speakerman {

static constexpr size_t MAX_SPEAKERMAN_GROUPS = 4;
static constexpr size_t NAME_LENGTH = 32;
typedef ConfigStringDefinition<ConfigStringFormatDefinition<NAME_LENGTH>>
    NameDefinition;
typedef ConfigString<NameDefinition> NameVariable;
static constexpr NameDefinition nameDefinition("name", "noname");

static constexpr double MAXIMUM_DELAY_SECONDS = 0.02;

struct EqualizerConfig {
  struct Definitions {
    static constexpr ConfigNumericDefinition<double> center{20, 1000, 22000,
                                                            "center"};
    static constexpr ConfigNumericDefinition<double> gain{0.1, 1, 10, "gain"};
    static constexpr ConfigNumericDefinition<double> bandwidth{0.125, 1, 8,
                                                               "bandwidth"};
  };

  ConfigNumeric<double> center;
  ConfigNumeric<double> gain;
  ConfigNumeric<double> bandwidth;

  EqualizerConfig()
      : center(Definitions::center), gain(Definitions::gain),
        bandwidth(Definitions::bandwidth) {}

  EqualizerConfig &operator=(const EqualizerConfig &source) {
    center = source.center;
    gain = source.gain;
    bandwidth = source.bandwidth;
    return *this;
  }

  static constexpr const char *KEY_SNIPPET_EQUALIZER = "equalizer";

  static const EqualizerConfig unsetConfig() { return {}; }

  void set_if_unset(const EqualizerConfig &base_config_if_unset) {
    if (center.is_set() || !base_config_if_unset.center.is_set()) {
      return;
    }
    this->operator=(base_config_if_unset);
  }
};

typedef struct GroupConfig {
  static constexpr size_t MAX_EQS = 2;
  struct Definitions {
    static constexpr ConfigNumericDefinition<size_t> equalizers{0, 0, MAX_EQS,
                                                                "equalizers"};
    static constexpr ConfigNumericDefinition<double> threshold{0.001, 0.1, 0.99,
                                                               "threshold"};
    static constexpr ConfigNumericDefinition<double> volume{0, 1.0, 40,
                                                            "volume"};
    static constexpr ConfigNumericDefinition<double> delay{
        0, 0, MAXIMUM_DELAY_SECONDS, "delay"};
    static constexpr ConfigNumericDefinition<int> useSub{0, 1, 1, "use-sub"};
    static constexpr ConfigNumericDefinition<int> mono{0, 0, 1, "mono"};
  };

  static constexpr const char *KEY_SNIPPET_GROUP = "group";
  static constexpr const char *KEY_SNIPPET_NAME = "name";
  static constexpr size_t NAME_LENGTH = 32;

  ConfigNumeric<double> threshold;
  ConfigNumericArray<double, MAX_SPEAKERMAN_GROUPS> volume;
  ConfigNumeric<double> delay;
  ConfigNumeric<int> use_sub;
  ConfigNumeric<int> mono;
  ConfigNumeric<size_t> eqs;
  EqualizerConfig eq[MAX_EQS];
  NameVariable name;

  GroupConfig()
      : threshold(Definitions::threshold), volume(Definitions::volume),
        delay(Definitions::delay), use_sub(Definitions::useSub),
        mono(Definitions::mono), eqs(Definitions::equalizers),
        name(nameDefinition) {}

  static const GroupConfig defaultConfig(size_t group_id);

  static const GroupConfig unsetConfig();

  void set_if_unset(const GroupConfig &config_if_unset);

  const GroupConfig with_groups_separated(size_t group_id) const;

  const GroupConfig with_groups_mixed() const;
};

struct DetectionConfig {
  struct Definitions {
    static constexpr ConfigNumericDefinition<double> slowSeconds{
        0.4, 0.4, 8.0, "detection.slow-seconds"};
    static constexpr ConfigNumericDefinition<double> fastSeconds{
        0.0001, 0.001, 0.01, "detection.fast-seconds",
        InvalidValuePolicy::Fit};
    static constexpr ConfigNumericDefinition<double> fastReleaseSeconds{
        0.001, 0.02, 0.1, "detection.fast-release-seconds",
        InvalidValuePolicy::Fit};
    static constexpr ConfigNumericDefinition<size_t> perceptiveLevels{
        2, 11, 32, "detection.time-constants"};
    static constexpr ConfigNumericDefinition<int> brickWallLimiterType{
        0, 1, 2, "detection.brick-wall-limiter-type"};
  };
  ConfigNumeric<double> maximum_window_seconds;
  ConfigNumeric<double> minimum_window_seconds;
  ConfigNumeric<double> rms_fast_release_seconds;
  ConfigNumeric<size_t> perceptive_levels;
  ConfigNumeric<int> brickWallLimiterType;

  DetectionConfig()
      : maximum_window_seconds(Definitions::slowSeconds),
        minimum_window_seconds(Definitions::fastSeconds),
        rms_fast_release_seconds(Definitions::fastReleaseSeconds),
        perceptive_levels(Definitions::perceptiveLevels),
        brickWallLimiterType(Definitions::brickWallLimiterType) {}

  static const DetectionConfig defaultConfig() { return {}; }

  static const DetectionConfig unsetConfig();

  void set_if_unset(const DetectionConfig &config_if_unset);
};

struct SpeakermanConfig {
  static constexpr size_t MIN_EQS = 0;
  static constexpr size_t DEFAULT_EQS = 0;
  static constexpr size_t MAX_EQS = 2;
  static constexpr const char *KEY_SNIPPET_EQ_COUNT = "equalizers";

  static constexpr size_t MIN_GROUPS = 1;
  static constexpr size_t DEFAULT_GROUPS = 1;
  static constexpr size_t MAX_GROUPS = MAX_SPEAKERMAN_GROUPS;

  static constexpr size_t MIN_GROUP_CHANNELS = 1;
  static constexpr size_t DEFAULT_GROUP_CHANNELS = 2;
  static constexpr size_t MAX_GROUP_CHANNELS = 5;

  static constexpr double MIN_REL_SUB_THRESHOLD = 0.25;
  static constexpr double DEFAULT_REL_SUB_THRESHOLD = M_SQRT2;
  static constexpr double MAX_REL_SUB_THRESHOLD = 2.0;

  static constexpr double MIN_SUB_DELAY = 0;
  static constexpr double DEFAULT_SUB_DELAY = 0;
  static constexpr double MAX_SUB_DELAY = MAXIMUM_DELAY_SECONDS;

  static constexpr size_t MIN_SUB_OUTPUT = 0;
  static constexpr size_t DEFAULT_SUB_OUTPUT = 1;
  static constexpr size_t MAX_SUB_OUTPUT = MAX_GROUPS * MAX_GROUP_CHANNELS + 1;

  static constexpr size_t MIN_CROSSOVERS = 1;
  static constexpr size_t DEFAULT_CROSSOVERS = 2;
  static constexpr size_t MAX_CROSSOVERS = 3;

  static constexpr size_t MIN_INPUT_OFFSET = 0;
  static constexpr size_t DEFAULT_INPUT_OFFSET = 0;
  static constexpr size_t MAX_INPUT_OFFSET = MAX_GROUPS * MAX_GROUP_CHANNELS;

  static constexpr size_t MIN_INPUT_COUNT = 1;
  static constexpr size_t DEFAULT_INPUT_COUNT = -1;
  static constexpr size_t MAX_INPUT_COUNT = MAX_GROUPS * MAX_GROUP_CHANNELS;

  static constexpr double MIN_THRESHOLD_SCALING = 1;
  static constexpr double DEFAULT_THRESHOLD_SCALING = 1;
  static constexpr double MAX_THRESHOLD_SCALING = 5;

  static constexpr int DEFAULT_GENERATE_NOISE = 0;

  static constexpr const char *KEY_SNIPPET_GROUP_COUNT = "groups";
  static constexpr const char *KEY_SNIPPET_CHANNELS = "group-channels";
  static constexpr const char *KEY_SNIPPET_SUB_THRESHOLD =
      "sub-relative-threshold";
  static constexpr const char *KEY_SNIPPET_SUB_DELAY = "sub-delay";
  static constexpr const char *KEY_SNIPPET_SUB_OUTPUT = "sub-output";
  static constexpr const char *KEY_SNIPPET_CROSSOVERS = "crossovers";
  static constexpr const char *KEY_SNIPPET_INPUT_OFFSET = "input-offset";
  static constexpr const char *KEY_SNIPPET_INPUT_COUNT = "input-count";
  static constexpr const char *KEY_SNIPPET_GENERATE_NOISE = "generate-noise";

  size_t groups = DEFAULT_GROUPS;
  size_t groupChannels = DEFAULT_GROUP_CHANNELS;
  size_t subOutput = DEFAULT_SUB_OUTPUT;
  size_t crossovers = DEFAULT_CROSSOVERS;
  size_t inputOffset = DEFAULT_INPUT_OFFSET;
  size_t inputCount = DEFAULT_INPUT_COUNT;
  double relativeSubThreshold = DEFAULT_REL_SUB_THRESHOLD;
  double subDelay = DEFAULT_SUB_DELAY;
  int generateNoise = DEFAULT_GENERATE_NOISE;
  long long timeStamp = -1;
  double threshold_scaling = DEFAULT_THRESHOLD_SCALING;
  DetectionConfig detection;
  GroupConfig group[MAX_GROUPS];
  EqualizerConfig eq[MAX_EQS];
  size_t eqs = DEFAULT_EQS;

  static const SpeakermanConfig defaultConfig();

  static const SpeakermanConfig unsetConfig();

  void set_if_unset(const SpeakermanConfig &config_if_unset);

  const SpeakermanConfig with_groups_mixed() const;

  const SpeakermanConfig with_groups_separated() const;

  const SpeakermanConfig with_groups_first() const;
};

using tdap::IndexPolicy;
using tdap::Values;

class DynamicProcessorLevels {
  double signal_square_[SpeakermanConfig::MAX_GROUPS + 1];
  size_t channels_;
  size_t count_;

  void addGainAndSquareSignal(size_t group, double signal) {
    size_t i = IndexPolicy::array(group, channels_);
    signal_square_[i] = Values::max(signal_square_[i], signal);
  }

public:
  DynamicProcessorLevels() : channels_(0), count_(0){};

  DynamicProcessorLevels(size_t groups, size_t crossovers)
      : channels_(groups + 1), count_(0) {}

  size_t groups() const { return channels_ - 1; }

  size_t count() const { return count_; }

  void operator+=(const DynamicProcessorLevels &levels) {
    size_t count = Values::min(channels_, levels.channels_);
    for (size_t i = 0; i < count; i++) {
      signal_square_[i] =
          Values::max(signal_square_[i], levels.signal_square_[i]);
    }
    count_ += levels.count_;
  }

  void next() { count_++; }

  void reset() {
    for (size_t limiter = 0; limiter < channels_; limiter++) {
      signal_square_[limiter] = 0.0;
    }
    count_ = 0;
  }

  void addValues(size_t group, double signal) {
    addGainAndSquareSignal(group, signal);
  }

  double getSignal(size_t group) const {
    return sqrt(signal_square_[IndexPolicy::array(group, channels_)]);
  }
};

class SpeakerManagerControl {
public:
  enum class MixMode { AS_CONFIGURED, ALL, OWN, FIRST };

  virtual const SpeakermanConfig &getConfig() const = 0;

  virtual bool
  applyConfigAndGetLevels(const SpeakermanConfig &config,
                          DynamicProcessorLevels *levels,
                          std::chrono::milliseconds timeoutMillis) = 0;

  virtual bool getLevels(DynamicProcessorLevels *levels,
                         std::chrono::milliseconds timeoutMillis) = 0;

  virtual ~SpeakerManagerControl() = default;
};

const char *getInstallBaseDirectory();

const char *getWebSiteDirectory();

const char *configFileName();

const char *webDirectory();

const char *getWatchDogScript();

SpeakermanConfig readSpeakermanConfig();

SpeakermanConfig readSpeakermanConfig(const SpeakermanConfig &basedUpon,
                                      bool initial);

void dumpSpeakermanConfig(const SpeakermanConfig &dump, std::ostream &output);

long long getFileTimeStamp(const char *fileName);

long long getConfigFileTimeStamp();

class StreamOwner {
  std::ifstream &stream_;
  bool owns_;

  void operator=(const StreamOwner &source) {}

  void operator=(StreamOwner &&source) noexcept {}

public:
  explicit StreamOwner(std::ifstream &owned);
  StreamOwner(const StreamOwner &source);
  StreamOwner(StreamOwner &&source) noexcept;
  static StreamOwner open(const char *file_name);
  bool is_open() const;
  std::ifstream &stream() const { return stream_; };
  ~StreamOwner();
};

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_SPEAKERMANCONFIG_GUARD_H_ */
