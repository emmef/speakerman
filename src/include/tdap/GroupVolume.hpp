#ifndef SPEAKERMAN_GROUPVOLUME_HPP
#define SPEAKERMAN_GROUPVOLUME_HPP
/*
 * speakerman/GroupVolume.hpp
 *
 * Added by michel on 2020-02-18
 * Copyright (C) 2015-2020 Michel Fleur.
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

#include <tdap/VolumeMatrix.hpp>

namespace tdap {

/**
 * Maps a maximum of CHANNELS channels to a maximum of CHANNELS groups.
 *
 * @tparam CHANNELS The maximum number of both channels and groups.
 */
template <size_t CHANNELS> class ChannelMapping {
  ptrdiff_t map_[CHANNELS];

public:
  static constexpr size_t channels = CHANNELS;

  /**
   * Creates a channel mapping, where no channel is mapped to any group.
   */
  ChannelMapping() { unmapGroup(-1); }

  /**
   * Returns the group that channel is mapped to or -1 when it is not mapped. If
   * the channel number is invalid, the method returns -1 and sets Error to
   * Error::BOUND.
   * @param channel The channel to get the mapping for.
   * @return The group the channel is assigned to or -1 if not assigned or on
   * error.
   */
  ptrdiff_t getGroupFor(size_t channel) const noexcept {
    if (channel < CHANNELS) {
      return map_[channel];
    }
    return Error::setReturn(Error::BOUND, -1);
  }

  /**
   * Maps channel to group. If that was successful, the method returns true. If
   * not, Error is set and the method returns false. If the channel or group are
   * invalid, Error is set to Error::BOUND. If the channel was already mapped to
   * another group and force was not set, Error is set to Error::FULL.
   * @param group The group.
   * @param channel The channel to map to the group.
   * @param force Determines if the channel is set even if already mapped to
   * another group.
   * @return true on success
   */
  bool map(size_t group, size_t channel, bool force = false) noexcept {

    if (group >= CHANNELS || channel >= CHANNELS) {
      return Error::setReturn(Error::BOUND);
    }
    ptrdiff_t &mapped = map_[channel];
    if (!force && mapped >= 0 && mapped != group) {
      return Error::setReturn(Error::FULL);
    }
    mapped = group;
    return true;
  }

  /**
   * Maps al unmapped channels to the group. If that was successful, the method
   * returns true. If not, Error is set and the method returns false. If the
   * group is invalid, Error is set to Error::BOUND.
   * @param group The group.
   * @return true on success
   */
  bool mapUnmapped(size_t group) noexcept {
    if (group >= CHANNELS) {
      return Error::setReturn(Error::BOUND);
    }
    size_t count = 0;
    for (size_t channel = 0; channel < CHANNELS; channel++) {
      ptrdiff_t &mapped = map_[channel];
      if (mapped < 0) {
        mapped = group;
        count++;
      }
    }
    return true;
  }

  /**
   * Remove mapping from channel to group. If that was successful, the method
   * returns true. If not, Error is set and the method returns false. If the
   * channel or group are invalid, Error is set to Error::BOUND. If the channel
   * was not assigned to this group, Error is set to Error::ACCESS.
   * @param group The group.
   * @param channel The channel to remove mapping for.
   * @return true on success
   */
  bool unmap(ptrdiff_t group, size_t channel) noexcept {
    if (group >= (ptrdiff_t )CHANNELS || channel >= CHANNELS) {
      return Error::setReturn(Error::BOUND);
    }
    ptrdiff_t &mapped = map_[channel];
    if (group < 0) {
      if (mapped < 0) {
        return Error::setReturn(Error::EMPTY);
      }
    } else if (mapped != group) {
      return Error::setReturn(Error::ACCESS);
    }
    map_[channel] = -1;
    return true;
  }

  /**
   * Remove all channel mappings for group or for all groups if group is
   * negative. If that was successful, the method returns true. If not, Error is
   * set and the method returns false. If the group is invalid, Error is set to
   * Error::BOUND.
   * @param group The group to remove all mappings for.
   * @return true on success
   */
  bool unmapGroup(ptrdiff_t group) noexcept {
    if (group >= (ptrdiff_t )CHANNELS) {
      return Error::setReturn(Error::BOUND);
    }
    for (size_t channel = 0; channel < CHANNELS; channel++) {
      ptrdiff_t &mapped = map_[channel];
      if (group < 0 || group == mapped) {
        mapped = -1;
      }
    }
    return true;
  }

  /**
   * @return the maxmimum channel number that is mapped to a group or -1 if no
   * channel is mapped to a group.
   */
  ptrdiff_t getMaxAssignedChannel() const noexcept {
    ptrdiff_t i = CHANNELS;
    while (--i >= 0) {
      if (map_[i] >= 0) {
        return i;
      }
    }
    return -1;
  }

  /**
   * @return the maxmimum group number that has channels mapped to it or -1 if
   * no channel is mapped to a group.
   */
  ptrdiff_t getMaxAssignedGroup() const noexcept {
    ptrdiff_t maxAssigned = -1;
    for (size_t i = 0; i < CHANNELS; i++) {
      maxAssigned = std::max(maxAssigned, map_[i]);
    }
    return maxAssigned;
  }

  /**
   * Gets the number of channels that is mapped to the group, which is zero if
   * the group has no assigned channels or the group is invalid. In the latter
   * case, error is set to Error::BOUND.
   * @param group The group.
   * @return the number of channels mapped to the group.
   */
  size_t getGroupChannels(size_t group) const noexcept {
    if (group >= CHANNELS) {
      return Error::setErrorReturn(Error::BOUND, 0);
    }
    size_t count = 0;
    for (size_t channel = 0; channel < CHANNELS; channel++) {
      if (map_[channel] == group) {
        count++;
      }
    }
    return count;
  }

  /**
   * Gets the number of the nth channel that is mapped to the group or zero if
   * there is no channel mapped to the group. In the latter case, or when n
   * exceeds the maximum possible channel number, error is set to Error::BOUND.
   * @param group The group.
   * @param n The channel index in the group.
   * @return
   */
  size_t getGroupChannel(size_t group, size_t n) const noexcept {
    ptrdiff_t count = -1;
    for (size_t channel = 0; channel < CHANNELS; channel++) {
      if (map_[channel] == group) {
        if (++count == n) {
          return channel;
        }
      }
    }
    return Error::setErrorReturn(Error::BOUND, 0);
  }
};

