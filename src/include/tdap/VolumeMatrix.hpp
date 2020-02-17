/*
 * tdap/VolumeMatrix.hpp
 *
 * Part of TdAP
 * Time-domain Audio Processing
 * Copyright (C) 2015 Michel Fleur.
 * Source https://bitbucket.org/emmef/tdap
 * Email  tdap@emmef.org
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

#ifndef TDAP_VALUE_VOLUME_MATRIX_GUARD
#define TDAP_VALUE_VOLUME_MATRIX_GUARD

#include <algorithm>
#include <tdap/AlignedFrame.hpp>
#include <tdap/IndexPolicy.hpp>
#include <tdap/Integration.hpp>

namespace tdap {

template <typename T, size_t INPUTS, size_t OUTPUTS, size_t ALIGN = 4>
class VolumeMatrix {
  static_assert(std::is_floating_point<T>::value,
                "Expecting floating point type parameter");
  static_assert(Power2::constant::is(ALIGN), "ALIGN is not a power of 2.");
  static_assert(Count<T>::valid_positive(INPUTS), "Invalid INPUTS parameter");
  static_assert(Count<T>::valid_positive(OUTPUTS), "Invalid OUTPUTS parameter");
  static constexpr size_t MIN_CHANNELS = std::min(INPUTS, OUTPUTS);
  static constexpr size_t MAX_CHANNELS = std::max(INPUTS, OUTPUTS);

public:
  using InputVolumes = AlignedFrame<T, INPUTS, ALIGN>;
  using InputFrame = AlignedFrame<T, INPUTS, ALIGN>;
  using OutputFrame = AlignedFrame<T, OUTPUTS, ALIGN>;

  static constexpr size_t inputs = INPUTS;
  static constexpr size_t outputs = OUTPUTS;
  static constexpr size_t alignment = ALIGN;

  T epsilon() const noexcept { return epsilon_; }

  T setEpsilon(T epsilon) noexcept {
    return epsilon_ =
               std::clamp(epsilon, std::numeric_limits<T>::epsilon(), 0.1);
  }

  const InputVolumes &getOUtputMix(size_t idx) const {
    return matrix_[IndexPolicy::method(idx, OUTPUTS)];
  }

  VolumeMatrix(T value) { set_all(value); }

  VolumeMatrix() { identity(); }

  void identity(T scale = 1.0) noexcept {
    zero();
    T flushedScale = flushToZero(scale);
    for (size_t i = 0; i < MIN_CHANNELS; i++) {
      matrix_[i][i] = flushedScale;
    }
  }

  void identityWrapped(T scale = 1.0) noexcept {
    zero();
    T flushedScale = flushToZero(scale);
    for (size_t output = 0; output < MAX_CHANNELS; output++) {
      matrix_[output % OUTPUTS][output % INPUTS] = flushedScale;
    }
  }

  void zero() noexcept {
    for (size_t output = 0; output < OUTPUTS; output++) {
      for (size_t input = 0; input < INPUTS; input++) {
        matrix_[output][input] = 0;
      }
    }
  }

  void flushAllToZero() noexcept {
    for (size_t output = 0; output < OUTPUTS; output++) {
      for (size_t input = 0; input < INPUTS; input++) {
        matrix_[output][input] = flushToZero(matrix_[output][input]);
      }
    }
  }

  void set(size_t output, size_t input, T volume) {
    int x = matrix_[IndexPolicy::method(output, OUTPUTS)]
                   [IndexPolicy::method(input, INPUTS)] = flushToZero(volume);
  }

  T get(size_t output, size_t input) const {
    return matrix_[IndexPolicy::method(output, OUTPUTS)]
                  [IndexPolicy::method(input, INPUTS)];
  }

  void setAll(T volume, T epsilon = 1e-6) noexcept {
    T flushedToZero = flushToZero(volume);
    for (size_t output = 0; output < OUTPUTS; output++) {
      for (size_t input = 0; input < INPUTS; input++) {
        matrix_[output][input] = flushedToZero;
      }
    }
  }

  void approach(const VolumeMatrix &source,
                const IntegrationCoefficients<T> &coefficients) {
    for (size_t output = 0; output < OUTPUTS; output++) {
      for (size_t input = 0; input < INPUTS; input++) {
        T sourceValue = source.matrix_[output][input];
        T &out = matrix_[output][input];
        if (out == 0 && sourceValue > epsilon_) {
          out = sourceValue;
        } else {
          out = flushToZero(coefficients.getIntegrated(sourceValue, out));
        }
      }
    }
  }

  template <size_t A>
  AlignedFrame<T, OUTPUTS, A>
  apply(const AlignedFrame<T, INPUTS, A> &inputs) const {
    AlignedFrame<T, OUTPUTS, A> outputs;
    for (size_t output = 0; output < OUTPUTS; output++) {
      outputs[output] = inputs.dot(matrix_[output]);
    }
    return outputs;
  }

  template <size_t A>
  AlignedFrame<T, OUTPUTS, A>
  applySeeded(const AlignedFrame<T, INPUTS, A> &inputs, T seed) const {
    AlignedFrame<T, OUTPUTS, A> outputs;
    for (size_t output = 0; output < OUTPUTS; output++) {
      outputs[output] = inputs.dotSeeded(matrix_[output], seed);
    }
    return outputs;
  }

  template <size_t A>
  void apply(AlignedFrame<T, OUTPUTS, A> &__restrict outputs,
             const AlignedFrame<T, INPUTS, A> &__restrict inputs) const {

    unSeededApply<A, std::min(A, ALIGN)>(outputs, inputs);
  }

  template <size_t A>
  void applySeeded(AlignedFrame<T, OUTPUTS, A> &__restrict outputs,
                   const AlignedFrame<T, INPUTS, A> &__restrict inputs,
                   T seed) const {
    seededApply<A, std::min(A, ALIGN)>(outputs, inputs, seed);
  }

private:
  InputVolumes matrix_[OUTPUTS];
  T epsilon_ = 1e-6;

  T flushToZero(T volume) const noexcept {
    return fabs(volume) > epsilon_ ? volume : 0;
  }

  template <size_t A, size_t COMMON_ALIGN>
  void seededApply(AlignedFrame<T, OUTPUTS, A> &__restrict outputs,
                   const AlignedFrame<T, INPUTS, A> &__restrict inputs,
                   T seed) const {
    AlignedFrame<T, OUTPUTS, A> &out =
        *assume_aligned<COMMON_ALIGN, AlignedFrame<T, OUTPUTS, A>>(&outputs);
    const AlignedFrame<T, INPUTS, A> &in =
        *assume_aligned<COMMON_ALIGN, const AlignedFrame<T, INPUTS, A>>(
            &inputs);
    for (size_t output = 0; output < OUTPUTS; output++) {
      out[output] = in.dotSeeded(matrix_[output], seed);
    }
  }

  template <size_t A, size_t COMMON_ALIGN>
  void
  unSeededApply(AlignedFrame<T, OUTPUTS, A> &__restrict outputs,
                const AlignedFrame<T, INPUTS, A> &__restrict inputs) const {

    AlignedFrame<T, OUTPUTS, A> &out =
        *assume_aligned<COMMON_ALIGN, AlignedFrame<T, OUTPUTS, A>>(&outputs);
    const AlignedFrame<T, INPUTS, A> &in =
        *assume_aligned<COMMON_ALIGN, const AlignedFrame<T, INPUTS, A>>(
            &inputs);
    for (size_t output = 0; output < OUTPUTS; output++) {
      out[output] = in.dot(matrix_[output]);
    }
  }
};

template <typename T, size_t INPUTS, size_t OUTPUTS, size_t ALIGN = 4>
struct VolumeMatrixSmooth {
  using Matrix = VolumeMatrix<T, INPUTS, OUTPUTS, ALIGN>;

  const Matrix &matrix() const noexcept { return actualVolume; }

  VolumeMatrixSmooth() {
    userVolume.zero();
    actualVolume.zero();
    integration.setCharacteristicSamples(96000 * 0.05);
  }

  void setSmoothSamples(T samples) {
    integration.setCharacteristicSamples(samples);
  }

  void zero() {
    userVolume.zero();
    actualVolume.zero();
  }

  void approach() { actualVolume.approach(userVolume, integration); }

  void configure(double sampleRate, double rc, Matrix initialVolumes) {
    integration.setCharacteristicSamples(sampleRate * rc);
    userVolume.identity(initialVolumes);
    actualVolume.set_all(0);
  }

  void setVolume(const Matrix &newVolumes) { userVolume = newVolumes; }

private:
  Matrix actualVolume;
  Matrix userVolume;
  IntegrationCoefficients<double> integration;
};

enum class VolumeControlResult {
  SUCCESS,
  HIGH_CHANNEL,
  HIGH_GROUP,
  CHANNEL_MAPPED,
  CHANNEL_NOT_MAPPED
};

template <typename T, size_t CHANNELS> class GroupMap {
  ptrdiff_t map_[CHANNELS];

  static bool returnAndAssign(VolumeControlResult *result,
                              VolumeControlResult code) noexcept {
    if (result) {
      *result = code;
    }
    return code == VolumeControlResult ::SUCCESS;
  }

public:
  static constexpr size_t channels = CHANNELS;

  GroupMap() { removeGroupChannels(-1); }

  ptrdiff_t getGroupFor(size_t channel) const noexcept {
    return channel < CHANNELS ? map_[channel] : -1;
  }

  bool addToGroup(size_t group, size_t channel, bool force = false,
                  VolumeControlResult *result = nullptr) noexcept {

    if (group >= CHANNELS) {
      return returnAndAssign(result, VolumeControlResult::HIGH_GROUP);
    }
    if (channel >= CHANNELS) {
      return returnAndAssign(result, VolumeControlResult::HIGH_CHANNEL);
    }
    if (map_[channel] >= 0 && !force) {
      return returnAndAssign(result, VolumeControlResult::CHANNEL_MAPPED);
    }
    map_[channel] = group;
    return returnAndAssign(result, VolumeControlResult ::SUCCESS);
  }

  bool addUnnasignedToGroup(size_t group,
                            VolumeControlResult *result = nullptr) noexcept {
    if (group >= CHANNELS) {
      return returnAndAssign(result, VolumeControlResult::HIGH_GROUP);
    }
    size_t count = 0;
    for (size_t channel = 0; channel < CHANNELS; channel++) {
      ptrdiff_t &mapped = map_[channel];
      if (mapped < 0) {
        mapped = group;
        count++;
      }
    }
    return returnAndAssign(result, VolumeControlResult ::SUCCESS);
  }

  bool removeFromGroup(ptrdiff_t group, size_t channel,
                       VolumeControlResult *result = nullptr) noexcept {
    if (group >= CHANNELS) {
      return returnAndAssign(result, VolumeControlResult::HIGH_GROUP);
    }
    if (channel >= CHANNELS) {
      return returnAndAssign(result, VolumeControlResult::HIGH_CHANNEL);
    }
    ptrdiff_t &mapped = map_[channel];
    if (group < 0) {
      if (mapped < 0) {
        return returnAndAssign(result, VolumeControlResult::CHANNEL_NOT_MAPPED);
      }
    } else if (mapped != group) {
      return returnAndAssign(result, VolumeControlResult::CHANNEL_NOT_MAPPED);
    }
    map_[channel] = -1;
    return returnAndAssign(result, VolumeControlResult ::SUCCESS);
  }

  bool removeGroupChannels(ptrdiff_t group,
                           VolumeControlResult *result = nullptr) noexcept {
    if (group >= CHANNELS) {
      return returnAndAssign(result, VolumeControlResult::HIGH_GROUP);
    }
    for (size_t channel = 0; channel < CHANNELS; channel++) {
      ptrdiff_t &mapped = map_[channel];
      if (group < 0 || group == mapped) {
        mapped = -1;
      }
    }
    return returnAndAssign(result, VolumeControlResult ::SUCCESS);
  }

  ptrdiff_t getMaxAssignedChannel() const noexcept {
    ptrdiff_t i = CHANNELS;
    while (--i >= 0) {
      if (map_[i] >= 0) {
        return i;
      }
    }
    return -1;
  }

  ptrdiff_t getMaxAssignedGroup() const noexcept {
    ptrdiff_t maxAssigned = -1;
    for (size_t i = 0; i < CHANNELS; i++) {
      maxAssigned = std::max(maxAssigned, map_[i]);
    }
    return maxAssigned;
  }

  size_t getGroupChannels(size_t group) const noexcept {
    size_t count = 0;
    for (size_t channel = 0; channel < CHANNELS; channel++) {
      if (map_[channel] == group) {
        count++;
      }
    }
    return count;
  }

  ptrdiff_t getGroupChannel(size_t group, size_t idx) const noexcept {
    ptrdiff_t count = -1;
    for (size_t channel = 0; channel < CHANNELS; channel++) {
      if (map_[channel] == group) {
        if (++count == idx) {
          return channel;
        }
      }
    }
    return -1;
  }
};

template <typename T, size_t ICHANNELS, size_t OCHANNELS>
class GroupVolumeControl {
public:
  static constexpr size_t inputChannels = ICHANNELS;
  static constexpr size_t outputChannels = OCHANNELS;

  using InputMap = GroupMap<T, ICHANNELS>;
  using OutputMap = GroupMap<T, OCHANNELS>;
  using GroupMatrix = VolumeMatrix<T, ICHANNELS, OCHANNELS, 2>;

  GroupVolumeControl() { zeroAll(); }

  const InputMap &inputGroups() const noexcept { return inputGroups_; }
  InputMap &inputGroups() noexcept { return inputGroups_; }
  const OutputMap &outputGroups() const noexcept { return outputGroups_; }
  OutputMap &outputGroups() noexcept { return outputGroups_; }

  const GroupMatrix &groupMatrix() const { return matrix_; }

  void zeroAll() noexcept { matrix_.zero(); }

  VolumeControlResult setGroupVolume(size_t inputGroup, size_t outputGroup,
                                     T volume) {
    if (inputGroup >= ICHANNELS || outputGroup >= OCHANNELS) {
      return VolumeControlResult ::HIGH_GROUP;
    }
    matrix_.set(outputGroup, inputGroup, volume);
    return VolumeControlResult ::SUCCESS;
  }

  VolumeControlResult getGroupVolume(T &volume, size_t inputGroup,
                                     size_t outputGroup) const {
    if (inputGroup >= inputGroups_ || outputGroup >= outputGroups_) {
      return VolumeControlResult ::HIGH_GROUP;
    }
    volume = matrix_.get(outputGroup, inputGroup);
    return VolumeControlResult ::SUCCESS;
  }

  template <size_t I, size_t O, size_t A>
  VolumeControlResult apply(VolumeMatrix<T, I, O, A> &applyTo) const {
    ptrdiff_t maxInputChannel = inputGroups_.getMaxAssignedChannel();
    ptrdiff_t maxOutputChannel = outputGroups_.getMaxAssignedChannel();
    if (maxInputChannel >= applyTo.inputs ||
        maxOutputChannel >= applyTo.outputs) {
      return VolumeControlResult ::HIGH_CHANNEL;
    }
    applyTo.zero();
    size_t igChannels[ICHANNELS];
    size_t ogChannels[OCHANNELS];
    for (ptrdiff_t oGroup = 0; oGroup <= maxOutputChannel; oGroup++) {
      ogChannels[oGroup] = outputGroups_.getGroupChannels(oGroup);
    }
    for (ptrdiff_t iGroup = 0; iGroup <= maxInputChannel; iGroup++) {
      igChannels[iGroup] = inputGroups_.getGroupChannels(iGroup);
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
        T volume = matrix_.get(oGroup, iGroup);
        size_t i;
        if (igCount == 1) {
          for (i = 0; i < ogCount; i++) {
            setVolume(applyTo, outputGroups_.getGroupChannel(oGroup, i),
                        inputGroups_.getGroupChannel(iGroup, 0), volume);
          }
          continue;
        }
        if (ogCount == 1) {
          for (i = 0; i < igCount; i++) {
            setVolume(applyTo, outputGroups_.getGroupChannel(oGroup, 0),
                        inputGroups_.getGroupChannel(iGroup, i), volume);
          }
          continue;
        }
        for (i = 0; i < std::min(igCount, ogCount); i++) {
          setVolume(applyTo, outputGroups_.getGroupChannel(oGroup, i),
                      inputGroups_.getGroupChannel(iGroup, i), volume);
        }
        if (igCount > ogCount) {
          for (; i < igCount; i++) {
            setVolume(applyTo, outputGroups_.getGroupChannel(oGroup, i % ogCount),
                        inputGroups_.getGroupChannel(iGroup, i), volume);
          }
        }
      }
    }
    return VolumeControlResult ::SUCCESS;
  }

private:
  InputMap inputGroups_;
  OutputMap outputGroups_;
  GroupMatrix matrix_;

  template <size_t I, size_t O, size_t A>
  static void setVolume(VolumeMatrix<T, I, O, A> &applyTo, ptrdiff_t output, ptrdiff_t input, T volume)  {
    if (output >= 0 && input >= 0) {
      applyTo.set(output, input, volume);
    }
  }

};

} // namespace tdap
// namespace tdap

#endif /* TDAP_VALUE_VOLUME_MATRIX_GUARD */
