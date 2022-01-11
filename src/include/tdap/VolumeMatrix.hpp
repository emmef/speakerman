#ifndef TDAP_M_VOLUME_MATRIX_HPP
#define TDAP_M_VOLUME_MATRIX_HPP
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

#include <algorithm>
#include <tdap/AlignedFrame.hpp>
#include <tdap/Errors.hpp>
#include <tdap/IndexPolicy.hpp>
#include <tdap/Integration.hpp>

namespace tdap {

/**
 * Defines and applies for each output in a set of OUTPUTS outputs what weight,
 * or "volume", is given to each input from a set of INPUT inputs. Outputs and
 * inputs contain samples of value-type T. For optimization, data is aligned to
 * ALIGN samples, where ALIGN is a power of two.
 *
 * @tparam T The type of samples used in inputs, outputs and weights.
 * @tparam INPUTS The number of inputs.
 * @tparam OUTPUTS The number of outputs.
 * @tparam ALIGN The number of samples to align to.
 */
template <typename T, size_t INPUTS, size_t OUTPUTS, size_t ALIGN = 4>
class VolumeMatrix {
  static_assert(std::is_floating_point<T>::value,
                "Expecting floating point type parameter");
  static_assert(Power2::constant::is(ALIGN), "ALIGN is not a power of 2.");
  static_assert(Count<T>::valid_positive(INPUTS), "Invalid INPUTS parameter");
  static_assert(Count<T>::valid_positive(OUTPUTS), "Invalid OUTPUTS parameter");

public:
  using InputVolumes = AlignedFrame<T, INPUTS, ALIGN>;
  using InputFrame = AlignedFrame<T, INPUTS, ALIGN>;
  using OutputFrame = AlignedFrame<T, OUTPUTS, ALIGN>;

  static constexpr size_t inputs = INPUTS;
  static constexpr size_t outputs = OUTPUTS;
  static constexpr size_t alignment = ALIGN;

  /**
   * Creates a volume matrix with all weights set to value.
   * @param value The value of all weights.
   */
  explicit VolumeMatrix(T value) { setAll(value); }

  /**
   * Creates a volume matrix where all outputs are mapped to the corresponding
   * inputs with unit volume.
   * @see identity
   */
  explicit VolumeMatrix() { identity(); }

  /**
   * Creates a volume matrix with the same values als the source, including
   * epsilon, the minimum representable volume.
   * @param source The source matrix to copy.
   */
  explicit VolumeMatrix(const VolumeMatrix &source) { operator=(source); }

  /**
   * Creates a volume matrix with the same values als the source, including
   * epsilon, the minimum representable volume.
   * Values from the source that are outside the dimensions of this matrix are
   * ignored. If the source is smaller in inputs, outputs or both, all volumes
   * outside the source boundaries are set to zero.
   * @param source The source matrix to copy.
   */
  template <typename V, size_t IN, size_t OUT, size_t AL>
  VolumeMatrix(const VolumeMatrix<V, IN, OUT, AL> &source) {
    operator=(source);
  }

  /**
   * Get the minimum (absolute) representable volume. Volumes below this value
   * will be set to zero.
   * @return The value of epsilon.
   */
  T getEpsilon() const noexcept { return eps; }

  /**
   * Sets the minimum (absolute) representable volume. If the value is
   * different, all volumes below this value will be set to zero.
   * @return The actually set value of epsilon.
   */
  T setEpsilon(T epsilon) noexcept {
    T ep = std::clamp(epsilon, 0.0, 0.1);
    if (ep != epsilon) {
      epsilon = ep;
      flushAllToZero();
    }
    return epsilon;
  }

  /**
   * @return whether epsilon is copied on assignments from other matrices.
   */
  bool copyEpsilon() const noexcept { return copy; }

  /**
   * Sets whether epsilon is copied on assignments from other matrices.
   */
  void setCopyEpsilon(bool copyEpsilon) noexcept { copy = copyEpsilon; }

  /**
   * Gets the input-weights for the specified output.
   * @param output The output number.
   * @return The input weights.
   * @throw std::invalid_argument if output is invalid.
   */
  const InputVolumes &getOUtputMix(size_t output) const {
    return volumes[IndexPolicy::method(output, OUTPUTS)];
  }

  /**
   * Copies all values from the source, including epsilon, the minimum
   * representable volume.
   * @param source The source matrix to copy.
   */
  VolumeMatrix &operator=(const VolumeMatrix &source) {
    if (copy || source.eps >= eps) {
      for (size_t out = 0; out < OUTPUTS; out++) {
        for (size_t in = 0; in < INPUTS; in++) {
          volumes[out][in] = source.volumes[out][in];
        }
      }
      if (copy) {
        eps = source.eps;
      }
    } else {
      for (size_t out = 0; out < OUTPUTS; out++) {
        for (size_t in = 0; in < INPUTS; in++) {
          volumes[out][in] = flushToZero(source.volumes[out][in]);
        }
      }
    }
  }

