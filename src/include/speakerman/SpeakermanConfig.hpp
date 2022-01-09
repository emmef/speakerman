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

#include <speakerman/jack/JackProcessor.hpp>
#include <speakerman/LogicalGroupConfig.h>
#include <speakerman/ProcessingGroupConfig.h>
#include <speakerman/DetectionConfig.h>
#include <speakerman/UnsetValue.h>
#include "LogicalGroupConfig.h"

namespace speakerman {


struct SpeakermanConfig {
  static constexpr size_t MIN_EQS = 0;
  static constexpr size_t DEFAULT_EQS = 0;
  static constexpr size_t MAX_EQS = 2;

  static constexpr size_t MIN_GROUPS = 1;
  static constexpr size_t DEFAULT_GROUPS = 1;

  static constexpr size_t MIN_GROUP_CHANNELS = 1;
  static constexpr size_t DEFAULT_GROUP_CHANNELS = 2;

  static constexpr double MIN_REL_SUB_THRESHOLD = 0.25;
  static constexpr double DEFAULT_REL_SUB_THRESHOLD = M_SQRT2;
  static constexpr double MAX_REL_SUB_THRESHOLD = 2.0;

  static constexpr double MIN_SUB_DELAY = ProcessingGroupConfig::MIN_DELAY;
  static constexpr double DEFAULT_SUB_DELAY =
      ProcessingGroupConfig::DEFAULT_DELAY;
  static constexpr double MAX_SUB_DELAY = ProcessingGroupConfig::MAX_DELAY;

  static constexpr size_t MIN_SUB_OUTPUT = 0;
  static constexpr size_t DEFAULT_SUB_OUTPUT = 1;
  static constexpr size_t MAX_SUB_OUTPUT = ProcessingGroupConfig::MAX_GROUPS * ProcessingGroupConfig::MAX_CHANNELS + 1;

  static constexpr size_t MIN_CROSSOVERS = 1;
  static constexpr size_t DEFAULT_CROSSOVERS = 2;
  static constexpr size_t MAX_CROSSOVERS = 3;

  static constexpr double MIN_THRESHOLD_SCALING = 1;
  static constexpr double DEFAULT_THRESHOLD_SCALING = 1;
  static constexpr double MAX_THRESHOLD_SCALING = 5;

  static constexpr int DEFAULT_GENERATE_NOISE = 0;

  size_t processingGroups = DEFAULT_GROUPS;
  size_t groupChannels = DEFAULT_GROUP_CHANNELS;
  size_t subOutput = DEFAULT_SUB_OUTPUT;
  size_t crossovers = DEFAULT_CROSSOVERS;
  double relativeSubThreshold = DEFAULT_REL_SUB_THRESHOLD;
  double subDelay = DEFAULT_SUB_DELAY;
  int generateNoise = DEFAULT_GENERATE_NOISE;
  long long timeStamp = -1;
  double threshold_scaling = DEFAULT_THRESHOLD_SCALING;
  DetectionConfig detection;
  LogicalInputsConfig logicalInputs;
  LogicalOutputsConfig logicalOutputs;
  ProcessingGroupConfig group[ProcessingGroupConfig::MAX_GROUPS];

  EqualizerConfig eq[MAX_EQS];
  size_t eqs = DEFAULT_EQS;

  static const SpeakermanConfig defaultConfig();

  static const SpeakermanConfig unsetConfig();

  void set_if_unset(const SpeakermanConfig &config_if_unset, bool initial);
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

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_SPEAKERMANCONFIG_GUARD_H_ */