/**
 * Produces an individual channel volume matrix that is based on volumes on the
 * group level, using channel mappings for input and output groups.
 * @tparam T The sample type.
 * @tparam ICHANNELS The number of input channels and groups.
 * @tparam OCHANNELS The number of output channels and groups.
 */
template <typename T, size_t ICHANNELS, size_t OCHANNELS, size_t ALIGN = 4>
class GroupVolumeMatrix {
public:
  static constexpr size_t inputChannels = ICHANNELS;
  static constexpr size_t outputChannels = OCHANNELS;

  ChannelMapping<ICHANNELS> inputGroups;
  ChannelMapping<OCHANNELS> outputGroups;
  VolumeMatrix<T, ICHANNELS, OCHANNELS, ALIGN> volumes;

  /**
   * Given the group volumes, translates this to a volume matrix for all the
   * individual output and input channels. This process is calculation
   * intensive. If input groups and output groups do not have the same number of
   * channels, a number of strategies is used. A single-channel input group is
   * added to all channels of the output group. A single-channel output group
   * gets the sum of all input channels. If the input group has more channels
   * than the output group, the input channels are "wrapped".
   * If the target matrix cannot contain the mapped channels, Error is set to
   * Error::BOUND.
   * @tparam I The number of inputs of the target matrix.
   * @tparam O The number of outputs of the target matrix.
   * @tparam A The alignmnent in samples of the output matrix.
   * @param applyTo The target matrix that will contain the individual channel
   * volumes.
   * @return true on success.
   */
  template <size_t I, size_t O, size_t A>
  bool apply(VolumeMatrix<T, I, O, A> &applyTo) const noexcept {
    ptrdiff_t maxInputChannel = inputGroups.getMaxAssignedChannel();
    ptrdiff_t maxOutputChannel = outputGroups.getMaxAssignedChannel();
    if (maxInputChannel >= (ptrdiff_t )applyTo.inputs ||
        maxOutputChannel >= (ptrdiff_t)applyTo.outputs) {
      return Error::setErrorReturn(Error::BOUND);
    }
    applyTo.zero();
    size_t igChannels[ICHANNELS];
    size_t ogChannels[OCHANNELS];
    for (ptrdiff_t oGroup = 0; oGroup <= maxOutputChannel; oGroup++) {
      ogChannels[oGroup] = outputGroups.getGroupChannels(oGroup);
    }
    for (ptrdiff_t iGroup = 0; iGroup <= maxInputChannel; iGroup++) {
      igChannels[iGroup] = inputGroups.getGroupChannels(iGroup);
    }

    for (ptrdiff_t oGroup = 0; oGroup <= maxOutputChannel; oGroup++) {
      size_t ogCount = ogChannels[oGroup];
      if (ogCount == 0) {
        continue;
      }
      for (ptrdiff_t iGroup = 0; iGroup <= maxInputChannel; iGroup++) {
        size_t igCount = igChannels[iGroup];
        if (igCount == 0) {
          continue;
        }
        T volume = volumes.get(oGroup, iGroup);
        size_t i;
        if (igCount == 1) {
          for (i = 0; i < ogCount; i++) {
            setVolume(applyTo, outputGroups.getGroupChannel(oGroup, i),
                      inputGroups.getGroupChannel(iGroup, 0), volume);
          }
          continue;
        }
        if (ogCount == 1) {
          for (i = 0; i < igCount; i++) {
            setVolume(applyTo, outputGroups.getGroupChannel(oGroup, 0),
                      inputGroups.getGroupChannel(iGroup, i), volume);
          }
          continue;
        }
        for (i = 0; i < std::min(igCount, ogCount); i++) {
          setVolume(applyTo, outputGroups.getGroupChannel(oGroup, i),
                    inputGroups.getGroupChannel(iGroup, i), volume);
        }
        if (igCount > ogCount) {
          for (; i < igCount; i++) {
            setVolume(applyTo,
                      outputGroups.getGroupChannel(oGroup, i % ogCount),
                      inputGroups.getGroupChannel(iGroup, i), volume);
          }
        }
      }
    }
    return true;
  }

private:

  template <size_t I, size_t O, size_t A>
  static void setVolume(VolumeMatrix<T, I, O, A> &applyTo, ptrdiff_t output,
                        ptrdiff_t input, T volume) {
    if (output >= 0 && input >= 0) {
      applyTo.set(output, input, volume);
    }
  }
};

} // namespace tdap

#endif // SPEAKERMAN_GROUPVOLUME_HPP
