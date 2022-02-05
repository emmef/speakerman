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
template <typename T, class D, size_t ALIGN_BYTES = 32> class VolumeMatrix {
  static_assert(std::is_floating_point<T>::value,
                "Expecting floating point type parameter");

  static_assert(Power2::constant::is(ALIGN_BYTES));
  static_assert((ALIGN_BYTES % sizeof(T)) == 0);

protected:
  /**
   * Returns the number of elements to represent a vector of inputs, where the
   * size of the elements is a multiple of ALIGN_BYTES.
   * @param inputs The number of inputs.
   * @return The number of elements, representing an aligned number of bytes.
   */
  static constexpr size_t alignedInputs(size_t inputs) {
    const size_t ALIGN_ELEMENTS = ALIGN_BYTES / sizeof(T);
    return ((inputs - 1) / ALIGN_ELEMENTS + 1) * ALIGN_ELEMENTS;
  }

  static constexpr size_t neededCapacity(size_t inputs, size_t outputs) {
    return validateGetInputs(inputs, outputs) * (outputs + 1);
  }

  static constexpr T eps = 1e-8;

  static constexpr T *aligned(T *raw) {
    ptrdiff_t pos = raw - static_cast<T *>(nullptr);
    ptrdiff_t offs = alignedInputs(pos) - pos;
    return raw + offs;
  }

  static constexpr size_t validateGetInputs(size_t inputs, size_t outputs) {
    if (inputs == 0 || outputs == 0 || Count<T>::max() / 2 < inputs ||
        !Count<T>::product(alignedInputs(inputs), outputs)) {
      return 0;
    } else {
      return inputs;
    }
  }

  static constexpr size_t extraUnalignedCapacity() {
    if (__STDCPP_DEFAULT_NEW_ALIGNMENT__ >= ALIGN_BYTES) {
      return 0;
    }
    return ALIGN_BYTES - __STDCPP_DEFAULT_NEW_ALIGNMENT__;
  }

  inline size_t ins() const {
    return static_cast<const D *>(this)->getInputs();
  }

  inline size_t outs() const {
    return static_cast<const D *>(this)->getOutputs();
  }

  inline size_t algn_ins() const {
    return static_cast<const D *>(this)->getAlignedInputs();
  }

  inline T *volumes(size_t output) {
    return std::assume_aligned<ALIGN_BYTES>(static_cast<D *>(this)->volumeData() +
                                            algn_ins() * output);
  }

  inline const T *volumes(size_t output) const {
    return std::assume_aligned<ALIGN_BYTES>(
        static_cast<const D *>(this)->volumeData() + algn_ins() * output);
  }

  inline size_t getCapacity() const {
    return static_cast<const D *>(this)->maxCapacity();
  }

  void unsafeCopy(const VolumeMatrix &source) const {
    std::copy(source.volumes(0), source.vol + (algn_ins() * outs()),
              volumes(0));
  }

public:
  template <size_t ALIGN> bool assign(const VolumeMatrix<T, D, ALIGN> &source) {
    if (ins() == source.ins() && outs() == source.outs()) {
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
    const size_t MIN_CHANNELS = std::min(ins(), outs());
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
    const size_t MAX_CHANNELS = std::max(ins(), outs());
    zero();
    T flushedScale = flushToZero(scale);
    for (size_t output = 0; output < MAX_CHANNELS; output++) {
      volumes(output % outs())[output % ins()] = flushedScale;
    }
  }

  /**
   * sets all volumes to zero.
   */
  void zero() noexcept {
    T *pT = std::assume_aligned<ALIGN_BYTES>(volumes(0));
    std::fill(pT, pT + algn_ins() * outs(), 0);
  }

  /**
   * Sets all volumes that are less than epsilon (absolute) to zero.
   * (This should never be necessary, but hiding this as an internal detail is
   * rather strict).
   */
  void flushAllToZero() noexcept {
    for (size_t output = 0; output < outs(); output++) {
      T *p = volumes(output);
      for (size_t input = 0; input < ins(); input++) {
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
    if (output < outs() && input < ins()) {
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
    if (output < outs() && input < ins()) {
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
    std::fill(pT, pT + algn_ins() * outs(), flushedToZero);
  }

  /**
   * Approach the volumes of the source matrix, using the specified integration.
   * After an infinite number of invocations, this and the source matrix should
   * be similar.
   * @param source The source matrix whose volumes to aproach.
   * @param coefficients The integration coefficients used.
   */
  void approach(const VolumeMatrix &source,
                const IntegrationCoefficients<T> &coefficients) {
    if (ins() != source.ins() || outs() != source.outs()) {
      throw std::invalid_argument("Approach matrix must have same dimensions");
    }
    for (size_t output = 0; output < outs(); output++) {
      for (size_t input = 0; input < ins(); input++) {
        approachValue(volumes(output)[input], source.volumes(output)[input],
                      coefficients);
      }
    }
  }

  void applyAlignedInputUnsafe(T *__restrict out,
                               const T *__restrict in) const {
    const size_t block = algn_ins();
    T *input = std::assume_aligned<ALIGN_BYTES>(in);
    T *v1 = std::assume_aligned<ALIGN_BYTES>(volumes(0));
    for (size_t output = 0; output < outs(); output++, v1 += block) {
      out[output] = v1[0] * input[0];
      for (size_t i = 1; i < ins(); i++) {
        out[output] += v1[i] * input[i];
      }
    }
  }

  template <size_t INS, size_t OUTS>
  void applyAlignedInputUnsafeFixed(T *__restrict out,
                                    const T *__restrict in) const {
    const size_t block = algn_ins();
    const T *input = std::assume_aligned<ALIGN_BYTES>(in);
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
    if (I != ins() || O != outs()) {
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
    if (I != ins() || O != outs()) {
      throw std::invalid_argument(
          "VolumeMatrix::apply: input and output sizes do not match");
    }
    if (((input.begin() - static_cast<T *>(nullptr)) % ALIGN_BYTES) == 0) {
      applyAlignedInputUnsafeFixed<I, O>(result.begin(), input);
    } else {
      std::copy(input.begin(), input.begin() + ins(), volumes(outs()));
      applyAlignedInputUnsafeFixed<I, O>(result.begin(), volumes(outs()));
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

    if (((inputs - static_cast<T *>(nullptr)) % ALIGN_BYTES) == 0) {
      applyAlignedInputUnsafe(outputs, inputs);
    } else {
      std::copy(inputs, inputs + ins(), volumes(outs()));
      applyAlignedInputUnsafe(outputs, volumes(outs()));
    }
    return true;
  }

private:
  inline T flushToZero(T volume) const noexcept {
    return fabs(volume) > eps ? volume : 0;
  }

  inline void
  approachValue(T &out, T sourceValue,
                const IntegrationCoefficients<T> &coefficients) const {
    out += flushToZero(coefficients.template getIntegrated(sourceValue, out) -
                       out);
  }
};

template <typename T, size_t ALIGN_BYTES>
class DefaultVolumeMatrix
    : public VolumeMatrix<T, DefaultVolumeMatrix<T, ALIGN_BYTES>, ALIGN_BYTES> {
  using Super =
      VolumeMatrix<T, DefaultVolumeMatrix<T, ALIGN_BYTES>, ALIGN_BYTES>;
  friend Super;

  size_t inputs;
  size_t outputs;
  size_t alignedIns;
  T *data;
  T *volumes;

  const T* volumeData() const { return volumes; }
  T* volumeData() { return volumes; }

public:
  DefaultVolumeMatrix(size_t inputs_, size_t outputs_)
      : inputs(Super::validateGetInputs(inputs_, outputs_)), outputs(outputs_),
        alignedIns(Super::alignedInputs(inputs)),
        data(new T[Super::neededCapacity(inputs, outputs) +
                   Super::extraUnalignedCapacity()]),
        volumes(Super::aligned(data)) {}

  /**
   * Creates a volume matrix with the same values als the source, including
   * epsilon, the minimum representable volume.
   * @param source The source matrix to copy.
   */
  explicit DefaultVolumeMatrix(const DefaultVolumeMatrix &source)
      : DefaultVolumeMatrix(source.inputs, source.outputs) {
    unsafeCopy(source);
  }

  /**
   * Creates a volume matrix with the same values als the source, including
   * epsilon, the minimum representable volume.
   * @param source The source matrix to copy.
   */
  template <size_t ALIGN>
  explicit DefaultVolumeMatrix(const DefaultVolumeMatrix<T, ALIGN> &source)
      : DefaultVolumeMatrix(source.inputs, source.outputs) {
    unsafeCopy(source);
  }

  DefaultVolumeMatrix(DefaultVolumeMatrix &&moved)
      : inputs(moved.inputs), alignedIns(moved.alignedIns),
        outputs(moved.outputs), data(moved.data), volumes(moved.volumes) {
    moved.data = nullptr;
    moved.volumes = nullptr;
  }

  ~DefaultVolumeMatrix() {
    if (data) {
      delete[] data;
      data = nullptr;
    }
    volumes = nullptr;
  }
  size_t getInputs() const { return inputs; }
  size_t getAlignedInputs() const { return alignedIns; }
  size_t getOutputs() const { return outputs; }
};

template <typename T, size_t INPUTS, size_t OUTPUTS, size_t ALIGN_BYTES>
class FixedVolumeMatrix
    : public VolumeMatrix<T, FixedVolumeMatrix<T, INPUTS, OUTPUTS, ALIGN_BYTES>,
                          ALIGN_BYTES> {
  using Super =
      VolumeMatrix<T, FixedVolumeMatrix<T, INPUTS, OUTPUTS, ALIGN_BYTES>,
                   ALIGN_BYTES>;
  friend Super;

  static_assert(Super::validateGetInputs(INPUTS, OUTPUTS) > 0);
  static constexpr size_t ALIGNED_INPUTS = Super::alignedInputs(INPUTS);
  static constexpr size_t CAPACITY = Super::neededCapacity(INPUTS, OUTPUTS);

  alignas(ALIGN_BYTES) T volumes[CAPACITY];
  const T* volumeData() const { return &volumes[0]; }
  T* volumeData() { return &volumes[0]; }

public:
  FixedVolumeMatrix() {}

  /**
   * Creates a volume matrix with the same values als the source, including
   * epsilon, the minimum representable volume.
   * @param source The source matrix to copy.
   */
  explicit FixedVolumeMatrix(const FixedVolumeMatrix &source) = default;

  size_t getInputs() const { return INPUTS; }
  size_t getAlignedInputs() const { return ALIGNED_INPUTS; }
  size_t getOutputs() const { return OUTPUTS; }
};

template <typename T, class D, size_t ALIGN_BYTES = 32>
class IntegratedVolumeMatrix : public VolumeMatrix<T, D, ALIGN_BYTES> {
  static_assert(std::is_base_of_v<VolumeMatrix<T, D, ALIGN_BYTES>, D>);
  VolumeMatrix<T, D, ALIGN_BYTES> toFollow;
  IntegrationCoefficients<T> integration;

public:
  IntegratedVolumeMatrix(size_t inputs, size_t outputs,
                         double integrationSamples)
      : VolumeMatrix<T, D, ALIGN_BYTES>(inputs, outputs),
        toFollow(inputs, outputs),
        integration(IntegrationCoefficients<T>(integrationSamples)) {}

  /**
   * Creates a volume matrix with the same values als the source, including
   * epsilon, the minimum representable volume.
   * @param source The source matrix to copy.
   */
  template <size_t ALIGN>
  explicit IntegratedVolumeMatrix(const VolumeMatrix<T, D, ALIGN> &source)
      : VolumeMatrix<T, D, ALIGN_BYTES>(source), toFollow(source) {}

  IntegratedVolumeMatrix(IntegratedVolumeMatrix &&moved) = default;

  void setIntegrationSamples(double integrationSamples) {
    integration.setCharacteristicSamples(integrationSamples);
  }

  template <size_t ALIGN> bool assign(const VolumeMatrix<T, D, ALIGN> *source) {
    return toFollow.template assign(source);
  }

  void approach() { approach(toFollow, integration); }
};

} // namespace tdap

#endif // TDAP_M_VOLUME_MATRIX_HPP
