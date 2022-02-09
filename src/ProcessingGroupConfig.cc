//
// Created by michel on 09-01-22.
//

#include <algorithm>
#include <speakerman/ProcessingGroupConfig.h>
#include <speakerman/UnsetValue.h>

namespace speakerman {

void ProcessingGroupConfig::makeValidateBasedOn(
    const ProcessingGroupConfig &copyFrom, size_t groupId,
    size_t logicalChannels) {
  // Number of EQs
  setDefaultOrBoxedFromSourceIfUnset(eqs, DEFAULT_EQS, copyFrom.eqs, MIN_EQS,
                                     MAX_EQS);
  // Set EQs themselves
  size_t i = 0;
  for (; i < eqs; i++) {
    eq[i].set_if_unset(copyFrom.eq[i]);
  }
  for (; i < MAX_EQS; i++) {
    eq[i] = EqualizerConfig::unsetConfig();
  }
  // Name
  if (isUnsetConfigValue(name)) {
    if (isUnsetConfigValue(copyFrom.name)) {
      setDefaultNumberedName(groupId + 1);
    } else {
      copyToName(copyFrom.name);
    }
  }
  // Volumes
  size_t zeroCount = std::min(
      logicalChannels ? logicalChannels : LogicalGroupConfig::DEFAULT_CHANNELS,
      MAX_CHANNELS);
  // Other values
  setDefaultOrBoxedFromSourceIfUnset(delay, DEFAULT_DELAY, copyFrom.delay,
                                     MIN_DELAY, MAX_DELAY);
  setDefaultOrBoxedFromSourceIfUnset(threshold, DEFAULT_THRESHOLD,
                                     copyFrom.threshold, MIN_THRESHOLD,
                                     MAX_THRESHOLD);
  setDefaultOrFromSourceIfUnset(mono, DEFAULT_MONO, copyFrom.mono);
  setDefaultOrFromSourceIfUnset(useSub, DEFAULT_USE_SUB, copyFrom.useSub);
}

ProcessingGroupConfig::ProcessingGroupConfig() : NamedConfig({0}) {
  std::fill(eq, eq + MAX_EQS, EqualizerConfig::unsetConfig());
}

const ProcessingGroupConfig &ProcessingGroupConfig::unsetConfig() {
  static ProcessingGroupConfig result;
  return result;
}

void ProcessingGroupConfig::setDefaultNumberedName(size_t groupId) {
  printToName("Processing group %zd", groupId);
}

void ProcessingGroupConfig::copyRuntimeValues(
    const ProcessingGroupConfig &copyFrom) {
  setBoxedFromSetSource(eqs, copyFrom.eqs, MIN_EQS, MAX_EQS);
  // Set EQs themselves
  size_t i = 0;
  for (; i < eqs; i++) {
    eq[i].set_if_unset(copyFrom.eq[i]);
  }
  for (; i < MAX_EQS; i++) {
    eq[i] = EqualizerConfig::unsetConfig();
  }
  setBoxedFromSetSource(delay, copyFrom.delay,
                                     MIN_DELAY, MAX_DELAY);
  setBoxedFromSetSource(threshold, copyFrom.threshold, MIN_THRESHOLD,
                                     MAX_THRESHOLD);
  setFromSetSource(mono, copyFrom.mono);
  setFromSetSource(useSub, copyFrom.useSub);
  if (!isUnsetConfigValue(copyFrom.name)) {
    copyToName(copyFrom.name);
  }
}

// ProcessingGroupsConfig

ProcessingGroupsConfig::ProcessingGroupsConfig() {
  std::fill(group, group + MAX_GROUPS, ProcessingGroupConfig::unsetConfig());
}

void ProcessingGroupsConfig::sanitizeInitial(size_t totalChannels) {
  setDefaultOrBoxedFromSourceIfUnset(groups, DEFAULT_GROUPS, groups, MIN_GROUPS,
                                     MAX_GROUPS);
  size_t maxChannels = groups ? LogicalGroupConfig::MAX_CHANNELS / groups
                              : LogicalGroupConfig::MAX_CHANNELS;
  setDefaultOrBoxedFromSourceIfUnset(channels, DEFAULT_GROUP_CHANNELS, channels,
                                     MIN_GROUP_CHANNELS, maxChannels);
  size_t i = 0;
  for (; i < groups; i++) {
    group[i].makeValidateBasedOn(group[i], i, totalChannels);
  }
  for (; i < MAX_GROUPS; i++) {
    group[i] = ProcessingGroupConfig::unsetConfig();
  }
}
void ProcessingGroupsConfig::changeRuntimeValues(
    const ProcessingGroupsConfig &runtime) {
  for (size_t i = 0; i < groups; i++) {
    group[i].copyRuntimeValues(runtime.group[i]);
  }
}

} // namespace speakerman