  /**
   * Copies values from the source, including epsilon, the minimum representable
   * volume. Values from the source that are outside the dimensions of this
   * matrix are ignored. If the source is smaller in inputs, outputs or both,
   * all volumes outside the source boundaries are set to zero.
   * @param source The source matrix to copy.
   */
  template <typename V, size_t IN, size_t OUT, size_t AL>
  VolumeMatrix &operator=(const VolumeMatrix<V, IN, OUT, AL> &source) {
    if (copy || source.eps >= eps) {
      for (size_t out = 0; out < OUTPUTS; out++) {
        for (size_t in = 0; in < INPUTS; in++) {
          volumes[out][in] = out < OUT && in < IN ? source.get(out, in) : 0;
        }
      }
      if (copy) {
        setEpsilon(source.getEpsilon());
      }
    } else {
      for (size_t out = 0; out < OUTPUTS; out++) {
        for (size_t in = 0; in < INPUTS; in++) {
          volumes[out][in] =
              out < OUT && in < IN ? flushToZero(source.get(out, in)) : 0;
        }
      }
    }
  }

  /**
   * Set volumes so that all outputs are mapped to the corresponding
   * inputs with the given volume, that defaults to unity. For matrices with a
   * different number of inputs and outputs, the smallest is used: al others
   * weights are zero.
   * @param volume The volume for mapped channels, that defaults to unity.
   */
  void identity(T volume = 1.0) noexcept {
    static constexpr size_t MIN_CHANNELS = std::min(INPUTS, OUTPUTS);
    zero();
    T flushedScale = flushToZero(volume);
    for (size_t i = 0; i < MIN_CHANNELS; i++) {
      volumes[i][i] = flushedScale;
    }
  }

  /**
   * Set volumes so that all outputs are mapped to the corresponding
   * inputs with the given volume, that defaults to unity. For matrices with a
   * different number of inputs and outputs, the largest is used and the smaller
   * dimension is "wrapped" or repeated.
   * @param volume The volume for mapped channels, that defaults to unity.
   */
  void identityWrapped(T scale = 1.0) noexcept {
    static constexpr size_t MAX_CHANNELS = std::max(INPUTS, OUTPUTS);
    zero();
    T flushedScale = flushToZero(scale);
    for (size_t output = 0; output < MAX_CHANNELS; output++) {
      volumes[output % OUTPUTS][output % INPUTS] = flushedScale;
    }
  }

  /**
   * sets all volumes to zero.
   */
  void zero() noexcept {
    for (size_t output = 0; output < OUTPUTS; output++) {
      for (size_t input = 0; input < INPUTS; input++) {
        volumes[output][input] = 0;
      }
    }
  }

  /**
   * Sets all volumes that are less than epsilon (absolute) to zero.
   * (This should never be necessary, but hiding this as an internal detail is
   * rather strict).
   */
  void flushAllToZero() noexcept {
    for (size_t output = 0; output < OUTPUTS; output++) {
      for (size_t input = 0; input < INPUTS; input++) {
        volumes[output][input] = flushToZero(volumes[output][input]);
      }
    }
  }

  /**
   * Sets the volume of the input for the output. Volumes with an absolute value
   * smaller than epsilon (getEpsilon()) will be set to zero. The method returns
   * true, but if either input or output are invalid, the method returns false
   * and sets Error to Error::BOUND.
   * @param output The output.
   * @param input The input.
   * @param volume The volume.
   * @return whether setting volume was successful.
   */
  bool set(size_t output, size_t input, T volume) noexcept {
    if (output < OUTPUTS && input < INPUTS) {
      volumes[output][input] = flushToZero(volume);
      return true;
    }
    return Error::setErrorReturn(Error::BOUND);
  }

  /**
   * Gets the volume of the input for the output. The method returns true, but
   * if either input or output are invalid, the method returns false and sets
   * Error to Error::BOUND.
   * @param output The output.
   * @param input The input.
   * @return whether setting volume was successful.
   */
  T get(size_t output, size_t input) const noexcept {
    if (output < OUTPUTS && input < INPUTS) {
      return volumes[output][input];
    }
    return Error::setErrorReturn(Error::BOUND, (T)0);
  }

  /**
   * Set all weights for inputs to all outputs to the same value: volume. If
   * volumes has an absolute value smaller than epsilon (getEpsilon()), it will
   * be considered zero.
   * @param volume The volume.
   */
  void setAll(T volume) noexcept {
    T flushedToZero = flushToZero(volume);
    for (size_t output = 0; output < OUTPUTS; output++) {
      for (size_t input = 0; input < INPUTS; input++) {
        volumes[output][input] = flushedToZero;
      }
    }
  }

