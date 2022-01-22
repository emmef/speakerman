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
#include <array>
#include <tdap/AlignedArray.h>
#include <tdap/Errors.hpp>
#include <tdap/IndexPolicy.hpp>
#include <tdap/Integration.hpp>

namespace tdap {

typedef int *nm;

/**
 * Defines and applies for each output in a set of OUTPUTS outputs what weight,
 * or "volume", is given to each input from a set of INPUT volumes. Outputs and
 * volumes contain samples of value-type T. For optimization, data is aligned to
 * ALIGN samples, where ALIGN is a power of two.
 *
 * @tparam T The type of samples used in volumes, outputs and weights.
 * @tparam INPUTS The number of volumes.
 * @tparam OUTPUTS The number of outputs.
 * @tparam ALIGN_BYTES The number of bytes to align to.
 */
template <typename T, size_t ALIGN_BYTES = 32> class VolumeMatrix {
  static_assert(std::is_floating_point<T>::value,
                "Expecting floating point type parameter");

  static_assert(Power2::constant::is(ALIGN_BYTES));
  static_assert((ALIGN_BYTES % sizeof(T)) == 0);
  static constexpr size_t ALIGN_ELEMENTS = ALIGN_BYTES / sizeof(T);
  static constexpr T eps = 1e-8;

  static constexpr size_t alignedBlocks(size_t inputs) {
    return ((inputs - 1) / ALIGN_ELEMENTS + 1);
  }

  static constexpr size_t roundUp(size_t inputs) {
    return alignedBlocks(inputs) * ALIGN_ELEMENTS;
  }

  static constexpr T *aligned(T *raw) {
    ptrdiff_t pos = raw - static_cast<T *>(nullptr);
    ptrdiff_t offs = roundUp(pos) - pos;
    return raw + offs;
  }

  static constexpr size_t validateGetInputs(size_t inputs, size_t outputs) {
    if (inputs == 0 || outputs == 0) {
      throw std::invalid_argument("Inputs and outputs must be positive");
    }
    size_t blocks = alignedBlocks(inputs);
    if (!Count<T>::product(blocks, ALIGN_ELEMENTS)) {
      throw std::invalid_argument("Number if volumes too big");
    }
    size_t alignedInputs = blocks * ALIGN_ELEMENTS;
    if (!Count<T>::product(alignedInputs, outputs)) {
      throw std::invalid_argument(
          "Combination of volumes and outputs too large");
    }
    return inputs;
  }

  inline T *volumes(size_t output) {
    return std::assume_aligned<ALIGN_BYTES>(vol +
                                            in_block * ALIGN_ELEMENTS * output);
  }

  inline const T *volumes(size_t output) const {
    return std::assume_aligned<ALIGN_BYTES>(vol + in_block * output);
  }

  size_t ins;
  size_t in_block;
  size_t outs;
  T *data;
  T *vol;

  void unsafeCopy(const VolumeMatrix &source) const {
    std::copy(source.vol, source.vol + (in_block * outs * ALIGN_ELEMENTS), vol);
  }

public:
  VolumeMatrix(size_t inputs, size_t outputs)
      : ins(validateGetInputs(inputs, outputs)), in_block(alignedBlocks(ins)),
        outs(outputs),
        data(new T[in_block * ALIGN_ELEMENTS * (outputs + 1) + 1]),
        vol(aligned(data)) {
    // An extra row of input volumes is included to copy non-aligned input
    // arrays to
  }

  /**
   * Creates a volume matrix with the same values als the source, including
   * epsilon, the minimum representable volume.
   * @param source The source matrix to copy.
   */
  explicit VolumeMatrix(const VolumeMatrix &source)
      : VolumeMatrix(source.ins, source.outs) {
    unsafeCopy(source);
  }

  /**
   * Creates a volume matrix with the same values als the source, including
   * epsilon, the minimum representable volume.
   * @param source The source matrix to copy.
   */
  template<size_t ALIGN>
  explicit VolumeMatrix(const VolumeMatrix<T, ALIGN> &source)
      : VolumeMatrix(source.ins, source.outs) {
    unsafeCopy(source);
  }

  VolumeMatrix(VolumeMatrix &&moved)
      : ins(moved.ins), in_block(moved.in_block), outs(moved.outs),
        data(moved.data), vol(moved.vol) {
    moved.data = nullptr;
    moved.vol = nullptr;
  }

  ~VolumeMatrix() {
    if (data) {
      delete[] data;
      data = nullptr;
    }
    vol = nullptr;
  }

  template<size_t ALIGN>
  bool assign(const VolumeMatrix<T, ALIGN> &source) {
    if (ins == source.ins &&outs = source.outs) {
      unsafeCopy(source);
      return true;
    }
    return false;
  }

  /**
   * Set volumes so that all outputs are mapped to the corresponding
   * volumes with the given volume, that defaults to unity. For matrices with a
   * different number of volumes and outputs, the smallest is used: al others
   * weights are zero.
   * @param volume The volume for mapped channels, that defaults to unity.
   */
  void identity(T volume = 1.0) noexcept {
    const size_t MIN_CHANNELS = std::min(ins, outs);
    zero();
    T flushedScale = flushToZero(volume);
    for (size_t output = 0; output < MIN_CHANNELS; output++) {
      volumes(output)[output] = flushedScale;
    }
  }

  /**
   * Set volumes so that all outputs are mapped to the corresponding
   * volumes with the given volume, that defaults to unity. For matrices with a
   * different number of volumes and outputs, the largest is used and the
   * smaller dimension is "wrapped" or repeated.
   * @param volume The volume for mapped channels, that defaults to unity.
   */
  void identityWrapped(T scale = 1.0) noexcept {
    const size_t MAX_CHANNELS = std::max(ins, outs);
    zero();
    T flushedScale = flushToZero(scale);
    for (size_t output = 0; output < MAX_CHANNELS; output++) {
      volumes(output % outs)[output % ins] = flushedScale;
    }
  }

  /**
   * sets all volumes to zero.
   */
  void zero() noexcept {
    T *pT = std::assume_aligned<ALIGN_BYTES>(vol);
    std::fill(pT, pT + in_block * outs * ALIGN_ELEMENTS, 0);
  }

  /**
   * Sets all volumes that are less than epsilon (absolute) to zero.
   * (This should never be necessary, but hiding this as an internal detail is
   * rather strict).
   */
  void flushAllToZero() noexcept {
    for (size_t output = 0; output < outs; output++) {
      T *p = volumes(output);
      for (size_t input = 0; input < ins; input++) {
        p[input] = flushToZero(p[input]);
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
    if (output < outs && input < ins) {
      volumes(output)[input] = flushToZero(volume);
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
    if (output < outs && input < ins) {
      return volumes(output)[input];
    }
    return Error::setErrorReturn(Error::BOUND, (T)0);
  }

  /**
   * Set all weights for volumes to all outputs to the same value: volume. If
   * volumes has an absolute value smaller than epsilon (getEpsilon()), it will
   * be considered zero.
   * @param volume The volume.
   */
  void setAll(T volume) noexcept {
    T flushedToZero = flushToZero(volume);
    T *pT = volumes(0);
    std::fill(pT, pT + in_block * outs * ALIGN_ELEMENTS, flushedToZero);
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
    if (ins != source.ins || outs != source.outs) {
      throw std::invalid_argument("Approach matrix must have same dimensions");
    }
    for (size_t output = 0; output < outs; output++) {
      for (size_t input = 0; input < ins; input++) {
        approachValue(volumes(output)[input], source.volumes(output)[input],
                      coefficients);
      }
    }
  }

  void applyAlignedInputUnsafe(T *__restrict out,
                               const T *__restrict in) const {
    const size_t block = in_block * ALIGN_ELEMENTS;
    T *input = std::assume_aligned<ALIGN_ELEMENTS>(in);
    T *v1 = std::assume_aligned<ALIGN_BYTES>(volumes(0));
    for (size_t output = 0; output < outs; output++, v1 += block) {
      out[output] = v1[0] * input[0];
      for (size_t i = 1; i < ins; i++) {
        out[output] += v1[i] * input[i];
      }
    }
  }

  template <size_t INS, size_t OUTS>
  void applyAlignedInputUnsafeFixed(T *__restrict out,
                                    const T *__restrict in) const {
    const size_t block = in_block * ALIGN_ELEMENTS;
    const T *input = std::assume_aligned<ALIGN_ELEMENTS>(in);
    const T *v1 = std::assume_aligned<ALIGN_BYTES>(volumes(0));
    for (size_t output = 0; output < OUTS; output++, v1 += block) {
      out[output] = v1[0] * input[0];
      for (size_t i = 1; i < INS; i++) {
        out[output] += v1[i] * input[i];
      }
    }
  }

  /**
   * Apply all volumes for all input channels in volumes to all outputs and
   * return the result. All outputs also get added the seed value.
   * @tparam A alignment of input frame.
   * @param inputs The input frame.
   * @param seed The seed value, added to all outputs.
   * @return The output frame.
   */
  template <size_t O, size_t I, size_t A>
  void apply(AlignedArray<T, O, A> &result,
             const AlignedArray<T, I, A> &input) const {
    static_assert((A % ALIGN_BYTES) == 0);
    if (I != ins || O != outs) {
      throw std::invalid_argument(
          "VolumeMatrix::apply: input and output sizes do not match");
    }
    applyAlignedInputUnsafeFixed<I, O>(result.begin(), input.begin());
  }

  /**
   * Apply all volumes for all input channels in volumes to all outputs and
   * return the result. All outputs also get added the seed value.
   * @tparam A alignment of input frame.
   * @param inputs The input frame.
   * @param seed The seed value, added to all outputs.
   * @return The output frame.
   */
  template <size_t O, size_t I>
  void apply(std::array<T, O> &result, const std::array<T, I> &input) const {
    if (I != ins || O != outs) {
      throw std::invalid_argument(
          "VolumeMatrix::apply: input and output sizes do not match");
    }
    if (((input.begin() - static_cast<T *>(nullptr)) % ALIGN_BYTES) == 0) {
      applyAlignedInputUnsafeFixed<I, O>(result.begin(), input);
    } else {
      std::copy(input.begin(), input.begin() + ins, volumes(outs));
      applyAlignedInputUnsafeFixed<I, O>(result.begin(), volumes(outs));
    }
  }

  /**
   * Apply all volumes for all input channels in volumes to all outputs. All
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
  bool apply(T *__restrict outputs, const T *__restrict inputs) const {
    if (!outputs || !inputs) {
      return Error::setErrorReturn(Error::NILL);
    }

    if (((inputs - static_cast<T *>(nullptr)) % ALIGN_ELEMENTS) == 0) {
      applyAlignedInputUnsafe(outputs, inputs);
    } else {
      std::copy(inputs, inputs + ins, volumes(outs));
      applyAlignedInputUnsafe(outputs, volumes(outs));
    }
    return true;
  }

private:
  inline T flushToZero(T volume) const noexcept {
    return fabs(volume) > eps ? volume : 0;
  }

  inline void
  approachValue(const T &out, T sourceValue,
                const IntegrationCoefficients<T> &coefficients) const {
    out += flushToZero(coefficients.template getIntegrated(sourceValue, out) -
                       out);
  }
};

template <typename T, size_t ALIGN_BYTES = 32>
class IntegratedVolumeMatrix : public VolumeMatrix<T, ALIGN_BYTES> {
  VolumeMatrix<T, ALIGN_BYTES> toFollow;
  IntegrationCoefficients<T> integration;

public:
  IntegratedVolumeMatrix(size_t inputs, size_t outputs,
                         double integrationSamples)
      : VolumeMatrix<T, ALIGN_BYTES>(inputs, outputs),
        toFollow(inputs, outputs),
        integration(IntegrationCoefficients<T>(integrationSamples)) {
  }

  /**
   * Creates a volume matrix with the same values als the source, including
   * epsilon, the minimum representable volume.
   * @param source The source matrix to copy.
   */
  template<size_t ALIGN>
  explicit IntegratedVolumeMatrix(const VolumeMatrix<T, ALIGN> &source)
      : VolumeMatrix<T, ALIGN_BYTES>(source), toFollow(source) {
  }

  IntegratedVolumeMatrix(IntegratedVolumeMatrix &&moved) = default;

  void setIntegrationSamples(double integrationSamples) {
    integration.setCharacteristicSamples(integrationSamples);
  }

  template <size_t ALIGN>
  bool assign(const VolumeMatrix<T, ALIGN> *source) {
    return toFollow.template assign(source);
  }

  void approach() {
    approach(toFollow, integration);
  }
};

} // namespace tdap

#endif // TDAP_M_VOLUME_MATRIX_HPP
