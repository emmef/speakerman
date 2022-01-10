//
// Created by michel on 09-01-22.
//

#include <speakerman/EqualizerConfig.h>
#include <speakerman/UnsetValue.h>

namespace speakerman {

const EqualizerConfig EqualizerConfig::unsetConfig() {
  return {speakerman::UnsetValue<double>::value,
          speakerman::UnsetValue<double>::value,
          speakerman::UnsetValue<double>::value};
}
void EqualizerConfig::set_if_unset(
    const EqualizerConfig &base_config_if_unset) {
  setDefaultOrBoxedFromSourceIfUnset(center, DEFAULT_CENTER_FREQ,
                                     base_config_if_unset.center,
                                     MIN_CENTER_FREQ, MAX_CENTER_FREQ);
  setDefaultOrBoxedFromSourceIfUnset(
      gain, DEFAULT_GAIN, base_config_if_unset.gain, MIN_GAIN, MAX_GAIN);
  setDefaultOrBoxedFromSourceIfUnset(bandwidth, DEFAULT_BANDWIDTH,
                                     base_config_if_unset.bandwidth,
                                     MIN_BANDWIDTH, MAX_BANDWIDTH);
}

} // namespace speakerman