  /**
   * Approach the volumes of the source matrix, using the specified integration.
   * After an infinite number of invocations, this and the source matrix should
   * be similar.
   * @param source The source matrix whose volumes to aproach.
   * @param coefficients The integration coefficients used.
   */
  void approach(const VolumeMatrix &source,
                const IntegrationCoefficients<T> &coefficients) noexcept {
    if (copy) {
      eps = source.eps;
    }
    for (size_t output = 0; output < OUTPUTS; output++) {
      for (size_t input = 0; input < INPUTS; input++) {
        T sourceValue = source.volumes[output][input];
        T &out = volumes[output][input];
        approachValue(out, sourceValue, coefficients);
      }
    }
  }

  /**
   * Approach the volumes of the source matrix, using the specified integration.
   * After an infinite number of invocations, this and the source matrix should
   * be similar. Values outside the dimensions of this matrix will be ignored,
   * while values that are outside the source matrix will be considered zero.
   * @param source The source matrix whose volumes to aproach.
   * @param coefficients The integration coefficients used.
   */
  template <typename V, size_t IN, size_t OUT, size_t AL>
  void approach(const VolumeMatrix<V, IN, OUT, AL> &source,
                const IntegrationCoefficients<T> &coefficients) {
    if (copy) {
      eps = source.getEpsilon();
    }
    for (size_t output = 0; output < OUTPUTS; output++) {
      for (size_t input = 0; input < INPUTS; input++) {
        T sourceValue =
            output < OUT && input < IN ? source.get(output, input) : 0;
        T &out = volumes[output][input];
        approachValue(out, sourceValue, coefficients);
      }
    }
  }

  /**
   * Apply all volumes for all input channels in inputs to all outputs and
   * return the result.
   * @tparam A alignment of input frame.
   * @param inputs The input frame.
   * @return The output frame.
   */
  template <size_t A>
  AlignedFrame<T, OUTPUTS, A>
  apply(const AlignedFrame<T, INPUTS, A> &inputs) const noexcept {
    AlignedFrame<T, OUTPUTS, A> outputs;
    for (size_t output = 0; output < OUTPUTS; output++) {
      outputs[output] = inputs.dot(volumes[output]);
    }
    return outputs;
  }

  /**
   * Apply all volumes for all input channels in inputs to all outputs and
   * return the result. All outputs also get added the seed value.
   * @tparam A alignment of input frame.
   * @param inputs The input frame.
   * @param seed The seed value, added to all outputs.
   * @return The output frame.
   */
  template <size_t A>
  AlignedFrame<T, OUTPUTS, A>
  applySeeded(const AlignedFrame<T, INPUTS, A> &inputs, T seed) const noexcept {
    AlignedFrame<T, OUTPUTS, A> outputs;
    for (size_t output = 0; output < OUTPUTS; output++) {
      outputs[output] = inputs.dotSeeded(volumes[output], seed);
    }
    return outputs;
  }

  /**
   * Apply all volumes for all input channels in inputs to all outputs.
   * @tparam A alignment of input and output frames.
   * @param outputs The output frame.
   * @param inputs The input frame.
   */
  template <size_t A>
  void apply(AlignedFrame<T, OUTPUTS, A> &__restrict outputs,
             const AlignedFrame<T, INPUTS, A> &__restrict inputs) const
      noexcept {

    unSeededApply<A, std::min(A, ALIGN)>(outputs, inputs);
  }

  /**
   * Apply all volumes for all input channels in inputs to all outputs. All
   * outputs also get added the seed value.
   * @tparam A alignment of input frame.
   * @param outputs The output frame.
   * @param inputs The input frame.
   * @param seed The seed value, added to all outputs.
   */
  template <size_t A>
  void applySeeded(AlignedFrame<T, OUTPUTS, A> &__restrict outputs,
                   const AlignedFrame<T, INPUTS, A> &__restrict inputs,
                   T seed) const noexcept {
    seededApply<A, std::min(A, ALIGN)>(outputs, inputs, seed);
  }

