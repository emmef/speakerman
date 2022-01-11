#ifndef SPEAKERMAN_M_EQUALIZER_CONFIG_H
#define SPEAKERMAN_M_EQUALIZER_CONFIG_H
/*
 * speakerman/EqualizerConfig.h
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
namespace speakerman {

struct EqualizerConfig {
  static constexpr double MIN_CENTER_FREQ = 20;
  static constexpr double DEFAULT_CENTER_FREQ = 1000;
  static constexpr double MAX_CENTER_FREQ = 22000;

  static constexpr double MIN_GAIN = 0.10;
  static constexpr double DEFAULT_GAIN = 1.0;
  static constexpr double MAX_GAIN = 10.0;

  static constexpr double MIN_BANDWIDTH = 0.25;
  static constexpr double DEFAULT_BANDWIDTH = 1;
  static constexpr double MAX_BANDWIDTH = 8.0;

  static const EqualizerConfig defaultConfig() { return {}; }

  static const EqualizerConfig unsetConfig();
  void set_if_unset(const EqualizerConfig &base_config_if_unset);

  double center = DEFAULT_CENTER_FREQ;
  double gain = DEFAULT_GAIN;
  double bandwidth = DEFAULT_BANDWIDTH;
};

} // namespace speakerman

#endif // SPEAKERMAN_M_EQUALIZER_CONFIG_H
