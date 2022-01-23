#ifndef SPEAKERMAN_M_LOGICAL_GROUP_CONFIG_H
#define SPEAKERMAN_M_LOGICAL_GROUP_CONFIG_H
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

#include <memory>
#include <speakerman/ConfigStage.h>
#include <speakerman/NamedConfig.h>
#include <speakerman/UnsetValue.h>

namespace speakerman {

struct LogicalGroupConfig : public NamedConfig {
  static constexpr size_t MAX_CHANNELS = 8;
  static constexpr size_t DEFAULT_CHANNELS = 2;

  static_assert(MAX_CHANNELS >= 1);
  static_assert(DEFAULT_CHANNELS >= 1 && DEFAULT_CHANNELS <= MAX_CHANNELS);

  enum class Direction { Input, Output };

  static constexpr double MIN_VOLUME = 0.0;
  static constexpr double DEFAULT_VOLUME = 1.0;
  static constexpr double MAX_VOLUME = 40.0;
  double volume = UnsetValue<double>::value;

  /**
   * Port numbers are 1-based
   */
  static constexpr size_t MIN_PORT_NUMBER = 1;
  static constexpr size_t MAX_PORT_NUMBER = 0xfffflu;
  size_t ports[MAX_CHANNELS];

  static constexpr bool isValidPortNumber(size_t number) {
    return number >= MIN_PORT_NUMBER && number < MAX_PORT_NUMBER;
  }

  static const LogicalGroupConfig &defaultConfig();

  static const LogicalGroupConfig &unsetConfig();

  LogicalGroupConfig();
  void setNoPorts();
  size_t compactPorts();

  void replaceWithDefaultsIfUnset(Direction direction, size_t groupNumber);
  void changeRuntimeValues(const LogicalGroupConfig &newRuntimeConfig);

  void sanitize(Direction direction, size_t groupNumber);
  size_t getPortCount() const;

  void setDefaultNumberedName(size_t number, Direction direction);
};

struct LogicalPortMapEntry {
  // 1-based port number, defaults to invalid value.
  size_t port = 0;
  // Linked logical group, 0-based, defaults to invalid value.
  size_t logicalGroup = UnsetValue<size_t>::value;
  // Linked channel in logical group, 0-based, defaults to invalid value.
  size_t groupChannel = UnsetValue<size_t>::value;
  // 0-based channel of all logical groups together, defaults to invalid value.
  size_t channel = UnsetValue<size_t>::value;

  size_t wrappedPort(size_t maximumPort) const;
};

class LogicalPortMap {
  LogicalPortMapEntry entries[LogicalGroupConfig::MAX_CHANNELS];
  size_t count = 0;

public:
  bool add(const LogicalPortMapEntry &entry);
  size_t getCount() const;
  LogicalPortMapEntry &operator[](size_t i);
  const LogicalPortMapEntry &operator[](size_t i) const;
  const LogicalPortMapEntry *begin() const;
  const LogicalPortMapEntry *end() const;
};

struct AbstractLogicalGroupsConfig {
  static constexpr size_t MAX_GROUPS = 8;

  static_assert(MAX_GROUPS >= 1);
  static_assert(MAX_GROUPS <= LogicalGroupConfig::MAX_CHANNELS);

  LogicalGroupConfig group[MAX_GROUPS];

  size_t getGroupCount() const;
  size_t getTotalChannels() const;

  const LogicalPortMap createMapping() const;

  double volumeForChannel(size_t channel) const;

protected:
  size_t compactGroups(LogicalGroupConfig::Direction direction);
  size_t validateGroups(LogicalGroupConfig::Direction direction);

  static const AbstractLogicalGroupsConfig &
  defaultConfig(LogicalGroupConfig::Direction direction);

  static const AbstractLogicalGroupsConfig &unsetConfig();

  void sanitizeInitial(LogicalGroupConfig::Direction direction);

  void changeRuntimeValues(const AbstractLogicalGroupsConfig &runtimeValues,
                           LogicalGroupConfig::Direction);
};

template <LogicalGroupConfig::Direction D>
struct LogicalGroupsConfig : public AbstractLogicalGroupsConfig {
public:
  LogicalGroupsConfig(const AbstractLogicalGroupsConfig &source)
      : AbstractLogicalGroupsConfig(source) {}
  LogicalGroupsConfig()
      : AbstractLogicalGroupsConfig(
            AbstractLogicalGroupsConfig::unsetConfig()) {}

  LogicalGroupConfig::Direction getDirection() { return D; }

  size_t getGroupCount() const {
    return AbstractLogicalGroupsConfig::getGroupCount();
  }
  size_t compactGroups() {
    return AbstractLogicalGroupsConfig::compactGroups(D);
  }
  size_t validateGroups() {
    return AbstractLogicalGroupsConfig::validateGroups(D);
  }

  static const LogicalGroupsConfig &defaultConfig() {
    static LogicalGroupsConfig config =
        AbstractLogicalGroupsConfig::defaultConfig(D);
    return config;
  }

  static const LogicalGroupsConfig &unsetConfig() {
    static LogicalGroupsConfig config =
        AbstractLogicalGroupsConfig::unsetConfig();
    return config;
  }

  void sanitizeInitial() { AbstractLogicalGroupsConfig::sanitizeInitial(D); }

  void changeRuntimeValues(const LogicalGroupsConfig &runtimeValues) {
    AbstractLogicalGroupsConfig::changeRuntimeValues(runtimeValues, D);
  }
};

typedef LogicalGroupsConfig<LogicalGroupConfig::Direction::Input>
    LogicalInputsConfig;
typedef LogicalGroupsConfig<LogicalGroupConfig::Direction::Output>
    LogicalOutputsConfig;

} // namespace speakerman

#endif // SPEAKERMAN_M_LOGICAL_GROUP_CONFIG_H
