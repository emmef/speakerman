//
// Created by michel on 20-01-22.
//

#include <speakerman/MatrixConfig.h>
#include <speakerman/UnsetValue.h>
#include <tdap/IndexPolicy.hpp>

namespace speakerman {

MatrixConfig::MatrixConfig() {
  std::fill(weights, weights + TOTAL_WEIGHTS, UnsetValue<double>::value);
}

double *MatrixConfig::weightsFor(size_t processingChannel) {
  return weights +
         LogicalGroupConfig::MAX_CHANNELS *
             tdap::IndexPolicy::method(processingChannel,
                                       ProcessingGroupConfig::MAX_CHANNELS);
}

const double *MatrixConfig::weightsFor(size_t processingChannel) const {
  return weights +
         LogicalGroupConfig::MAX_CHANNELS *
             tdap::IndexPolicy::method(processingChannel,
                                       ProcessingGroupConfig::MAX_CHANNELS);
}

double &MatrixConfig::weight(size_t processingChannel, size_t logicalChannel) {
  return *(weightsFor(processingChannel) +
           tdap::IndexPolicy::method(logicalChannel,
                                     LogicalGroupConfig::MAX_CHANNELS));
}

const double &MatrixConfig::weight(size_t processingChannel,
                                   size_t logicalChannel) const {
  return *(weightsFor(processingChannel) +
           tdap::IndexPolicy::method(logicalChannel,
                                     LogicalGroupConfig::MAX_CHANNELS));
}

const MatrixConfig &MatrixConfig::unsetConfig() {
  static const MatrixConfig instance;
  return instance;
}

void MatrixConfig::replaceWithDefaultsIfUnset(size_t processingChannels,
                                              size_t logicalChannels) {
  size_t minChannels = std::min(processingChannels, logicalChannels);
  for (size_t pc = 0; pc < processingChannels; pc++) {
    for (size_t lc = 0; lc < logicalChannels; lc++) {
      double defaultValue =
          ((pc % minChannels) == (lc % minChannels)) ? 1.0 : 0.0;
      setConfigValueIfUnset(weight(pc, lc), defaultValue);
    }
  }
}

void MatrixConfig::changeRuntimeValues(const MatrixConfig &newRuntimeConfig,
                                       size_t processingChannels,
                                       size_t logicalChannels) {
  size_t pc;
  size_t lc;
  size_t minChannels = std::min(processingChannels, logicalChannels);

  for (pc = 0; pc < processingChannels; pc++) {
    double *weights = weightsFor(pc);
    for (lc = 0; lc < logicalChannels; lc++) {
      double defaultValue =
          ((pc % minChannels) == (lc % minChannels)) ? 1.0 : 0.0;
      setDefaultOrBoxedFromSourceIfUnset(weights[lc], defaultValue,
                                         newRuntimeConfig.weight(pc, lc),
                                         MIN_WEIGHT, MAX_WEIGHT);
    }
    std::fill(weights + lc, weights + LogicalGroupConfig::MAX_CHANNELS,
              UnsetValue<double>::value);
  }

  for (; pc < ProcessingGroupConfig::MAX_CHANNELS; pc++) {
    double *weights = weightsFor(pc);
    std::fill(weights, weights + LogicalGroupConfig::MAX_CHANNELS,
              UnsetValue<double>::value);
  }
}

} // namespace speakerman
