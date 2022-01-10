//
// Created by michel on 09-01-22.
//

#include <speakerman/ProcessingGroupConfig.h>
#include <speakerman/UnsetValue.h>

namespace speakerman {

void ProcessingGroupConfig::set_if_unset(
    const ProcessingGroupConfig &config_if_unset) {
  size_t eq_idx;
  if (fixedValueIfUnsetOrOutOfRange(eqs, config_if_unset.eqs, MIN_EQS,
                                    MAX_EQS)) {
    for (eq_idx = 0; eq_idx < eqs; eq_idx++) {
      eq[eq_idx] = config_if_unset.eq[eq_idx];
    }
  } else {
    for (eq_idx = 0; eq_idx < eqs; eq_idx++) {
      eq[eq_idx].set_if_unset(config_if_unset.eq[eq_idx]);
    }
  }
  for (; eq_idx < MAX_EQS; eq_idx++) {
    eq[eq_idx] = EqualizerConfig::unsetConfig();
  }

  for (size_t group = 0; group < ProcessingGrouspConfig::MAX_GROUPS; group++) {
    fixedValueIfUnsetOrOutOfRange(volume[group], config_if_unset.volume[group],
                                  MIN_VOLUME, MAX_VOLUME);
  }

  fixedValueIfUnsetOrBoxedIfOutOfRange(threshold, config_if_unset.threshold,
                                       MIN_THRESHOLD, MAX_THRESHOLD);
  fixedValueIfUnsetOrBoxedIfOutOfRange(delay, config_if_unset.delay, MIN_DELAY,
                                       MAX_DELAY);
  fixedValueIfUnsetOrBoxedIfOutOfRange(use_sub, config_if_unset.use_sub, 0, 1);
  fixedValueIfUnsetOrBoxedIfOutOfRange(mono, config_if_unset.mono, 0, 1);
  if (UnsetValue<char[NAME_LENGTH + 1]>::is(name)) {
    name[0] = 0;
  }
}

const ProcessingGroupConfig
ProcessingGroupConfig::defaultConfig(size_t group_id) {
  ProcessingGroupConfig result;
  result.setDefaultNumberedName(group_id + 1);
  return result;
}

const ProcessingGroupConfig ProcessingGroupConfig::unsetConfig() {
  ProcessingGroupConfig result;
  for (size_t i = 0; i < MAX_EQS; i++) {
    result.eq[i] = EqualizerConfig::unsetConfig();
  }
  for (size_t i = 0; i < ProcessingGrouspConfig::MAX_GROUPS; i++) {
    unsetConfigValue(result.volume[i]);
  }
  unsetConfigValue(result.eqs);
  unsetConfigValue(result.threshold);
  unsetConfigValue(result.delay);
  unsetConfigValue(result.use_sub);
  unsetConfigValue(result.mono);
  result.name[0] = 0;
  return result;
}

void ProcessingGroupConfig::setDefaultNumberedName(size_t groupId) {
  printToName("Processing group %zd", groupId);
}



} // namespace speakerman
