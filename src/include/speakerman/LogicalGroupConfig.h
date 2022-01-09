#ifndef SPEAKERMAN_LOGICALGROUPCONFIG_H
#define SPEAKERMAN_LOGICALGROUPCONFIG_H
/*
 * speakerman/LogicalGroupConfig.h
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

#include <speakerman/NamedConfig.h>
#include <speakerman/UnsetValue.h>

namespace speakerman {

struct LogicalGroupConfig : public NamedConfig {
  static constexpr size_t MAX_GROUPS = 8;
  static constexpr size_t MAX_CHANNELS = 16;

  static_assert(MAX_GROUPS > 1);
  static_assert(MAX_CHANNELS >= MAX_GROUPS);

  static constexpr double MIN_VOLUME = 0.0;
  static constexpr double DEFAULT_VOLUME = 1.0;
  static constexpr double MAX_VOLUME = 1.0;
  double volume = UnsetValue<double>::value;

  static constexpr size_t MIN_PORT_NUMBER = 0;
  static constexpr size_t MAX_PORT_NUMBER = 0xfffflu;
  size_t ports[MAX_CHANNELS];

  static const LogicalGroupConfig defaultConfig() {
    static LogicalGroupConfig config;
    return config;
  }

  LogicalGroupConfig();

  static const LogicalGroupConfig unsetConfig();

  void set_if_unset(const LogicalGroupConfig &config_if_unset);

  void sanitize(size_t groupNumber, const char *typeOfGroup);
  static void sanitize(LogicalGroupConfig *pConfig, const size_t groups,
                       const char *typeOfGroup);
  size_t getPortCount();

  void setDefaultNumberedName(size_t number, const char *typeOfNumber) {
    printToName("Logical %s group %zu", typeOfNumber, number);
  }

};


} // namespace speakerman

#endif // SPEAKERMAN_LOGICALGROUPCONFIG_H
