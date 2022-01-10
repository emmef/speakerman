#ifndef SPEAKERMAN_PROCESSINGGROUPCONFIG_H
#define SPEAKERMAN_PROCESSINGGROUPCONFIG_H
/*
 * speakerman/ProcessingGroupConfig.h
 *
 * Added by michel on 2022-01-09
 * Copyright (C) 2015-2022 Michel Fleur.
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

#include <speakerman/EqualizerConfig.h>
#include <speakerman/LogicalGroupConfig.h>
#include <speakerman/NamedConfig.h>

namespace speakerman {

struct ProcessingGroupConfig : public NamedConfig {
  static constexpr size_t MAX_CHANNELS = 16;

  static constexpr size_t MIN_EQS = 0;
  static constexpr size_t DEFAULT_EQS = 0;
  static constexpr size_t MAX_EQS = 2;

  static constexpr double MIN_THRESHOLD = 0.001;
  static constexpr double DEFAULT_THRESHOLD = 0.1;
  static constexpr double MAX_THRESHOLD = 0.9;

  static constexpr double MIN_VOLUME = 0;
  static constexpr double DEFAULT_VOLUME = 1.0;
  static constexpr double MAX_VOLUME = 40.0;

  static constexpr double MIN_DELAY = 0;
  static constexpr double DEFAULT_DELAY = 0;
  static constexpr double MAX_DELAY = 0.020;

  static constexpr int DEFAULT_USE_SUB = 1;

  static constexpr int DEFAULT_MONO = 0;

  double threshold = DEFAULT_THRESHOLD;
  double volume[LogicalGroupConfig::MAX_CHANNELS];
  double delay = DEFAULT_DELAY;
  int use_sub = DEFAULT_USE_SUB;
  int mono = DEFAULT_MONO;
  EqualizerConfig eq[MAX_EQS];
  size_t eqs = DEFAULT_EQS;

  static const ProcessingGroupConfig defaultConfig(size_t group_id);

  static const ProcessingGroupConfig unsetConfig();

  void set_if_unset(const ProcessingGroupConfig &config_if_unset);

  void setDefaultNumberedName(size_t i);
};

struct ProcessingGrouspConfig {
  static constexpr size_t MAX_GROUPS = 4;

};

} // namespace speakerman

#endif // SPEAKERMAN_PROCESSINGGROUPCONFIG_H
