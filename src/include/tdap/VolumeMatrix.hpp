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

  const InputVolumes &getOUtputMix(size_t idx) const {
    return matrix[IndexPolicy::method(idx, OUTPUTS)];
  }

  VolumeMatrix(T value) { set_all(value); }

  VolumeMatrix() { identity(); }

  void identity(T scale = 1.0) noexcept {
    zero();
    T flushedScale = flushToZero(scale);
    for (size_t i = 0; i < MIN_CHANNELS; i++) {
      matrix[i][i] = flushedScale;
    }
  }

  void identityWrapped(T scale = 1.0) noexcept {
    zero();
    T flushedScale = flushToZero(scale);
    for (size_t output = 0; output < MAX_CHANNELS; output++) {
      matrix[output % OUTPUTS][output % INPUTS] = flushedScale;
    }
  }

  void zero() noexcept {
    for (size_t output = 0; output < OUTPUTS; output++) {
      for (size_t input = 0; input < INPUTS; input++) {
        matrix[output][input] = 0;
      }
    }
  }

  void flushAllToZero() noexcept {
    for (size_t output = 0; output < OUTPUTS; output++) {
      for (size_t input = 0; input < INPUTS; input++) {
        matrix[output][input] = flushToZero(matrix[output][input]);
      }
    }
  }

  void set(size_t output, size_t input, T volume, T epsilon = 1e-6) {
    int x = matrix[IndexPolicy::method(output, OUTPUTS)]
                  [IndexPolicy::method(input, INPUTS)] = flushToZero(volume);
  }

  void setAll(T volume, T epsilon = 1e-6) noexcept {
    T flushedToZero = flushToZero(volume);
    for (size_t output = 0; output < OUTPUTS; output++) {
      for (size_t input = 0; input < INPUTS; input++) {
        matrix[output][input] = flushedToZero;
      }
    }
  }

  void approach(const VolumeMatrix &source,
                const IntegrationCoefficients<T> &coefficients, T epsilon) {
    for (size_t output = 0; output < OUTPUTS; output++) {
      for (size_t input = 0; input < INPUTS; input++) {
        T sourceValue = source.matrix[output][input];
        T &out = matrix[output][input];
        if (out == 0 && sourceValue > epsilon) {
          out = sourceValue;
        }
        else {
          out = flushToZero(coefficients.getIntegrated(sourceValue, out));
        }
      }
    }
  }

  template <size_t A>
  AlignedFrame<T, OUTPUTS, A> apply(const AlignedFrame<T, INPUTS, A> &inputs) const {
    AlignedFrame<T, OUTPUTS, A> outputs;
    for (size_t output = 0; output < OUTPUTS; output++) {
      outputs[output] = inputs.dot(matrix[output]);
    }
    return outputs;
  }

  template <size_t A>
  AlignedFrame<T, OUTPUTS, A>
  applySeeded(const AlignedFrame<T, INPUTS, A> &inputs, T seed) const {
    AlignedFrame<T, OUTPUTS, A> outputs;
    for (size_t output = 0; output < OUTPUTS; output++) {
      outputs[output] = inputs.dotSeeded(matrix[output], seed);
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
  InputVolumes matrix[OUTPUTS];
  T epsilon = 1e-6;

  T flushToZero(T volume) const noexcept {
    return fabs(volume) > epsilon ? volume : 0;
  }

  template <size_t A, size_t COMMON_ALIGN>
  void seededApply(AlignedFrame<T, OUTPUTS, A> &__restrict outputs,
                   const AlignedFrame<T, INPUTS, A> &__restrict inputs,
                   T seed) const {
    AlignedFrame<T, OUTPUTS, A> &out =
        *assume_aligned<COMMON_ALIGN, AlignedFrame<T, OUTPUTS, A>>(
            &outputs);
    const AlignedFrame<T, INPUTS, A> &in =
        *assume_aligned<COMMON_ALIGN, const AlignedFrame<T, INPUTS, A>>(
            &inputs);
    for (size_t output = 0; output < OUTPUTS; output++) {
      out[output] = in.dotSeeded(matrix[output], seed);
    }
  }

  template <size_t A, size_t COMMON_ALIGN>
  void unSeededApply(AlignedFrame<T, OUTPUTS, A> &__restrict outputs,
             const AlignedFrame<T, INPUTS, A> &__restrict inputs) const {

    AlignedFrame<T, OUTPUTS, A> &out =
        *assume_aligned<COMMON_ALIGN, AlignedFrame<T, OUTPUTS, A>>(
            &outputs);
    const AlignedFrame<T, INPUTS, A> &in =
        *assume_aligned<COMMON_ALIGN, const AlignedFrame<T, INPUTS, A>>(
            &inputs);
    for (size_t output = 0; output < OUTPUTS; output++) {
      out[output] = in.dot(matrix[output]);
    }
  }
};

template <typename T, size_t INPUTS, size_t OUTPUTS, size_t ALIGN = 4>
struct VolumeMatrixWithSmoothControl {
  using Matrix = VolumeMatrix<T, INPUTS, OUTPUTS, ALIGN>;

  const Matrix &matrix() const noexcept {
    return actualVolume;
  }

  VolumeMatrixWithSmoothControl() {
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

  void approach() {
    actualVolume.approach(userVolume, integration);
  }

  void configure(double sampleRate, double rc, Matrix initialVolumes) {
    integration.setCharacteristicSamples(sampleRate * rc);
    userVolume.identity(initialVolumes);
    actualVolume.set_all(0);
  }

  void setVolume(const Matrix &newVolumes) {
    userVolume = newVolumes;
  }

private:
  Matrix actualVolume;
  Matrix userVolume;
  IntegrationCoefficients<double> integration;

};

} // namespace tdap

#endif /* TDAP_VALUE_VOLUME_MATRIX_GUARD */
