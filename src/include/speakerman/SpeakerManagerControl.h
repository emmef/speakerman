#ifndef SPEAKERMAN_M_SPEAKER_MANAGER_CONTROL_H
#define SPEAKERMAN_M_SPEAKER_MANAGER_CONTROL_H
/*
 * speakerman/SpeakerManagerControl.h
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

#include <speakerman/DynamicProcessorLevels.h>
#include <speakerman/SpeakermanConfig.hpp>

namespace speakerman {

class SpeakerManagerControl {
public:
  virtual const SpeakermanConfig &getConfig() const = 0;

  virtual bool
  applyConfigAndGetLevels(const SpeakermanConfig &config,
                          DynamicProcessorLevels *levels,
                          std::chrono::milliseconds timeoutMillis) = 0;

  virtual bool getLevels(DynamicProcessorLevels *levels,
                         std::chrono::milliseconds timeoutMillis) = 0;

  virtual const jack::ProcessingStatistics getStatistics() const = 0;
  virtual ~SpeakerManagerControl() = default;
};

} // namespace speakerman

#endif // SPEAKERMAN_M_SPEAKER_MANAGER_CONTROL_H