  /**
   * Apply all volumes for all input channels in inputs to all outputs. All
   * outputs also get added the seed value, that defaults to zero.
   * This method is potentially dangerous as no check is performed whether the
   * arrays are actually big enough. If the arrays are NIL, the method retruns
   * false and Error is set to Error::NIL.
   * @tparam A alignment of input frame.
   * @param outputs The output frame.
   * @param inputs The input frame.
   * @param seed The seed value, added to all outputs.
   * @return True if both input and output are not null and volumes are applied.
   */
  template <size_t ALIGN_SAMPLES = 1>
  bool apply(T *__restrict outputs, const T *__restrict inputs,
             T seed = 0) const noexcept {
    if (!outputs || !inputs) {
      return Error::setErrorReturn(Error::NILL);
    }
    T *out = assume_aligned<ALIGN_SAMPLES * sizeof(T), T *>(out);
    const T *in = assume_aligned<ALIGN_SAMPLES * sizeof(T), const T *>(out);

    for (size_t o = 0; o < OUTPUTS; o++) {
      T sum = seed;
      for (size_t i = 0; i < INPUTS; i++) {
        sum += in[i] * volumes[o][i];
      }
      out[o] = sum;
    }
    return true;
  }

private:
  InputVolumes volumes[OUTPUTS];
  T eps = 1e-6;
  bool copy = true;

  inline T flushToZero(T volume) const noexcept {
    return eps == 0 || fabs(volume) > eps ? volume : 0;
  }

  template <size_t A, size_t COMMON_ALIGN>
  void seededApply(AlignedFrame<T, OUTPUTS, A> &__restrict outputs,
                   const AlignedFrame<T, INPUTS, A> &__restrict inputs,
                   T seed) const noexcept {
    AlignedFrame<T, OUTPUTS, A> &out =
        *assume_aligned<COMMON_ALIGN, AlignedFrame<T, OUTPUTS, A>>(&outputs);
    const AlignedFrame<T, INPUTS, A> &in =
        *assume_aligned<COMMON_ALIGN, const AlignedFrame<T, INPUTS, A>>(
            &inputs);
    for (size_t output = 0; output < OUTPUTS; output++) {
      out[output] = in.dotSeeded(volumes[output], seed);
    }
  }

  template <size_t A, size_t COMMON_ALIGN>
  void unSeededApply(AlignedFrame<T, OUTPUTS, A> &__restrict outputs,
                     const AlignedFrame<T, INPUTS, A> &__restrict inputs) const
      noexcept {

    AlignedFrame<T, OUTPUTS, A> &out =
        *assume_aligned<COMMON_ALIGN, AlignedFrame<T, OUTPUTS, A>>(&outputs);
    const AlignedFrame<T, INPUTS, A> &in =
        *assume_aligned<COMMON_ALIGN, const AlignedFrame<T, INPUTS, A>>(
            &inputs);
    for (size_t output = 0; output < OUTPUTS; output++) {
      out[output] = in.dot(volumes[output]);
    }
  }

  inline void
  approachValue(const T &out, T sourceValue,
                const IntegrationCoefficients<T> &coefficients) const noexcept {
    if (eps == 0) {
      coefficients.integrate(sourceValue, out);
    } else if (out == 0 && fabs(sourceValue) > eps) {
      out = sourceValue;
    } else {
      T t = coefficients.getIntegrated(sourceValue, out);
      out = fabs(t) > eps ? t : 0;
    }
  }
};

/**
 * Manages a target volume matrix and an actual volume matrix, that approaches
 * the target marix on each call to approach with a certain integration
 * coefficient.
 * @see VolumeMatrix
 * @tparam T The type of sample.
 * @tparam INPUTS The number of inputs.
 * @tparam OUTPUTS The number of outputs.
 * @tparam ALIGN The alignment in samples.
 */
template <typename T, size_t INPUTS, size_t OUTPUTS, size_t ALIGN = 4>
struct SmoothVolumeMatrix {
  using Matrix = VolumeMatrix<T, INPUTS, OUTPUTS, ALIGN>;

  /**
   * Returns the actual volume matrix that can be used, "applied", to calculate
   * outputs from inputs.
   * @see VolumeMatrix.apply()
   * @return the volume matrix
   */
  const Matrix &volumes() const noexcept { return vm; }

  /**
   * Returns the target volume matrix. The volume matrix returned by volumes()
   * will approach this matrix more on each consequent call to approach().
   * @see VolumeMatrix.apply
   * @see approach()
   * @return the volume matrix
   */
  Matrix &target() noexcept { return tm; }
  const Matrix &target() const noexcept { return tm; }

  /**
   * Returns the integration coefficients, used for the approach() method.
   * @see IntegrationCoefficients
   * @return the integration coefficients.
   */
  IntegrationCoefficients<T> &integration() noexcept { return coeffs; }
  const IntegrationCoefficients<T> &integration() const noexcept {
    return coeffs;
  }

  /**
   * Makes the volume matrix approach the target volume matrix, using the set
   * integration coefficients.
   */
  void approach() { vm.approach(tm, coeffs); }

  /**
   * Sets the volume matrix to the same values as the target matrix immediately.
   */
  void adopt() { vm = tm; }

private:
  Matrix vm;
  Matrix tm;
  IntegrationCoefficients<double> coeffs;
};

} // namespace tdap

#endif // TDAP_M_VOLUME_MATRIX_HPP
