//
// Created by michel on 09-01-22.
//
#include <speakerman/DetectionConfig.h>
#include <speakerman/UnsetValue.h>

namespace speakerman {

const DetectionConfig DetectionConfig::unsetConfig() {
  DetectionConfig result;

  unsetConfigValue(result.useBrickWallPrediction);
  unsetConfigValue(result.maximum_window_seconds);
  unsetConfigValue(result.minimum_window_seconds);
  unsetConfigValue(result.rms_fast_release_seconds);
  unsetConfigValue(result.perceptive_levels);

  return result;
}

void DetectionConfig::set_if_unset(const DetectionConfig &config_if_unset) {
  int &value = useBrickWallPrediction;
  bool result;
  result = setConfigValueIfUnset(value, config_if_unset.useBrickWallPrediction);
  fixedValueIfUnsetOrBoxedIfOutOfRange(
      maximum_window_seconds, config_if_unset.maximum_window_seconds,
      MIN_MAXIMUM_WINDOW_SECONDS, MAX_MAXIMUM_WINDOW_SECONDS);
  fixedValueIfUnsetOrBoxedIfOutOfRange(
      minimum_window_seconds, config_if_unset.minimum_window_seconds,
      MIN_MINIMUM_WINDOW_SECONDS, MAX_MINIMUM_WINDOW_SECONDS);
  fixedValueIfUnsetOrBoxedIfOutOfRange(
      rms_fast_release_seconds, config_if_unset.rms_fast_release_seconds,
      MIN_RMS_FAST_RELEASE_SECONDS, MAX_RMS_FAST_RELEASE_SECONDS);
  fixedValueIfUnsetOrBoxedIfOutOfRange(
      perceptive_levels, config_if_unset.perceptive_levels,
      MIN_PERCEPTIVE_LEVELS, MAX_PERCEPTIVE_LEVELS);
}

} // namespace speakerman
