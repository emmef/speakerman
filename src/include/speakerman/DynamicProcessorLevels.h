#ifndef SPEAKERMAN_DYNAMICPROCESSORLEVELS_H
#define SPEAKERMAN_DYNAMICPROCESSORLEVELS_H
/*
 * speakerman/DynamicProcessorLevels.h
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

#include <speakerman/ProcessingGroupConfig.h>
#include <tdap/IndexPolicy.hpp>
#include <tdap/Value.hpp>

#include <cstddef>
namespace speakerman {

using tdap::IndexPolicy;
using tdap::Values;

class DynamicProcessorLevels {
  double signal_square_[ProcessingGroupConfig::MAX_GROUPS + 1];
  size_t channels_;
  size_t count_;

  void addGainAndSquareSignal(size_t group, double signal) {
    size_t i = IndexPolicy::array(group, channels_);
    signal_square_[i] = Values::max(signal_square_[i], signal);
  }

public:
  DynamicProcessorLevels() : channels_(0), count_(0){};

  DynamicProcessorLevels(size_t groups)
      : channels_(groups + 1), count_(0) {}

  size_t groups() const { return channels_ - 1; }

  size_t count() const { return count_; }

  void operator+=(const DynamicProcessorLevels &levels) {
    size_t count = Values::min(channels_, levels.channels_);
    for (size_t i = 0; i < count; i++) {
      signal_square_[i] =
          Values::max(signal_square_[i], levels.signal_square_[i]);
    }
    count_ += levels.count_;
  }

  void next() { count_++; }

  void reset() {
    for (size_t limiter = 0; limiter < channels_; limiter++) {
      signal_square_[limiter] = 0.0;
    }
    count_ = 0;
  }

  void addValues(size_t group, double signal) {
    addGainAndSquareSignal(group, signal);
  }

  double getSignal(size_t group) const {
    return sqrt(signal_square_[IndexPolicy::array(group, channels_)]);
  }
};


} // namespace speakerman

#endif // SPEAKERMAN_DYNAMICPROCESSORLEVELS_H
