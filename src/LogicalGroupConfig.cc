//
// Created by michel on 09-01-22.
//

#include <algorithm>
#include <array>
#include <iostream>
#include <speakerman/LogicalGroupConfig.h>
#include <tdap/IndexPolicy.hpp>

namespace speakerman {

static inline LogicalGroupConfig createUnsetConfig() {
  LogicalGroupConfig result;
  unsetConfigValue(result.name);
  unsetConfigValue(result.volume);
  return result;
}

const LogicalGroupConfig &LogicalGroupConfig::defaultConfig() {
  return unsetConfig();
}

const LogicalGroupConfig &LogicalGroupConfig::unsetConfig() {
  static LogicalGroupConfig result = createUnsetConfig();
  return result;
}

LogicalGroupConfig::LogicalGroupConfig() { setNoPorts(); }

void LogicalGroupConfig::setNoPorts() {
  std::fill(ports, ports + LogicalGroupConfig::MAX_CHANNELS,
            UnsetValue<size_t>::value);
}

size_t LogicalGroupConfig::compactPorts() {
  size_t portCount = 0;
  for (size_t i = 0; i < MAX_CHANNELS; i++) {
    if (isValidPortNumber(ports[i])) {
      ports[portCount++] = ports[i];
    } else {
      ports[i] = UnsetValue<size_t>::value;
    }
  }
  return portCount;
}

size_t LogicalGroupConfig::getPortCount() const {
  size_t count = 0;
  for (const size_t port : ports) {
    if (isValidPortNumber(port)) {
      count++;
    }
  }
  return count;
}

void LogicalGroupConfig::replaceWithDefaultsIfUnset(Direction direction,
                                                    size_t groupNumber) {
  setConfigValueIfUnset(volume,
                        std::clamp(DEFAULT_VOLUME, MIN_VOLUME, MAX_VOLUME));
  if (isUnsetConfigValue(name)) {
    setDefaultNumberedName(groupNumber, direction);
  }
  if (compactPorts() == 0) {
    setNoPorts();
    for (size_t i = 0, offs = DEFAULT_CHANNELS * groupNumber + 1;
         i < DEFAULT_CHANNELS; i++, offs++) {
      ports[i] = offs;
    }
  }
}

void LogicalGroupConfig::changeRuntimeValues(
    const LogicalGroupConfig &newRuntimeConfig) {
  if (!isUnsetConfigValue(newRuntimeConfig.volume)) {
    volume = std::clamp(newRuntimeConfig.volume, MIN_VOLUME, MAX_VOLUME);
  }
  if (!isUnsetConfigValue(newRuntimeConfig.name)) {
    copyToName(newRuntimeConfig.name);
  }
}

void LogicalGroupConfig::sanitize(Direction direction, size_t groupNumber) {
  if (getPortCount()) {
    replaceWithDefaultsIfUnset(direction, groupNumber);
  } else {
    *this = unsetConfig();
  }
}

static std::ostream &
operator<<(std::ostream &out, const LogicalGroupConfig::Direction &direction) {
  switch (direction) {
  case LogicalGroupConfig::Direction::Input:
    out << "input";
    break;
  case LogicalGroupConfig::Direction::Output:
    out << "output";
    break;
  default:
    out << "[unknown]]";
    break;
  }
  return out;
}

void LogicalGroupConfig::setDefaultNumberedName(size_t number,
                                                Direction direction) {
  printToName("Logical %s group %zu",
              direction == Direction::Input ? "input" : "output", number);
}

size_t AbstractLogicalGroupsConfig::getGroupCount() const {
  size_t count = 0;
  for (size_t i = 0; i < MAX_GROUPS; i++) {
    size_t ports = group[i].getPortCount();
    if (ports) {
      count++;
    } else {
      break;
    }
  }
  return count;
}

size_t AbstractLogicalGroupsConfig::compactGroups(
    LogicalGroupConfig::Direction direction) {
  size_t groupCount = getGroupCount();
  for (size_t g = 0; g < groupCount; g++) {
    group[g].sanitize(direction, g);
    group[g].compactPorts();
  }
  size_t count = 0;
  for (size_t i = 0; i < groupCount; i++) {
    if (group[i].getPortCount()) {
      group[count++] = group[i];
    }
  }
  std::fill(group + count, group + MAX_GROUPS,
            LogicalGroupConfig::unsetConfig());

  return count;
}

size_t AbstractLogicalGroupsConfig::validateGroups(
    LogicalGroupConfig::Direction direction) {
  std::array<size_t, LogicalGroupConfig::MAX_CHANNELS> usedPorts;
  usedPorts.fill(UnsetValue<size_t>::value);
  size_t groupCount = compactGroups(direction);
  size_t channelCount = 0;
  for (size_t g = 0; g < groupCount; g++) {
    LogicalGroupConfig &config = group[g];
    for (size_t &port : config.ports) {
      if (!LogicalGroupConfig::isValidPortNumber(port)) {
        port = UnsetValue<size_t>::value;
        continue;
      }
      if (std::find(usedPorts.begin(), usedPorts.end(), port) ==
          usedPorts.end()) {
        if (channelCount < LogicalGroupConfig::MAX_CHANNELS) {
          usedPorts[channelCount++] = port;
        } else {
          std::cerr
              << "Logical " << direction << " group \"" << config.name
              << "\" removed port " << port
              << " as the maximum number of associated ports was exceeded."
              << std::endl;
          port = UnsetValue<size_t>::value;
        }
      } else {
        std::cerr << "Logical " << direction << " group \"" << config.name
                  << "\" removed port " << port << " that was already in use."
                  << std::endl;
        port = UnsetValue<size_t>::value;
      }
    }
    if (config.getPortCount() == 0) {
      if (!isUnsetConfigValue(config.name)) {
        std::cerr << "Logical " << direction << " group \"" << config.name
                  << "\" removed, as it is not associated with any port."
                  << std::endl;
      }
      config = LogicalGroupConfig::unsetConfig();
    }
  }
  return compactGroups(direction);
}

static inline AbstractLogicalGroupsConfig createUnsetLogicalGroupsConfig() {
  AbstractLogicalGroupsConfig result;
  std::fill(result.group,
            result.group + AbstractLogicalGroupsConfig::MAX_GROUPS,
            LogicalGroupConfig::unsetConfig());
  return result;
}

static inline AbstractLogicalGroupsConfig
createDefaultLogicalGroupsConfig(LogicalGroupConfig::Direction direction) {
  AbstractLogicalGroupsConfig result = createUnsetLogicalGroupsConfig();
  result.group[0].replaceWithDefaultsIfUnset(direction, 0);
  std::fill(result.group + 1,
            result.group + AbstractLogicalGroupsConfig::MAX_GROUPS,
            LogicalGroupConfig::unsetConfig());
  return result;
}

const AbstractLogicalGroupsConfig &AbstractLogicalGroupsConfig::defaultConfig(
    LogicalGroupConfig::Direction direction) {
  static AbstractLogicalGroupsConfig defaultInput =
      createDefaultLogicalGroupsConfig(LogicalGroupConfig::Direction::Input);
  static AbstractLogicalGroupsConfig defaultOutput =
      createDefaultLogicalGroupsConfig(LogicalGroupConfig::Direction::Output);
  return direction == LogicalGroupConfig::Direction::Input ? defaultInput
                                                           : defaultOutput;
}

const AbstractLogicalGroupsConfig &AbstractLogicalGroupsConfig::unsetConfig() {
  static AbstractLogicalGroupsConfig unset = createUnsetLogicalGroupsConfig();
  return unset;
}

void AbstractLogicalGroupsConfig::changeRuntimeValues(
    const AbstractLogicalGroupsConfig &runtimeValues,
    LogicalGroupConfig::Direction direction) {
  AbstractLogicalGroupsConfig copy = runtimeValues;
  size_t groupCount = copy.validateGroups(direction);
  size_t changes = std::min(groupCount, getGroupCount());
  for (size_t g = 0; g < changes; g++) {
    group[g].changeRuntimeValues(copy.group[g]);
  }
}
void AbstractLogicalGroupsConfig::sanitizeInitial(
    LogicalGroupConfig::Direction direction) {
  validateGroups(direction);
  if (getGroupCount() == 0) {
    *this = defaultConfig(direction);
  }
}
const LogicalPortMap AbstractLogicalGroupsConfig::createMapping() const {
  LogicalPortMap map;
  size_t groups = getGroupCount();
  for (size_t g = 0; g < groups; g++) {
    for (size_t c = 0; c < LogicalGroupConfig::MAX_CHANNELS; c++) {
      if (LogicalGroupConfig::isValidPortNumber(group[g].ports[c])) {
        map.add({group[g].ports[c], g, c});
      } else {
        break;
      }
    }
  }
  return map;
}
size_t AbstractLogicalGroupsConfig::getTotalChannels() const {
  size_t channels = 0;
  size_t groups = getGroupCount();
  for (size_t i = 0; i < groups; i++) {
    channels += group[i].getPortCount();
  }
  return channels;
}

LogicalPortMapEntry &LogicalPortMap::operator[](size_t i) {
  return entries[tdap::IndexPolicy::array(i, count)];
}
const LogicalPortMapEntry &LogicalPortMap::operator[](size_t i) const {
  return entries[tdap::IndexPolicy::array(i, count)];
}
bool LogicalPortMap::add(const LogicalPortMapEntry &entry) {
  if (count < LogicalGroupConfig::MAX_CHANNELS) {
    entries[count] = entry;
    entries[count].channel = count;
    count++;
    return true;
  }
  return false;
}
size_t LogicalPortMap::getCount() const { return count; }
const LogicalPortMapEntry *LogicalPortMap::begin() const { return entries; }
const LogicalPortMapEntry *LogicalPortMap::end() const {
  return entries + count;
}
size_t LogicalPortMapEntry::wrappedPort(size_t maximumPort) const {
  return 1 + std::max(0lu, port - 1) % std::max(1lu, maximumPort);
}
} // namespace speakerman
