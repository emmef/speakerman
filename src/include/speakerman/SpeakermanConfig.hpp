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
#include <speakerman/jack/JackProcessor.hpp>
#include <tdap/IndexPolicy.hpp>
#include <tdap/Value.hpp>

namespace speakerman {

static constexpr size_t MAX_PHYSICAL_PORTS = 64;
static constexpr size_t MAX_LOGICAL_GROUPS = 8;
static constexpr size_t MAX_LOGICAL_CHANNELS = 16;
static constexpr size_t MAX_PROCESSING_GROUPS = 4;
static constexpr size_t MAX_NAME_LENGTH = 32;

template <typename T> struct UnsetValue {};

template <> struct UnsetValue<size_t> {
  static constexpr size_t value = static_cast<size_t>(-1);

  static bool is(size_t test) { return test == value; }

  static void set(size_t &destination) { destination = value; }
};

template <> struct UnsetValue<char *> {
  static constexpr char *value = {0};

  static bool is(const char *test) { return !test || !*test; }

  static void set(char *destination) {
    if (destination) {
      *destination = 0;
    }
  }
};

template <> struct UnsetValue<char[MAX_NAME_LENGTH + 1]> {
  static constexpr char *value = {0};

  static bool is(const char *test) { return !test || !*test; }

  static void set(char *destination) {
    if (destination) {
      *destination = 0;
    }
  }
};

template <> struct UnsetValue<double> {
  static constexpr double value = std::numeric_limits<double>::quiet_NaN();

  union Tester {
    long long l;
    double f;

    Tester(double v) : l(0) { f = v; }

    bool operator==(const Tester &other) const { return l == other.l; }
  };

  static bool is(double test) {
    static const Tester sNan = {std::numeric_limits<double>::signaling_NaN()};
    static const Tester qNan = {value};
    Tester t{test};
    return t == sNan || t == qNan;
  }

  static void set(double &destination) { destination = value; }
};

template <> struct UnsetValue<int> {
  static constexpr int value = -1;

  static bool is(size_t test) { return test == value; }

  static void set(int &destination) { destination = value; }
};

template <typename T> static void unsetConfigValue(T &target) {
  UnsetValue<T>::set(target);
}

template <typename T> static bool setConfigValueIfUnset(T &target, T copyFrom) {
  if (UnsetValue<T>::is(target)) {
    target = copyFrom;
    return true;
  }
  return false;
}

template <typename T> static bool isUnsetConfigValue(const T &value) {
  return UnsetValue<T>::is(value);
}

struct NamedConfig {
  static constexpr const char *KEY_SNIPPET_NAME = "name";
  static constexpr size_t NAME_LENGTH = MAX_NAME_LENGTH;
  char name[NAME_LENGTH + 1] = "[No name]";

  void unset(NamedConfig &config) { UnsetValue<char *>::set(name); }
};

struct EqualizerConfig {
  static constexpr double MIN_CENTER_FREQ = 20;
  static constexpr double DEFAULT_CENTER_FREQ = 1000;
  static constexpr double MAX_CENTER_FREQ = 22000;

  static constexpr double MIN_GAIN = 0.10;
  static constexpr double DEFAULT_GAIN = 1.0;
  static constexpr double MAX_GAIN = 10.0;

  static constexpr double MIN_BANDWIDTH = 0.25;
  static constexpr double DEFAULT_BANDWIDTH = 1;
  static constexpr double MAX_BANDWIDTH = 8.0;

  static constexpr const char *KEY_SNIPPET_EQUALIZER = "equalizer";
  static constexpr const char *KEY_SNIPPET_CENTER = "center";
  static constexpr const char *KEY_SNIPPET_GAIN = "gain";
  static constexpr const char *KEY_SNIPPET_BANDWIDTH = "bandwidth";

  static const EqualizerConfig defaultConfig() { return {}; }

  static const EqualizerConfig unsetConfig();
  void set_if_unset(const EqualizerConfig &base_config_if_unset);

  double center = DEFAULT_CENTER_FREQ;
  double gain = DEFAULT_GAIN;
  double bandwidth = DEFAULT_BANDWIDTH;
};
struct LogicalGroupConfig : public NamedConfig {
  static_assert(MAX_LOGICAL_GROUPS > 1);

