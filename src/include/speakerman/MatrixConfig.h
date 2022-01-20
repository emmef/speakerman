#ifndef SPEAKERMAN_M_MATRIX_CONFIG_H
#define SPEAKERMAN_M_MATRIX_CONFIG_H
/*
 * speakerman/MatrixConfig.h
 *
 * Added by michel on 2022-01-17
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

#include <speakerman/LogicalGroupConfig.h>
#include <speakerman/ProcessingGroupConfig.h>

namespace speakerman {
  struct MatrixConfig {
    static constexpr size_t TOTAL_WEIGHTS = ProcessingGroupConfig::MAX_CHANNELS * LogicalGroupConfig::MAX_CHANNELS;
    double weights[TOTAL_WEIGHTS];

    static constexpr double MIN_WEIGHT = 0.0;
    static constexpr double MAX_WEIGHT = 10.0;

    double *weightsFor(size_t processingChannel);
    const double *weightsFor(size_t processingChannel) const;
    double & weight(size_t processingChannel, size_t logicalChannel);
    const double & weight(size_t processingChannel, size_t logicalChannel) const;

    MatrixConfig();

    static const MatrixConfig &unsetConfig();
    void replaceWithDefaultsIfUnset(size_t processingChannels, size_t logicalChannels);
    void changeRuntimeValues(const MatrixConfig &newRuntimeConfig, size_t processingChannels,
                             size_t logicalChannels);
  };
} // namespace speakerman

#endif // SPEAKERMAN_M_MATRIX_CONFIG_H
