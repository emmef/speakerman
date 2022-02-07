//
// Created by michel on 07-02-22.
//

#include <speakerman/SpeakerManagerGenerator.h>

namespace speakerman {

template <typename F, size_t GROUPS, size_t CROSSOVERS, size_t LOGICAL_INPUTS,
          size_t CHANNELS_PER_GROUP = ProcessingGroupConfig::MAX_CHANNELS>
AbstractSpeakerManager *
createManagerSampleType(const SpeakermanConfig &config) {
  static_assert(is_floating_point<F>::value,
                "Sample type must be floating point");
  const size_t channelsPerGroup = config.processingGroups.channels;

  if (channelsPerGroup > ProcessingGroupConfig::MAX_CHANNELS) {
    throw invalid_argument("Maximum number of channels per group exceeded.");
  }
  if constexpr (CHANNELS_PER_GROUP < 1) {
    throw invalid_argument("Must have at least one channel per group.");
  } else if (channelsPerGroup == CHANNELS_PER_GROUP) {
    return new SpeakerManager<F, CHANNELS_PER_GROUP, GROUPS, CROSSOVERS,
                              LOGICAL_INPUTS>(config);
  } else {
    return createManagerSampleType<F, GROUPS, CROSSOVERS, LOGICAL_INPUTS,
                                   CHANNELS_PER_GROUP - 1>(config);
  }
}

template <typename F, size_t CROSSOVERS, size_t LOGICAL_INPUTS,
          size_t PROCESSING_GROUPS = ProcessingGroupsConfig::MAX_GROUPS>
static AbstractSpeakerManager *
createManagerGroup(const SpeakermanConfig &config) {

  size_t processingGroups = config.processingGroups.groups;
  if (processingGroups > ProcessingGroupsConfig::MAX_GROUPS) {
    throw std::invalid_argument(
        "Maximum number of processing groups exceeded.");
  }
  if constexpr (PROCESSING_GROUPS < 1) {
    throw std::invalid_argument("Need at least one processing group.");
  } else if (processingGroups == PROCESSING_GROUPS) {
    return createManagerSampleType<F, PROCESSING_GROUPS, CROSSOVERS,
                                   LOGICAL_INPUTS>(config);
  } else {
    return createManagerGroup<F, CROSSOVERS, LOGICAL_INPUTS,
                              PROCESSING_GROUPS - 1>(config);
  }
}

template <typename F, size_t LOGICAL_INPUTS,
          size_t CROSSOVERS = SpeakermanConfig::MAX_CROSSOVERS>
static AbstractSpeakerManager *
createManagerCrossovers(const SpeakermanConfig &config) {
  size_t crossovers = config.crossovers;
  if (crossovers > SpeakermanConfig::MAX_CROSSOVERS) {
    throw std::invalid_argument("Maximum number of crossovers exceeded.");
  }
  if constexpr (CROSSOVERS < 1) {
    throw std::invalid_argument("Need at least one crossover.");
  } else if (crossovers == CROSSOVERS) {
    return createManagerGroup<double, CROSSOVERS, LOGICAL_INPUTS>(config);
  } else {
    return createManagerCrossovers<F, LOGICAL_INPUTS, CROSSOVERS - 1>(config);
  }
}

using namespace std;

template <size_t TOTAL_CHANNELS = LogicalGroupConfig::MAX_CHANNELS>
AbstractSpeakerManager *create_manager(const SpeakermanConfig &config) {
  size_t channels = config.logicalInputs.getTotalChannels();
  if (channels > LogicalGroupConfig::MAX_CHANNELS) {
    throw std::invalid_argument("Maximum total number logical input channels exceeded.");
  }
  if constexpr (TOTAL_CHANNELS < 1) {
    throw std::invalid_argument("Need at least one logical input channel.");
  }
  else if (channels == TOTAL_CHANNELS) {
    return createManagerCrossovers<double, TOTAL_CHANNELS>(config);
  }
  else {
    return create_manager<TOTAL_CHANNELS - 1>(config);
  }
}

AbstractSpeakerManager *createManager(const SpeakermanConfig &config) {
  return create_manager(config);
}

} // namespace speakerman