  static constexpr const char *KEY_SNIPPET_INPUT = "logical-input";
  static constexpr const char *KEY_SNIPPET_OUTPUT = "logical-output";
  static constexpr int DEFAULT_IS_MUTED = 0;
  static constexpr const char *KEY_SNIPPET_MUTED = "mute";
  int muted = UnsetValue<int>::value;

  static constexpr double MIN_VOLUME = 0.0;
  static constexpr double DEFAULT_VOLUME = 1.0;
  static constexpr double MAX_VOLUME = 1.0;
  static constexpr const char *KEY_SNIPPET_VOLUME = "volume";
  double volume = UnsetValue<double>::value;

  static constexpr size_t MIN_PORT_NUMBER = 0;
  static constexpr size_t MAX_PORT_NUMBER = 0xfffflu;
  static constexpr const char *KEY_PORT_NUMBER = "port-numbers";
  size_t ports[MAX_LOGICAL_CHANNELS];

  static const LogicalGroupConfig defaultConfig() {
    static LogicalGroupConfig config;
    return config;
  }

  LogicalGroupConfig();

  static const LogicalGroupConfig unsetConfig();;

  void set_if_unset(const LogicalGroupConfig &config_if_unset);

  void sanitize(size_t groupNumber, const char *typeOfGroup);
  static void sanitize(LogicalGroupConfig *pConfig, const size_t groups,
                       const char *typeOfGroup);
  size_t getPortCount();
};

struct ProcessingGroupConfig : public NamedConfig {
  static constexpr size_t MIN_EQS = 0;
  static constexpr size_t DEFAULT_EQS = 0;
  static constexpr size_t MAX_EQS = 2;
  static constexpr const char *KEY_SNIPPET_EQ_COUNT = "equalizers";

  static constexpr double MIN_THRESHOLD = 0.001;
  static constexpr double DEFAULT_THRESHOLD = 0.1;
  static constexpr double MAX_THRESHOLD = 0.9;
  static constexpr const char *KEY_SNIPPET_THRESHOLD = "threshold";

  static constexpr double MIN_VOLUME = 0;
  static constexpr double DEFAULT_VOLUME = 1.0;
  static constexpr double MAX_VOLUME = 40.0;
  static constexpr const char *KEY_SNIPPET_VOLUME = "volume";

  static constexpr double MIN_DELAY = 0;
  static constexpr double DEFAULT_DELAY = 0;
  static constexpr double MAX_DELAY = 0.020;
  static constexpr const char *KEY_SNIPPET_DELAY = "delay";

  static constexpr int DEFAULT_USE_SUB = 1;
  static constexpr const char *KEY_SNIPPET_USE_SUB = "use-sub";

  static constexpr int DEFAULT_MONO = 0;
  static constexpr const char *KEY_SNIPPET_MONO = "mono";

  static constexpr const char *KEY_SNIPPET_GROUP = "group";

  double threshold = DEFAULT_THRESHOLD;
  double volume[MAX_LOGICAL_CHANNELS];
  double delay = DEFAULT_DELAY;
  int use_sub = DEFAULT_USE_SUB;
  int mono = DEFAULT_MONO;
  EqualizerConfig eq[MAX_EQS];
  size_t eqs = DEFAULT_EQS;

  static const ProcessingGroupConfig defaultConfig(size_t group_id);

  static const ProcessingGroupConfig unsetConfig();

  void set_if_unset(const ProcessingGroupConfig &config_if_unset);

  const ProcessingGroupConfig with_groups_separated(size_t group_id) const;

  const ProcessingGroupConfig with_groups_mixed() const;
};

struct DetectionConfig {
  static constexpr double MIN_MAXIMUM_WINDOW_SECONDS = 0.4;
  static constexpr double DEFAULT_MAXIMUM_WINDOW_SECONDS = 0.4;
  static constexpr double MAX_MAXIMUM_WINDOW_SECONDS = 8.0;
  static constexpr const char *KEY_SNIPPET_MAXIMUM_WINDOW_SECONDS =
      "detection.slow-seconds";

