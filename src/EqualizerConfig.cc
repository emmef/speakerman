//
// Created by michel on 09-01-22.
//

#include <speakerman/EqualizerConfig.h>
#include <speakerman/UnsetValue.h>

namespace speakerman {

const EqualizerConfig EqualizerConfig::unsetConfig() {
  return {speakerman::UnsetValue<double>::value, speakerman::UnsetValue<double>::value,
          speakerman::UnsetValue<double>::value};
}
void EqualizerConfig::set_if_unset(
    const EqualizerConfig &base_config_if_unset) {
  if (fixedValueIfUnsetOrOutOfRange(center, base_config_if_unset.center,
                                           MIN_CENTER_FREQ, MAX_CENTER_FREQ)) {
    (*this) = base_config_if_unset;
  } else {
    unsetIfInvalid(center, MIN_CENTER_FREQ, MAX_CENTER_FREQ);
    fixedValueIfUnsetOrBoxedIfOutOfRange(gain, DEFAULT_GAIN, MIN_GAIN, MAX_GAIN);
    fixedValueIfUnsetOrBoxedIfOutOfRange(bandwidth, DEFAULT_BANDWIDTH, MIN_BANDWIDTH,
                        MAX_BANDWIDTH);
  }
}

} // namespace speakerman
