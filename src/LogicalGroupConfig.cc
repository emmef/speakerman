//
// Created by michel on 09-01-22.
//

#include <speakerman/LogicalGroupConfig.h>
#include <array>
#include <iostream>

namespace speakerman {
const LogicalGroupConfig LogicalGroupConfig::unsetConfig() {
  LogicalGroupConfig result;
  unsetConfigValue(result.name);
  unsetConfigValue(result.volume);
  for (size_t &port : result.ports) {
    unsetConfigValue(port);
  }
  return result;
}
void LogicalGroupConfig::set_if_unset(
    const LogicalGroupConfig &config_if_unset) {
  setConfigValueIfUnset(volume, config_if_unset.volume);
}
void LogicalGroupConfig::sanitize(size_t groupNumber, const char *typeOfGroup) {
  size_t hasPorts = getPortCount();
  if (hasPorts > 0) {
    if (isUnsetConfigValue(volume)) {
      volume = DEFAULT_VOLUME;
    }
    if (isUnsetConfigValue(name)) {
      setDefaultNumberedName(groupNumber, typeOfGroup);
    }
    size_t destination = 0;
    for (size_t i = 0; i < LogicalGroupConfig::MAX_CHANNELS; i++) {
      if (!isUnsetConfigValue(ports[i])) {
        ports[destination++] = ports[i];
      }
    }
  } else {
    *this = unsetConfig();
  }
}
size_t LogicalGroupConfig::getPortCount() {
  size_t count = 0;
  for (const size_t port : ports) {
    if (!isUnsetConfigValue(port)) {
      count++;
    }
  }
  return count;
}
LogicalGroupConfig::LogicalGroupConfig() {
  for (size_t &port : ports) {
    unsetConfigValue(port);
  }
}
void LogicalGroupConfig::sanitize(LogicalGroupConfig *pConfig,
                                  const size_t groups,
                                  const char *typeOfGroup) {
  // First sanitize individual logical groups
  for (size_t group = 0; group < groups; group++) {
    pConfig->sanitize(group, typeOfGroup);
  }
  // Strip doubly-used I/O ports
  std::array<size_t, LogicalGroupConfig::MAX_CHANNELS> usedPorts;
  usedPorts.fill(UnsetValue<size_t>::value);
  size_t count = 0;
  for (size_t group = 0; group < groups; group++) {
    LogicalGroupConfig &config = pConfig[group];
    for (size_t &port : config.ports) {
      if (isUnsetConfigValue(port)) {
        break; // sanitize on individual group moved all ports tro the front of
               // the arrays.
      }
      if (std::find(usedPorts.begin(), usedPorts.end(), port) ==
          usedPorts.end()) {
        if (count < LogicalGroupConfig::MAX_CHANNELS) {
          usedPorts[count++] = port;
        } else {
          std::cerr
              << "Logical " << typeOfGroup << " group \"" << config.name
              << "\" removed port " << port
              << " as the maximum number of associated ports was exceeded."
              << std::endl;
          port = UnsetValue<size_t>::value;
        }
      } else {
        std::cerr << "Logical " << typeOfGroup << " group \"" << config.name
                  << "\" removed port " << port << " that was already in use."
                  << std::endl;
        port = UnsetValue<size_t>::value;
      }
    }
    if (config.getPortCount() == 0) {
      if (!isUnsetConfigValue(config.name)) {
        std::cerr << "Logical " << typeOfGroup << " group \"" << config.name
                  << "\" removed, as it is not associated with any port."
                  << std::endl;
      }
      config.sanitize(group, nullptr);
    }
  }
}

} // namespace speakerman