  static constexpr size_t MIN_PERCEPTIVE_LEVELS = 2;
  static constexpr size_t DEFAULT_PERCEPTIVE_LEVELS = 11;
  static constexpr size_t MAX_PERCEPTIVE_LEVELS = 32;
  static constexpr const char *KEY_SNIPPET_PERCEPTIVE_LEVELS =
      "detection.time-constants";

  static constexpr double MIN_MINIMUM_WINDOW_SECONDS = 0.0001;
  static constexpr double DEFAULT_MINIMUM_WINDOW_SECONDS = 0.001;
  static constexpr double MAX_MINIMUM_WINDOW_SECONDS = 0.010;
  static constexpr const char *KEY_SNIPPET_MINIMUM_WINDOW_SECONDS =
      "detection.fast-seconds";

  static constexpr double MIN_RMS_FAST_RELEASE_SECONDS = 0.001;
  static constexpr double DEFAULT_RMS_FAST_RELEASE_SECONDS = 0.02;
  static constexpr double MAX_RMS_FAST_RELEASE_SECONDS = 0.10;
  static constexpr const char *KEY_SNIPPET_RMS_FAST_RELEASE_SECONDS =
      "detection.rms-fast-release-seconds";

  static constexpr int DEFAULT_USE_BRICK_WALL_PREDICTION = 1;
  static constexpr const char *KEY_SNIPPET_USE_BRICK_WALL_PREDICTION =
      "detection.use-brick-wall-prediction";

  double maximum_window_seconds = DEFAULT_MAXIMUM_WINDOW_SECONDS;
  double minimum_window_seconds = DEFAULT_MINIMUM_WINDOW_SECONDS;
  double rms_fast_release_seconds = DEFAULT_RMS_FAST_RELEASE_SECONDS;
  size_t perceptive_levels = DEFAULT_PERCEPTIVE_LEVELS;
  int useBrickWallPrediction = DEFAULT_USE_BRICK_WALL_PREDICTION;

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

  static constexpr size_t MIN_GROUP_CHANNELS = 1;
  static constexpr size_t DEFAULT_GROUP_CHANNELS = 2;
  static constexpr size_t MAX_GROUP_CHANNELS = 5;

  static constexpr double MIN_REL_SUB_THRESHOLD = 0.25;
  static constexpr double DEFAULT_REL_SUB_THRESHOLD = M_SQRT2;
  static constexpr double MAX_REL_SUB_THRESHOLD = 2.0;

  static constexpr double MIN_SUB_DELAY = ProcessingGroupConfig::MIN_DELAY;
  static constexpr double DEFAULT_SUB_DELAY =
      ProcessingGroupConfig::DEFAULT_DELAY;
  static constexpr double MAX_SUB_DELAY = ProcessingGroupConfig::MAX_DELAY;

  static constexpr size_t MIN_SUB_OUTPUT = 0;
  static constexpr size_t DEFAULT_SUB_OUTPUT = 1;
  static constexpr size_t MAX_SUB_OUTPUT = MAX_PROCESSING_GROUPS * MAX_GROUP_CHANNELS + 1;

  static constexpr size_t MIN_CROSSOVERS = 1;
  static constexpr size_t DEFAULT_CROSSOVERS = 2;
  static constexpr size_t MAX_CROSSOVERS = 3;

  static constexpr size_t MIN_INPUT_OFFSET = 0;
  static constexpr size_t DEFAULT_INPUT_OFFSET = 0;
  static constexpr size_t MAX_INPUT_OFFSET = MAX_PROCESSING_GROUPS * MAX_GROUP_CHANNELS;

  static constexpr size_t MIN_INPUT_COUNT = 1;
  static constexpr size_t DEFAULT_INPUT_COUNT = -1;
  static constexpr size_t MAX_INPUT_COUNT = MAX_PROCESSING_GROUPS * MAX_GROUP_CHANNELS;

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
  LogicalGroupConfig logicalInputs[MAX_LOGICAL_GROUPS];
  LogicalGroupConfig logicalOutputs[MAX_LOGICAL_GROUPS];
  ProcessingGroupConfig group[MAX_PROCESSING_GROUPS];
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
  double signal_square_[MAX_PROCESSING_GROUPS + 1];
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

  virtual const ProcessingStatistics getStatistics() const = 0;
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
