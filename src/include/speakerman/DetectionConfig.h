#ifndef SPEAKERMAN_M_DETECTION_CONFIG_H
#define SPEAKERMAN_M_DETECTION_CONFIG_H
/*
 * speakerman/DetectionConfig.h
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

#include <cstddef>

namespace speakerman {

struct DetectionConfig {
  static constexpr double MIN_MAXIMUM_WINDOW_SECONDS = 0.4;
  static constexpr double DEFAULT_MAXIMUM_WINDOW_SECONDS = 0.4;
  static constexpr double MAX_MAXIMUM_WINDOW_SECONDS = 8.0;

  static constexpr size_t MIN_PERCEPTIVE_LEVELS = 2;
  static constexpr size_t DEFAULT_PERCEPTIVE_LEVELS = 11;
  static constexpr size_t MAX_PERCEPTIVE_LEVELS = 32;

  static constexpr double MIN_MINIMUM_WINDOW_SECONDS = 0.0001;
  static constexpr double DEFAULT_MINIMUM_WINDOW_SECONDS = 0.001;
  static constexpr double MAX_MINIMUM_WINDOW_SECONDS = 0.010;

  static constexpr double MIN_RMS_FAST_RELEASE_SECONDS = 0.001;
  static constexpr double DEFAULT_RMS_FAST_RELEASE_SECONDS = 0.02;
  static constexpr double MAX_RMS_FAST_RELEASE_SECONDS = 0.10;

  static constexpr int DEFAULT_USE_BRICK_WALL_PREDICTION = 1;

  double maximum_window_seconds = DEFAULT_MAXIMUM_WINDOW_SECONDS;
  double minimum_window_seconds = DEFAULT_MINIMUM_WINDOW_SECONDS;
  double rms_fast_release_seconds = DEFAULT_RMS_FAST_RELEASE_SECONDS;
  size_t perceptive_levels = DEFAULT_PERCEPTIVE_LEVELS;
  int useBrickWallPrediction = DEFAULT_USE_BRICK_WALL_PREDICTION;

  static const DetectionConfig defaultConfig() { return {}; }

  static const DetectionConfig unsetConfig();

  void set_if_unset(const DetectionConfig &config_if_unset);
};


} // namespace speakerman

#endif // SPEAKERMAN_M_DETECTION_CONFIG_H
