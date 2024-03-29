#ifndef TDAP_M_IIR_COEFFICIENTS_HPP
#define TDAP_M_IIR_COEFFICIENTS_HPP
/*
 * tdap/IirCoefficients.hpp
 *
 * Part of TdAP
 * Time-domain Audio Processing
 * Copyright (C) 2015 Michel Fleur.
 * Source https://bitbucket.org/emmef/tdap
 * Email  tdap@emmef.org
 *
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

#include <cstddef>
#include <memory>
#include <tdap/AlignedArray.h>
#include <tdap/AlignedFrame.hpp>
#include <tdap/AlignedPointer.h>
#include <tdap/Count.hpp>
#include <tdap/Denormal.hpp>
#include <tdap/Filters.hpp>
#include <tdap/FixedSizeArray.hpp>
#include <tdap/Value.hpp>
#include <type_traits>

namespace tdap {

using namespace std;

/*
 * IRR Filter algorithm and algorithm parts for scalars.
 *
 * The pointers to values and buffers are completely unchecked here.
 */

/**
 * Calculates the scalar output for a scalar input for the specified IIR filter
 * coefficients and the history of input and output values.
 *
 * A filter of order \c N, requires
 * <ul>
 *   <li><code>N + 1</code> feed-forward (FF) coefficients</li>
 *   <li><code>N + 1</code> feed-backward (FF) coefficients, though only \c N
 * are actually used. This is for backward compatibility reasons and a newer
 * implementation should only use \c N coefficients.</li>
 *   <li>\c N elements of input history</li>
 *   <li>\c N elements of output history</li>
 * </ul>
 *
 * The \c Scale parameter configures whether the first feed-forward coefficient
 * functions as scaling factor. This should only be used when the application
 * has write access to both input and output history and also uses the provided
 * shift-history functions. In all other cases do not use it or reset the
 * coefficient to unity.
 *
 * With scale:
 * <code><pre>output = input * FF[0] +
 *   inputHistory[0] * FF[1] + outputHistory[0] * FB[1] +
 *   ...
 *   inputHistory[order - 1] * FF[order] + outputHistory[order - 1] * FB[order];
 * </pre></code>
 *
 * Without scale:
 *
 * <code><pre>output = input +
 *   inputHistory[0] * FF[1] + outputHistory[0] * FB[1] +
 *   ...
 *   inputHistory[order - 1] * FF[order] + outputHistory[order - 1] * FB[order];
 * </pre></code>
 *
 * The choice was made to use addition for both forward and backward
 * coefficients, which is reflected in all filter designs that are part of this
 * library. <strong>Pay attention when using designs from other libraries: you
 * might need to swap the sign of the feed-backward coefficients!</strong>
 *
 * The \c Flush parameter configures whether the output flushes de-normal values
 * to zero. Processors in with multimedia extension can often be put in an
 * operation mode that does this automatically. There are other ways to prevent
 * de-normal numbers, like dithering. But is all this cannot be applied, the
 * option is there to flush.
 *
 * @tparam Scalar The (accurate) type for coefficients and intermediate results.
 * @tparam Value The input and output values of the filter.
 * @tparam Scaled Indicates if the filter scales all input with the zeroth
 * feed-forward coefficient.
 * @param output Will contain the output value.
 * @param input Contains the input value
 * @param order The order of the filter. This can be zero, in which case the
 * output is just the input, that is Scaled by the first feed-forward
 * coefficient if \c Scaled is \c true.
 * @param feedForward The (\c order + 1) feed-forward coefficients.
 * @param feedBackward The (\c order + 1) feed-backward coefficients.
 * @param inHistory The \c order previous input values.
 * @param outHistory The \c order previous output values.
 */
template <typename Scalar, typename Value, bool Scaled>
requires(is_floating_point_v<Scalar> &&std::is_arithmetic_v<Value>) //
    inline static void iir_calculate_output_scalar(
        Value &output,      // output sample value
        const Value &input, // input sample value
        size_t order,
        const Scalar *__restrict feedForward,  // (ORDER + 1) C-coefficients
        const Scalar *__restrict feedBackward, // (ORDER + 1) D-coefficients
        const Value *__restrict inHistory,     // (ORDER) x value history
        const Value *__restrict outHistory     // (ORDER) y value history
    ) {
  if constexpr (Scaled) {
    output = feedForward[0] * input;
  } else {
    output = input;
  }
  for (size_t i = 0; i < order; i++) {
    output +=
        feedForward[i + 1] * inHistory[i] + feedBackward[i + 1] * outHistory[i];
  }
}

/**
 * Shifts the input and output history one sample back in time and sets the
 * most recent values to the provided input and output value.
 * @tparam Value The value type.
 * @param output The output element.
 * @param input The input element.
 * @param order The filter order.
 * @param inHistory The input history, that must be \c order in size.
 * @param outHistory The output history, that must be \c order in size.
 */
template <typename Value>
inline static void
iir_shift_history_scalar(Value &output,      // output sample value
                         const Value &input, // input sample value
                         size_t order,
                         Value *__restrict inHistory, // (ORDER) x value history
                         Value *__restrict outHistory // (ORDER) y value history
) {
  for (size_t i = order - 1; order > 1 && i > 0; i--) {
    inHistory[i] = inHistory[i - 1];
    outHistory[i] = outHistory[i - 1];
  }
  inHistory[0] = input;
  outHistory[0] = output;
}

template <typename Value>
requires(std::is_arithmetic_v<Value>) //
    inline static void iir_flush_denormal_to_zero_scalar(
        Value &output // output sample value
    ) {
  Denormal::flush(output);
}

template <typename Scalar, typename Value, bool Flush = false>
requires(is_floating_point_v<Scalar>) //
    inline static void iir_filter_scalar(
        Value &output,      // output sample value
        const Value &input, // input sample value
        size_t order,
        const Scalar *__restrict feedForward,  // (ORDER + 1) C-coefficients
        const Scalar *__restrict feedBackward, // (ORDER + 1) D-coefficients
        Value *__restrict inHistory,           // (ORDER) x value history
        Value *__restrict outHistory           // (ORDER) y value history
    ) {
  iir_calculate_output_scalar<Scalar, Value, true>(
      output, input, order, feedForward, feedBackward, inHistory, outHistory);
  iir_shift_history_scalar(output, input, order, inHistory, outHistory);

  if constexpr (Flush) {
    iir_flush_denormal_to_zero_scalar(output);
  }
}

template <typename C, typename S, bool Flush = false>
requires(std::is_floating_point_v<C> &&std::is_arithmetic_v<S>) //
    inline static S
    iir_filter(int order,
               const C *__restrict feedForward,  // (order + 1) C-coefficients
               const C *__restrict feedBackward, // (order + 1) D-coefficients
               S *__restrict xHistory,           // (order) x value history
               S *__restrict yHistory,           // (order) y value history
               const S input)                    // input sample value
{
  S result;
  iir_filter_scalar<C, S, Flush>(result, input, order, feedForward,
                                 feedBackward, xHistory, yHistory);
  return result;
}

template <typename C, typename S, size_t ORDER, bool Flush = false>
requires(std::is_floating_point_v<C> &&std::is_arithmetic_v<S> &&ORDER > 0) //
    inline static S iir_filter_fixed(
        const C *__restrict feedForward,  // (ORDER + 1) C-coefficients
        const C *__restrict feedBackward, // (ORDER + 1) D-coefficients
        S *__restrict xHistory,           // (ORDER) x value history
        S *__restrict yHistory,           // (ORDER) y value history
        const S input)                    // input sample value
{
  return iir_filter<C, S, Flush>(ORDER, feedForward, feedBackward, xHistory,
                                 yHistory, input);
}

/**
 * Calculates the vector output for a vector input for the specified IIR filter
 * coefficients and the history of input and output values.
 *
 * A filter of order \c N, requires
 * <ul>
 *   <li><code>N + 1</code> feed-forward (FF) coefficients</li>
 *   <li><code>N + 1</code> feed-backward (FF) coefficients, though only \c N
 * are actually used. This is for backward compatibility reasons and a newer
 * implementation should only use \c N coefficients.</li>
 *   <li>\c N elements of input history</li>
 *   <li>\c N elements of output history</li>
 * </ul>
 *
 * The \c Scale parameter configures whether the first feed-forward coefficient
 * functions as scaling factor. This should only be used when the application
 * has write access to both input and output history and also uses the provided
 * shift-history functions. In all other cases do not use it or reset the
 * coefficient to unity.
 *
 * With scale:
 * <code><pre>output = input * FF[0] +
 *   inputHistory[0] * FF[1] + outputHistory[0] * FB[1] +
 *   ...
 *   inputHistory[order - 1] * FF[order] + outputHistory[order - 1] * FB[order];
 * </pre></code>
 *
 * Without scale:
 *
 * <code><pre>output = input +
 *   inputHistory[0] * FF[1] + outputHistory[0] * FB[1] +
 *   ...
 *   inputHistory[order - 1] * FF[order] + outputHistory[order - 1] * FB[order];
 * </pre></code>
 *
 * The choice was made to use addition for both forward and backward
 * coefficients, which is reflected in all filter designs that are part of this
 * library. <strong>Pay attention when using designs from other libraries: you
 * might need to swap the sign of the feed-backward coefficients!</strong>
 *
 * The \c Flush parameter configures whether the output flushes de-normal values
 * to zero. Processors in with multimedia extension can often be put in an
 * operation mode that does this automatically. There are other ways to prevent
 * de-normal numbers, like dithering. But is all this cannot be applied, the
 * option is there to flush.
 *
 * @tparam Scalar The (accurate) type for coefficients and intermediate results.
 * @tparam Value The input and output values of the filter.
 * @tparam Scaled Indicates if the filter scales all input with the zeroth
 * feed-forward coefficient.
 * @param output Will contain the output value.
 * @param input Contains the input value
 * @param rows The number of rows per vector, that should be positive.
 * @param order The order of the filter. This can be zero, in which case the
 * output is just the input, that is Scaled by the first feed-forward
 * coefficient if \c Scaled is \c true.
 * @param feedForward The (\c order + 1) feed-forward coefficients.
 * @param feedBackward The (\c order + 1) feed-backward coefficients.
 * @param inHistory The \c order previous input values.
 * @param outHistory The \c order previous output values.
 */
template <typename Scalar, typename Vector, bool Scaled>
requires(is_floating_point_v<Scalar>) //
    inline static void iir_calculate_output_vector(
        Vector &output,      // output sample value
        const Vector &input, // input sample value
        size_t rows, size_t order,
        const Scalar *__restrict feedForward,  // (ORDER + 1) C-coefficients
        const Scalar *__restrict feedBackward, // (ORDER + 1) D-coefficients
        const Vector *__restrict inHistory,    // (ORDER) x value history
        const Vector *__restrict outHistory    // (ORDER) y value history
    ) {
  if constexpr (Scaled) {
    for (size_t r = 0; r < rows; r++) {
      output[r] = feedForward[0] * input[r];
    }
  } else {
    for (size_t r = 0; r < rows; r++) {
      output[r] = input[r];
    }
  }
  for (size_t i = 0; i < order; i++) {
    const Scalar &forward = feedForward[i + 1];
    const Scalar &backward = feedBackward[i + 1];
    for (size_t r = 0; r < rows; r++) {
      output[r] += forward * inHistory[i][r] + backward * outHistory[i][r];
    }
  }
}

template <typename Scalar, typename Vector>
requires(is_floating_point_v<Scalar>) //
    inline static void iir_calculate_output_vector_with_history_pointer_scaled(
        Vector &output,      // output sample value
        const Vector &input, // input sample value
        size_t rows, size_t order, size_t &historyPtr,
        const Scalar *__restrict feedForward,  // (ORDER + 1) C-coefficients
        const Scalar *__restrict feedBackward, // (ORDER + 1) D-coefficients
        const Vector *__restrict inHistory,    // (ORDER) x value history
        const Vector *__restrict outHistory    // (ORDER) y value history
    ) {
  Vector scaledIn;
  for (size_t r = 0; r < rows; r++) {
    const auto v = feedForward[0] * input[r];
    output[r] = v;
    scaledIn[r] = v;
  }
  for (size_t i = 1, h = historyPtr; i <= order; i++, h = ((h + 1) % order)) {
    const Scalar &forward = feedForward[i];
    const Scalar &backward = feedBackward[i];
    for (size_t r = 0; r < rows; r++) {
      output[r] += forward * inHistory[h][r] + backward * outHistory[h][r];
    }
  }
  historyPtr = historyPtr > 0 ? historyPtr - 1 : order - 1;
  auto &in = inHistory[historyPtr];
  auto &out = outHistory[historyPtr];
  for (size_t r = 0; r < rows; r++) {
    in[r] = scaledIn[r];
    out[r] = output[r];
  }
}

template <typename Scalar, typename Vector>
requires(is_floating_point_v<Scalar>) //
    inline static void iir_calculate_output_vector_with_history_pointer(
        Vector &output,      // output sample value
        const Vector &input, // input sample value
        size_t rows, size_t order, size_t &historyPtr,
        const Scalar *__restrict feedForward,  // (ORDER + 1) C-coefficients
        const Scalar *__restrict feedBackward, // (ORDER + 1) D-coefficients
        const Vector *__restrict inHistory,    // (ORDER) x value history
        const Vector *__restrict outHistory    // (ORDER) y value history
    ) {
  for (size_t r = 0; r < rows; r++) {
    output[r] = feedForward[0] * input[r];
  }
  for (size_t i = 1, h = historyPtr; i <= order; i++, h = ((h + 1) % order)) {
    const Scalar &forward = feedForward[i];
    const Scalar &backward = feedBackward[i];
    for (size_t r = 0; r < rows; r++) {
      output[r] += forward * inHistory[h][r] + backward * outHistory[h][r];
    }
  }
  historyPtr = historyPtr > 0 ? historyPtr - 1 : order - 1;
  auto &in = inHistory[historyPtr];
  auto &out = outHistory[historyPtr];
  for (size_t r = 0; r < rows; r++) {
    in[r] = input[r];
    out[r] = output[r];
  }
}

/**
 * Shifts the input and output history one sample back in time and sets the
 * most recent values to the provided input and output value.
 * @tparam Vector The vector type.
 * @param output The output element.
 * @param input The input element.
 * @param rows The number of rows of each element (vector).
 * @param order The filter order.
 * @param inHistory The input history, that must be \c order in size.
 * @param outHistory The output history, that must be \c order in size.
 */
template <typename Vector>
inline static void iir_shift_history_vector(
    Vector &output,      // output sample value
    const Vector &input, // input sample value
    size_t rows, size_t order,
    Vector *__restrict inHistory, // (ORDER) x value history
    Vector *__restrict outHistory // (ORDER) y value history
) {
  for (size_t i = order - 1; order > 1 && i > 0; i--) {
    for (size_t r = 0; r < rows; r++) {
      inHistory[i][r] = inHistory[i - 1][r];
      outHistory[i][r] = outHistory[i - 1][r];
    }
  }
  for (size_t r = 0; r < rows; r++) {
    inHistory[0][r] = input[r];
    outHistory[0][r] = output[r];
  }
}

template <typename Vector>
inline static void
iir_flush_denormal_to_zero_vector(Vector &output, // output sample value
                                  size_t rows) {
  for (size_t r = 0; r < rows; r++) {
    Denormal::flush(output[r]);
  }
}

template <typename Scalar, typename Vector, bool Flush = false>
requires(is_floating_point_v<Scalar>) //
    inline static void iir_filter_vector(
        Vector &output,      // output sample value
        const Vector &input, // input sample value
        size_t rows, size_t order,
        const Scalar *__restrict feedForward,  // (ORDER + 1) C-coefficients
        const Scalar *__restrict feedBackward, // (ORDER + 1) D-coefficients
        Vector *__restrict inHistory,          // (ORDER) x value history
        Vector *__restrict outHistory          // (ORDER) y value history
    ) {
  iir_calculate_output_vector<Scalar, Vector, true>(output, input, rows, order,
                                                    feedForward, feedBackward,
                                                    inHistory, outHistory);

  iir_filter_vector_shift(output, input, rows, order, inHistory, outHistory);

  if constexpr (Flush) {
    iir_flush_denormal_to_zero_vector(output);
  }
}

template <typename Scalar, typename Vector, size_t ROWS, size_t ORDER,
          bool Flush = false>
requires(is_floating_point_v<Scalar> &&ROWS > 0 && ORDER > 0) //
    inline static void iir_filter_fixed_vector(
        Vector &output,                        // output sample value
        const Vector &input,                   // input sample value
        const Scalar *__restrict feedForward,  // (ORDER + 1) C-coefficients
        const Scalar *__restrict feedBackward, // (ORDER + 1) D-coefficients
        Vector *__restrict inHistory,          // (ORDER) x value history
        Vector *__restrict outHistory          // (ORDER) y value history
    ) {
  iir_filter_vector<Scalar, Vector, Flush>(output, input, ROWS, ORDER,
                                           feedForward, feedBackward, inHistory,
                                           outHistory);
}

/*
 * The coefficients define infinite impulse response (IIR) recursive filters. A
 * filter of order \c N, requires
 * <ul>
 *   <li><code>N + 1</code> feed-forward (FF) coefficients</li>
 *   <li><code>N + 1</code> feed-backward (FF) coefficients, though only \c N
 * are actually used. This is for backward compatibility reasons and a newer
 * implementation should only use \c N coefficients.</li>
 *   <li>\c N elements of input history</li>
 *   <li>\c N elements of output history</li>
 * </ul>
 *
 * The algorithm is executed by one of the above filter methods.
 *
 * @see #iir_calculate_output_scalar
 * @see iir_calculate_output_vector
 */
struct IirCoefficients {
  static constexpr size_t coefficientsForOrder(size_t order) {
    return order + 1;
  }

  static constexpr size_t totalCoefficientsForOrder(size_t order) {
    return 2 * coefficientsForOrder(order);
  }

  static constexpr size_t historyForOrder(size_t order) { return order; }

  static constexpr size_t totalHistoryForOrder(size_t order) {
    return 2 * historyForOrder(order);
  }

  virtual size_t order() const noexcept = 0;

  virtual size_t maxOrder() const noexcept = 0;

  virtual bool hasFixedOrder() const noexcept = 0;

  void setOrder(size_t newOrder) {
    if (newOrder == order()) {
      return;
    }
    if (hasFixedOrder()) {
      throw std::runtime_error("This set of coefficients has a fixed order.");
    }
    if (newOrder > maxOrder()) {
      throw std::invalid_argument(
          "Exceeded maximum order for this set of coefficients.");
    }
    setOrderUnchecked(newOrder);
  }

  void setC(size_t idx, const double coefficient) {
    return setCUnchecked(validIndex(idx), coefficient);
  }

  void setD(size_t idx, const double coefficient) {
    return setDUnchecked(validIndex(idx), coefficient);
  }

  double getC(size_t idx) const { return getCUnchecked(validIndex(idx)); }

  double getD(size_t idx) const { return getDUnchecked(validIndex(idx)); }

  size_t coefficientCount() const { return coefficientsForOrder(order()); }

  size_t totalCoefficientsCount() const {
    return totalCoefficientsForOrder(order());
  }

  size_t historyCount() const { return historyForOrder(order()); }

  void scaleOnly(double scale) {
    setCUnchecked(0, scale);
    setDUnchecked(0, 0.0);
    for (size_t i = 1; i < order(); i++) {
      setCUnchecked(i, 0.0);
      setDUnchecked(i, 0.0);
    }
  }

  virtual ~IirCoefficients() = default;

protected:
  virtual void setOrderUnchecked(size_t newOrder) = 0;

  virtual void setCUnchecked(size_t idx, const double coefficient) = 0;

  virtual void setDUnchecked(size_t idx, const double coefficient) = 0;

  virtual double getCUnchecked(size_t idx) const = 0;

  virtual double getDUnchecked(size_t idx) const = 0;

  [[nodiscard]] size_t validIndex(size_t index) const {
    if (index <= order()) {
      return index;
    }
    throw std::invalid_argument("Index out of bounds for this coefficient set");
  }
};

template <class Implementation>
class WrappedIirCoefficients : public IirCoefficients {
  Implementation &impl_;

public:
  WrappedIirCoefficients(Implementation &impl) : impl_(impl) {}

  size_t order() const noexcept override { return impl_.order(); }

  size_t maxOrder() const noexcept override { return impl_.maxOrder(); }

  bool hasFixedOrder() const noexcept override { return impl_.hasFixedOrder(); }

protected:
  void setOrderUnchecked(size_t newOrder) override { impl_.setOrder(newOrder); }

  void setCUnchecked(size_t idx, const double coefficient) override {
    impl_.setC(idx, coefficient);
  }

  void setDUnchecked(size_t idx, const double coefficient) override {
    impl_.setD(idx, coefficient);
  }

  double getCUnchecked(size_t idx) const override { return impl_.getC(idx); }

  double getDUnchecked(size_t idx) const override { return impl_.getC(idx); }
};

template <typename C> class VariableSizedIirCoefficients;

template <typename C, size_t ORDER> class FixedSizeIirCoefficients {
  static_assert(is_floating_point<C>::value,
                "Coefficient type should be floating-point");

public:
  static constexpr size_t COEFFS = IirCoefficients::coefficientsForOrder(ORDER);
  static constexpr size_t TOTAL_COEEFS =
      IirCoefficients::totalCoefficientsForOrder(ORDER);
  static constexpr size_t C_OFFSET = 0;
  static constexpr size_t D_OFFSET = COEFFS;
  static constexpr size_t HISTORY = IirCoefficients::historyForOrder(ORDER);
  static constexpr size_t TOTAL_HISTORY =
      IirCoefficients::totalHistoryForOrder(ORDER);

private:
  constexpr size_t getCOffset(size_t idx) const {
    return Value<size_t>::valid_below(idx, COEFFS) + C_OFFSET;
  }

  constexpr size_t getDOffset(size_t idx) const {
    return Value<size_t>::valid_below(idx, COEFFS) + D_OFFSET;
  }

  C &C_(size_t idx) { return data[getCOffset(idx)]; }

  C &D_(size_t idx) { return data[getDOffset(idx)]; }

  const C &C_(size_t idx) const { return data[getCOffset(idx)]; }

  const C &D_(size_t idx) const { return data[getDOffset(idx)]; }

  const C *const unsafeC() const { return data + C_OFFSET; }

  const C *const unsafeD() const { return data + D_OFFSET; }

public:
  size_t order() const { return ORDER; }

  size_t maxOrder() const { return ORDER; }

  bool hasFixedOrder() const { return true; }

  void setOrder(size_t){};

  void setC(size_t idx, const C coefficient) { C_(idx) = coefficient; }

  void setD(size_t idx, const C coefficient) { D_(idx) = coefficient; }

  C getC(size_t idx) const { return C_(idx); }

  C getD(size_t idx) const { return D_(idx); }

  void setTransparent() {
    for (size_t i = 0; i < TOTAL_COEEFS; i++) {
      data[i] = 0.0;
    }
    C_(0) = 1;
  }

  void assign(const IirCoefficients &source) {
    if (source.order() == ORDER) {
      for (size_t i = 0; i < COEFFS; i++) {
        C_(i) = source.getC(i);
        D_(i) = source.getD(i);
      }
      return;
    }
    throw invalid_argument("Value not below threshold_");
  }

  void operator=(const IirCoefficients &source) { assign(source); }

  template <typename S>
  void assign(const FixedSizeIirCoefficients<S, ORDER> &coeffs) {
    for (size_t i = 0; i < COEFFS; i++) {
      setC(i, coeffs.getC(i));
      setD(i, coeffs.getD(i));
    }
  }

  template <typename S>
  void operator=(const FixedSizeIirCoefficients<S, ORDER> &coeffs) {
    assign(coeffs);
  }

  template <typename S>
  void assign(const VariableSizedIirCoefficients<S> &coeffs) {
    if (coeffs.order() != ORDER) {
      throw invalid_argument("FixedSizeIirCoefficients: Source coefficients "
                             "must be of same order");
    }
    for (size_t i = 0; i < COEFFS; i++) {
      setC(i, coeffs.getC(i));
      setD(i, coeffs.getD(i));
    }
  }

  template <typename S>
  void operator=(const VariableSizedIirCoefficients<S> &coeffs) {
    assign(coeffs);
  }

  template <typename S, bool flushToZero>
  S do_filter(S *const xHistory, // (ORDER) x value history
              S *const yHistory, // (ORDER) y value history
              S input) const {
    return iir_filter_fixed<C, S, ORDER, flushToZero>(
        unsafeC(), unsafeD(), xHistory, yHistory, input);
  }

  template <typename S>
  S filter(S *const xHistory, S *const yHistory, const S input) const {
    return do_filter<S, false>(xHistory, yHistory, input);
  }

  WrappedIirCoefficients<FixedSizeIirCoefficients<C, ORDER>> wrap() {
    return WrappedIirCoefficients<FixedSizeIirCoefficients<C, ORDER>>(*this);
  }

private:
  alignas(Count<C>::align()) C data[TOTAL_COEEFS];
};

template <typename C, size_t ORDER, size_t Rows, typename V = C>
requires(std::is_floating_point_v<C> &&std::is_arithmetic_v<V> &&Rows >
         0) class AlignedArrayFilter
    : public tdap::VectorFilter<AlignedArray<V, Rows>> {
  std::array<AlignedArray<V, Rows>, 2 * ORDER> history;
  size_t historyPtr;

public:
  const FixedSizeIirCoefficients<C, ORDER> coefficients_;

  AlignedArrayFilter(const FixedSizeIirCoefficients<C, ORDER> &coeffs)
      : coefficients_(coeffs) {}

  bool filter(AlignedArray<V, Rows> &output,
              const AlignedArray<V, Rows> &input) override {
    iir_calculate_output_vector_with_history_pointer_scaled<
        C, AlignedArray<V, Rows>>(
        output, input, Rows, ORDER, historyPtr, &coefficients_.getC(0),
        &coefficients_.getD(0), history.begin(), history.begin() + ORDER);
    return true;
  }

  bool filter(AlignedArray<V, Rows> &output,
              const AlignedArray<V, Rows> &input, size_t rows) override {
    if (rows <= Rows) {
      iir_calculate_output_vector_with_history_pointer_scaled<
          C, AlignedArray<V, Rows>>(
          output, input, Rows, ORDER, historyPtr, &coefficients_.getC(0),
          &coefficients_.getD(0), history.begin(), history.begin() + ORDER);
      return true;
    }
    return false;
  }

  template<size_t rows>
  bool filter(AlignedArray<V, Rows> &output,
              const AlignedArray<V, Rows> &input) {
    if constexpr (rows <= Rows) {
      iir_calculate_output_vector_with_history_pointer_scaled<
          C, AlignedArray<V, Rows>>(
          output, input, Rows, ORDER, historyPtr, &coefficients_.getC(0),
          &coefficients_.getD(0), history.begin(), history.begin() + ORDER);
      return true;
    }
    return false;
  }

  void reset() override {
    for (size_t i = 0; i < 2 * ORDER; i++) {
      std::fill(history[i].begin(), history[i].end(), 0);
    }
    historyPtr = 0;
  }
};

template <typename C, size_t ORDER, size_t Rows, typename V = C>
requires(std::is_floating_point_v<C> &&std::is_arithmetic_v<V> &&Rows >
         0) class AlignedPointerFilter
    : public tdap::VectorFilter<AlignedPointer<V, Rows>> {
  std::array<AlignedArray<V, Rows>, 2 * ORDER> history;
  size_t historyPtr;

public:
  const FixedSizeIirCoefficients<C, ORDER> coefficients_;

  AlignedPointerFilter(const FixedSizeIirCoefficients<C, ORDER> &coeffs)
      : coefficients_(coeffs) {}

  bool filter(AlignedPointer<V, Rows> &output,
              const AlignedPointer<V, Rows> &input) override {
    iir_calculate_output_vector_with_history_pointer_scaled<
        C, AlignedArray<V, Rows>>(
        output, input, Rows, ORDER, historyPtr, &coefficients_.getC(0),
        &coefficients_.getD(0), history.begin(), history.begin() + ORDER);
    return true;
  }

  bool filter(AlignedPointer<V, Rows> &output,
              const AlignedPointer<V, Rows> &input, size_t rows) override {
    if (rows <= Rows) {
      iir_calculate_output_vector_with_history_pointer_scaled<
          C, AlignedArray<V, Rows>>(
          output, input, Rows, ORDER, historyPtr, &coefficients_.getC(0),
          &coefficients_.getD(0), history.begin(), history.begin() + ORDER);
      return true;
    }
    return false;
  }

  template <size_t rows>
  bool filter(AlignedPointer<V, Rows> &output,
              const AlignedPointer<V, Rows> &input) {
    if constexpr (rows <= Rows) {
      iir_calculate_output_vector_with_history_pointer_scaled<
          C, AlignedArray<V, Rows>>(
          output, input, Rows, ORDER, historyPtr, &coefficients_.getC(0),
          &coefficients_.getD(0), history.begin(), history.begin() + ORDER);
      return true;
    }
    return false;
  }

  void reset() override {
    for (size_t i = 0; i < 2 * ORDER; i++) {
      std::fill(history[i].begin(), history[i].end(), 0);
    }
    historyPtr = 0;
  }
};

template <typename C> class VariableSizedIirCoefficients {
  const size_t maxOrder_;
  size_t order_;
  C *data_;

  size_t getCBaseOffset() const { return 0; }

  size_t getDBaseOffset() const { return maxOrder_ + 1; }

  size_t getCOffset(size_t i) const {
    return getCBaseOffset() + Value<size_t>::valid_below_or_same(i, order_);
  }

  size_t getDOffset(size_t i) const {
    return getDBaseOffset() + Value<size_t>::valid_below_or_same(i, order_);
  }

  C &C_(size_t i) { return data_[getCOffset(i)]; }

  C &D_(size_t i) { return data_[getDOffset(i)]; }

  const C &C_(size_t i) const { return data_[getCOffset(i)]; }

  const C &D_(size_t i) const { return data_[getDOffset(i)]; }

  C *const unsafeC() const { return data_ + getCBaseOffset(); }

  C *const unsafeD() const { return data_ + getDBaseOffset(); }

public:
  VariableSizedIirCoefficients(size_t maxOrder)
      : maxOrder_(Value<size_t>::valid_between(maxOrder, 1, 64)),
        order_(maxOrder_),
        data_(new C[IirCoefficients::totalCoefficientsForOrder(maxOrder_)]) {}

  VariableSizedIirCoefficients(size_t maxOrder, size_t order)
      : maxOrder_(Value<size_t>::valid_between(maxOrder, 1, 64)),
        order_(Value<size_t>::valid_between(order, 1, maxOrder_)),
        data_(new C[IirCoefficients::totalCoefficientsForOrder(maxOrder_)]) {}

  size_t order() const { return order_; }

  size_t maxOrder() const { return maxOrder_; }

  bool hasFixedOrder() { return false; }

  void setOrder(size_t order) {
    order_ = Value<size_t>::valid_between(order, 1, maxOrder_);
  }

  void setC(size_t idx, const C coefficient) override { C_(idx) = coefficient; }

  void setD(size_t idx, const C coefficient) override { D_(idx) = coefficient; }

  C getC(size_t idx) const override { return C_(idx); }

  C getD(size_t idx) const override { return D_(idx); }

  void assign(const IirCoefficients &source) {
    setOrder(source.order());
    for (size_t i = 0; i <= order_; i++) {
      C_(i) = source.getC(i);
      D_(i) = source.getD(i);
    }
  }

  template <typename S, size_t ORDER>
  void assign(const FixedSizeIirCoefficients<S, ORDER> &coeffs) {
    if (ORDER > maxOrder()) {
      throw invalid_argument(
          "VariableSizedIirCoefficients: order of source exceeds my max order");
    }
    setOrder(ORDER);
    for (size_t i = 0; i < IirCoefficients::totalCoefficientsForOrder(order_);
         i++) {
      setC(i, coeffs.getC(i));
      setD(i, coeffs.getD(i));
    }
  }

  template <typename S, size_t ORDER>
  void operator=(const FixedSizeIirCoefficients<S, ORDER> &coeffs) {
    assign(coeffs);
  }

  template <typename S, size_t ORDER>
  void assign(const VariableSizedIirCoefficients<S> &coeffs) {
    if (coeffs.order() > maxOrder()) {
      throw invalid_argument(
          "VariableSizedIirCoefficients: order of source exceeds my max order");
    }
    setOrder(coeffs.order());
    for (size_t i = 0; i < IirCoefficients::totalCoefficientsForOrder(order_);
         i++) {
      setC(i, coeffs.getC(i));
      setD(i, coeffs.getD(i));
    }
  }

  template <typename S, size_t ORDER>
  void operator=(const VariableSizedIirCoefficients<S> &coeffs) {
    assign(coeffs);
  }

  template <typename S, bool flushToZero = false>
  S do_filter(S *const xHistory, // (ORDER) x value history
              S *const yHistory, // (ORDER) y value history
              S input) const {
    return iir_filter<C, S, flushToZero>(order_, unsafeC(), unsafeD(), xHistory,
                                         yHistory, input);
  }

  template <typename S>
  S filter(S *const xHistory, // (ORDER) x value history
           S *const yHistory, // (ORDER) y value history
           S input) const {
    return do_filter<S, false>(xHistory, yHistory, input);
  }

  WrappedIirCoefficients<VariableSizedIirCoefficients<C>> wrap() {
    return WrappedIirCoefficients<VariableSizedIirCoefficients<C>>(*this);
  }

  ~VariableSizedIirCoefficients() { delete[] data_; }
};

template <typename C, size_t CHANNELS, size_t ORDER>
struct FixedSizeIirCoefficientFilter {
  using Coefficients = FixedSizeIirCoefficients<C, ORDER>;

  static constexpr size_t historySize() {
    return IirCoefficients::historyForOrder(ORDER);
  }

  static constexpr size_t coefficientSize() {
    return IirCoefficients::coefficientsForOrder(ORDER);
  }

  struct History {
    C x[historySize()];
    C y[historySize()];
  };

  struct SingleChannelFilter : public tdap::Filter<C> {
    FixedSizeIirCoefficientFilter<C, CHANNELS, ORDER> &wrapped_;

    virtual void reset() { wrapped_.reset(); }

    virtual C filter(C input) { return wrapped_.filter(0, input); }

    SingleChannelFilter(
        FixedSizeIirCoefficientFilter<C, CHANNELS, ORDER> &wrapped)
        : wrapped_(wrapped) {}
  };

  struct MultiChannelFilter : public tdap::MultiFilter<C> {
    FixedSizeIirCoefficientFilter<C, CHANNELS, ORDER> &wrapped_;

    virtual size_t channels() const override { return CHANNELS; }

    virtual void reset() override { wrapped_.reset(); }

    virtual C filter(size_t idx, C input) override {
      return wrapped_.filter(idx, input);
    }

    MultiChannelFilter(FixedSizeIirCoefficientFilter &wrapped)
        : wrapped_(wrapped) {}
  };

  Coefficients coefficients_;
  History history[CHANNELS];

  FixedSizeIirCoefficientFilter() = default;

  FixedSizeIirCoefficientFilter(const Coefficients &coefficients)
      : coefficients_(coefficients) {}

  void reset() {
    for (size_t channel = 0; channel < CHANNELS; channel++) {
      for (size_t t = 0; t < historySize(); t++) {
        history[channel].x[t] = 0;
        history[channel].y[t] = 0;
      }
    }
  }

  template <bool flushToZero> C do_filter(size_t channel, C input) {
    IndexPolicy::array(channel, CHANNELS);
    return coefficients_.template do_filter<C, flushToZero>(
        history[channel].x, history[channel].y, input);
  }

  C filter(size_t channel, C input) { return do_filter<false>(channel, input); }

  template <size_t N, typename... A>
  void filterArray(const FixedSizeArrayTraits<C, N, A...> &input,
                   FixedSizeArrayTraits<C, N, A...> &output) {
    for (size_t channel = 0; channel < Value<size_t>::min(CHANNELS, N);
         channel++) {
      output[channel] = filter(channel, input[channel]);
    }
  }

  template <typename... A>
  void filterArray(const ArrayTraits<C, A...> &input,
                   ArrayTraits<C, A...> &output) {
    for (size_t channel = 0;
         channel < Value<size_t>::min(CHANNELS, input.size(), output.size());
         channel++) {
      output[channel] = filter(channel, input[channel]);
    }
  }

  SingleChannelFilter wrapSingle() { return SingleChannelFilter(*this); }

  MultiChannelFilter wrapMulti() { return MultiChannelFilter(*this); }

  Filter<C> *createFilter() { return new SingleChannelFilter(*this); }

  MultiFilter<C> *createMultiFilter() { return new MultiChannelFilter(*this); }
};

template <typename S, size_t ORDER, size_t CHANNELS, bool FLUSH = false,
          size_t ALIGN = Count<S>::align()>
struct MultiFilterData {
  static_assert(is_arithmetic<S>::value, "Value type should be arithmetic");
  static_assert(ORDER > 0, "ORDER of filter must be positive");
  static_assert(CHANNELS > 0, "CHANNELS of filter must be positive");
  static constexpr size_t HISTORY = IirCoefficients::historyForOrder(ORDER);

  struct alignas(ALIGN) Vector : public FixedSizeArray<S, HISTORY> {
    using FixedSizeArray<S, HISTORY>::operator=;
    template <typename... A>
    Vector(const FixedSizeArrayTraits<S, HISTORY, A...> &source)
        : FixedSizeArray<S, HISTORY>(source) {}
    Vector(const S value) : FixedSizeArray<S, HISTORY>(value) {}
  };

  static_assert(Count<Vector>::is_valid_sum(CHANNELS, CHANNELS, CHANNELS),
                "CHANNELS must theoretically fit in memory");

  FixedSizeIirCoefficients<S, ORDER> coeff;
  Vector xHistory[CHANNELS];
  Vector yHistory[CHANNELS];

  void zero() {
    for (size_t i = 0; i < CHANNELS; i++) {
      xHistory[i].zero();
      yHistory[i].zero();
    }
  }

  MultiFilterData() { zero(); }

  template <typename... A>
  inline void filter(FixedSizeArrayTraits<S, CHANNELS, A...> &target,
                     const FixedSizeArrayTraits<S, CHANNELS, A...> &source) {
    Vector Y = 0.0;
    const Vector input = source;
    Vector X = input; // source is xN0
    Vector yN0 = 0.0;
    size_t i, j;
    Vector xN1;
    Vector yN1;
    for (i = 0, j = 1; i < ORDER; i++, j++) {
      xN1 = xHistory[i];
      yN1 = yHistory[i];
      xHistory[i] = X;
      X = xN1;
      yHistory[i] = Y;
      Y = yN1;
      yN0 += xN1 * coeff.getC(j);
      yN0 += yN1 * coeff.getD(j);
    }
    yN0 += input * coeff.getC(0);

    if (FLUSH) {
      Denormal::flush(yN0);
    }
    yHistory[0] = yN0;

    target = yN0;
  }
};

enum class IirFilterResult { SUCCESS, NULL_PTR, UNALIGNED_PTR };

template <typename C, size_t ORDER, size_t ALIGN_SAMPLES = 4>
struct FixedOrderIirFrameFilterBase : public IirCoefficients {
  static_assert(ORDER > 0 && ORDER < 16, "ORDER is not between 1 and 16");
  static_assert(Power2::constant::is(ALIGN_SAMPLES),
                "ALIGNMENT is not a power of two.");
  static constexpr size_t ALIGN_BYTES = ALIGN_SAMPLES * sizeof(C);
  using Coeffs = AlignedFrame<C, ORDER + 1, ALIGN_SAMPLES>;

  size_t order() const noexcept override { return ORDER; }

  size_t maxOrder() const noexcept override { return ORDER; }

  bool hasFixedOrder() const noexcept override { return false; }

  const C *cCoeffs() const noexcept { return c.data; }

  const C *dCoeffs() const noexcept { return d.data; }

  static constexpr size_t alignedSamplesInFrame(size_t channels) noexcept {
    return Power2::constant::aligned_with(channels, ALIGN_SAMPLES);
  }

  static constexpr IirFilterResult checkIO(const C *x, const C *y) {
    if (!x || !y) {
      return IirFilterResult::NULL_PTR;
    }
    if ((reinterpret_cast<size_t>(x) & (ALIGN_BYTES - 1)) != 0) {
      return IirFilterResult::UNALIGNED_PTR;
    }
    if ((reinterpret_cast<size_t>(y) & (ALIGN_BYTES - 1)) != 0) {
      return IirFilterResult::UNALIGNED_PTR;
    }
    return IirFilterResult::SUCCESS;
  }

  C filterSingleWithHistory(C *__restrict xHistory, C *__restrict yHistory,
                            const C &x) noexcept {

    C Y = 0;
    C X = x; // input is xN0
    C yN0 = c[0] * x;
    size_t i, j;
    for (i = 0, j = 1; i < ORDER; i++, j++) {
      const C xN1 = xHistory[i];
      const C yN1 = yHistory[i];
      xHistory[i] = X;
      X = xN1;
      yHistory[i] = Y;
      Y = yN1;
      yN0 += xN1 * c[j] + yN1 * d[j];
    }

    yHistory[0] = yN0;

    return yN0;
  }

  IirFilterResult filterSingeChannelOffsetByOrder(C *__restrict y,
                                                  const C *__restrict x,
                                                  size_t count) noexcept {
    if (count == 0) {
      return IirFilterResult::SUCCESS;
    }
    if (!y || !x) {
      return IirFilterResult::NULL_PTR;
    }
    unsafeSingleChannelIterations(y, x, count, ORDER);
    return IirFilterResult::SUCCESS;
  }

  template <size_t CHANNELS>
  IirFilterResult filterOffsetByOrderFrames(C *__restrict y,
                                            const C *__restrict x,
                                            size_t count) noexcept {
    if (count == 0) {
      return IirFilterResult::SUCCESS;
    }
    IirFilterResult result = checkIO(x, y);
    if (result != IirFilterResult::SUCCESS) {
      return result;
    }
    unsafeIterations<CHANNELS>(y, x, count);
    return IirFilterResult::SUCCESS;
  }

  IirFilterResult filterSingleChannelHistoryZero(C *__restrict y,
                                                 const C *__restrict x,
                                                 size_t count) noexcept {
    if (count == 0) {
      return IirFilterResult::SUCCESS;
    }
    if (!y || !x) {
      return IirFilterResult::NULL_PTR;
    }
    unsafeSingleChannelRampUpIterations(y, x, count);
    unsafeSingleChannelIterations(y, x, count);
    return IirFilterResult::SUCCESS;
  }

  template <size_t CHANNELS>
  IirFilterResult filterHistoryZero(C *__restrict y, const C *__restrict x,
                                    size_t count) noexcept {
    if (count == 0) {
      return IirFilterResult::SUCCESS;
    }
    IirFilterResult result = checkIO(x, y);
    if (result != IirFilterResult::SUCCESS) {
      return result;
    }
    unsafeRampUpIterations<CHANNELS>(y, x, count);
    unsafeIterationsAlt<CHANNELS>(y, x, count);
    return IirFilterResult::SUCCESS;
  }

protected:
  void setOrderUnchecked(size_t) override {}

  void setCUnchecked(size_t idx, const double coefficient) override {
    c[idx] = coefficient;
  }

  void setDUnchecked(size_t idx, const double coefficient) override {
    d[idx] = coefficient;
  }

  double getCUnchecked(size_t idx) const override { return c[idx]; }

  double getDUnchecked(size_t idx) const override { return d[idx]; }

protected:
  Coeffs c;
  Coeffs d;

  tdap_force_inline void
  unsafeSingleChannelRampUpIterations(C *__restrict y, const C *__restrict x,
                                      size_t count) const noexcept {
    size_t end = std::min(ORDER, count);
    for (size_t n = 0; n < end; n++) {
      C yN = c[0] * x[n];
      ptrdiff_t h = n - 1;
      for (size_t j = 1; h >= 0 && j <= this->ORDER; j++, h--) {
        yN += x[h] * c[j] + y[h] * d[j];
      }
      y[n] = yN;
    }
  }

  template <size_t CHANNELS>
  tdap_force_inline void unsafeRampUpIterations(C *__restrict yPtr,
                                                const C *__restrict xPtr,
                                                size_t count) const noexcept {
    static constexpr size_t FRAME_ELEMENTS =
        Power2::constant::aligned_with(CHANNELS, ALIGN_SAMPLES);

    C *y = assume_aligned<ALIGN_BYTES, C>(yPtr);
    const C *x = assume_aligned<ALIGN_BYTES, const C>(xPtr);
    size_t end = std::min(ORDER, count);

    for (size_t i = 0, n = 0; i < end; i++, n += FRAME_ELEMENTS) {
      for (size_t channel = 0; channel < CHANNELS; channel++) {
        const size_t offs = n + channel;
        C yN = c[0] * x[offs];
        for (size_t j = 1, h = offs; j <= i; j++) {
          h -= FRAME_ELEMENTS;
          yN += x[h] * c[j] + y[h] * d[j];
        }
        y[offs] = yN;
      }
    }
  }

  tdap_force_inline void
  unsafeSingleChannelIterations(C *__restrict y, const C *__restrict x,
                                size_t count) const noexcept {
    for (size_t n = ORDER; n < count; n++) {
      C yN = c[0] * x[n];
      for (size_t j = 1; j <= ORDER; j++) {
        yN += x[n - j] * c[j] + y[n - j] * d[j];
      }
      y[n] = yN;
    }
  }

  template <size_t CHANNELS>
  tdap_force_inline void unsafeIterations(C *__restrict yPtr,
                                          const C *__restrict xPtr,
                                          size_t count) const noexcept {
    static constexpr size_t FRAME_ELEMENTS =
        Power2::constant::aligned_with(CHANNELS, ALIGN_SAMPLES);

    C *y = assume_aligned<ALIGN_BYTES, C>(yPtr);
    const C *x = assume_aligned<ALIGN_BYTES, const C>(xPtr);
    size_t start = FRAME_ELEMENTS * ORDER;
    const size_t end = count * FRAME_ELEMENTS;

    for (size_t n = start; n < end; n += FRAME_ELEMENTS) {
      for (size_t channel = 0; channel < CHANNELS; channel++) {
        size_t offs = n + channel;
        C yN = c[0] * x[offs];
        for (size_t j = 1, h = offs; j <= ORDER; j++) {
          h -= FRAME_ELEMENTS;
          yN += x[h] * c[j] + y[h] * d[j];
        }
        y[offs] = yN;
      }
    }
  }

  template <size_t CHANNELS>
  tdap_force_inline void unsafeIterationsAlt(C *__restrict yPtr,
                                             const C *__restrict xPtr,
                                             size_t count) const noexcept {
    static constexpr size_t FRAME_ELEMENTS =
        Power2::constant::aligned_with(CHANNELS, ALIGN_SAMPLES);

    C *y = assume_aligned<ALIGN_BYTES, C>(yPtr);
    const C *x = assume_aligned<ALIGN_BYTES, const C>(xPtr);
    size_t start = FRAME_ELEMENTS * ORDER;
    const size_t end = count * FRAME_ELEMENTS;

    for (size_t n = start; n < end; n += FRAME_ELEMENTS) {
      C yN[CHANNELS];
      for (size_t channel = 0, offs = n; channel < CHANNELS;
           channel++, offs++) {
        yN[channel] = c[0] * x[offs];
      }
      for (size_t j = 1, h = n; j <= ORDER; j++) {
        h -= FRAME_ELEMENTS;
        for (size_t channel = 0, offs = h; channel < CHANNELS;
             channel++, offs++) {
          yN[channel] += x[h] * c[j] + y[h] * d[j];
        }
      }
      for (size_t channel = 0, offs = n; channel < CHANNELS;
           channel++, offs++) {
        y[offs] = yN[channel];
      }
    }
  }
};

template <typename C, size_t ORDER, size_t CHANNELS, size_t ALIGN_SAMPLES = 4>
struct FixedOrderIirFrameFilter {
  using Coeffs = FixedOrderIirFrameFilterBase<C, ORDER, ALIGN_SAMPLES>;
  static_assert(CHANNELS > 0 && CHANNELS < 1024,
                "CHANNELS is not between 1 and 1024");
  static constexpr size_t HISTORY_SIZE = ORDER + 1;
  using Frame = AlignedFrame<C, CHANNELS, ALIGN_SAMPLES>;

  Coeffs coeffs;

  void clearHistory() noexcept {
    for (size_t i = 0; i < HISTORY_SIZE; i++) {
      x[i].zero();
      y[i].zero();
    }
  }

  inline void filter_history_shift(Frame &__restrict out,
                                   const Frame &__restrict in) noexcept {
    Frame &output = *assume_aligned<Frame::alignBytes, Frame>(&out);
    const Frame &input = *assume_aligned<Frame::alignBytes, const Frame>(&in);

    Frame Y(0);
    Frame X = input; // input is xN0
    Frame yN0(0);
    size_t i, j;
    auto c = coeffs.cCoeffs();
    auto d = coeffs.cCoeffs();
    for (i = 0, j = 1; i < ORDER; i++, j++) {
      const Frame xN1 = x[i];
      const Frame yN1 = y[i];
      x[i] = X;
      X = xN1;
      y[i] = Y;
      Y = yN1;
      yN0 += c[j] * xN1 + d[j] * yN1;
    }
    yN0 += c[0] * input;

    output = y[0] = yN0;
  }

  inline void filterHistoryZero(Frame *__restrict out,
                                const Frame *__restrict in,
                                size_t count) noexcept {
    Frame *y = assume_aligned<Frame::alignBytes, Frame>(out);
    const Frame *x = assume_aligned<Frame::alignBytes, const Frame>(in);
    size_t end = std::min(ORDER, count);
    auto c = coeffs.cCoeffs();
    auto d = coeffs.cCoeffs();

    for (size_t n = 0; n < end; n++) {
      Frame yN = c[0] * x[n];
      for (size_t j = 1, h = n; j <= n; j++) {
        h--;
        yN += x[h] * c[j] + y[h] * d[j];
      }
      y[n] = yN;
    }
    for (size_t n = ORDER; n < count; n++) {
      Frame yN = c[0] * x[n];
      for (size_t j = 1, h = n; j <= ORDER; j++) {
        h--;
        yN += x[h] * c[j] + y[h] * d[j];
      }
      y[n] = yN;
    }
  }

  inline void filterOffsetByOrder(Frame *__restrict out,
                                  const Frame *__restrict in,
                                  size_t count) noexcept {
    Frame *y = assume_aligned<Frame::alignBytes, Frame>(out);
    const Frame *x = assume_aligned<Frame::alignBytes, const Frame>(in);
    auto c = coeffs.cCoeffs();
    auto d = coeffs.cCoeffs();

    for (size_t n = ORDER; n < count; n++) {
      Frame yN = c[0] * x[n];
      for (size_t j = 1, h = n; j <= ORDER; j++) {
        h--;
        yN += x[h] * c[j] + y[h] * d[j];
      }
      y[n] = yN;
    }
  }

  //
  //  inline void filter(C *__restrict out, const C *__restrict in,
  //                     size_t count) noexcept {
  //    static constexpr size_t alignBytes = Frame::alignBytes;
  //    static constexpr size_t ALIGN_ELEM = Frame::frameSize;
  //    const size_t COUNT = ALIGN_ELEM * count;
  //    C *y = assume_aligned<Frame::alignBytes, C>(out);
  //    const C *x = assume_aligned<Frame::alignBytes, const C>(in);
  //
  //    for (size_t n = ALIGN_ELEM * SKIP_ELEMENTS; n < COUNT; n += ALIGN_ELEM)
  //    {
  //      size_t hStart = n - ALIGN_ELEM;
  //      for (size_t i = 0; i < CHANNELS; i++) {
  //        C yN = c[0] * x[n + i];
  //        size_t h = hStart;
  //        for (size_t j = 1; j <= ORDER; j++, h -= ALIGN_ELEM) {
  //          yN += x[h + i] * c[j] + y[h + i] * d[j];
  //        }
  //        y[n + i] = yN;
  //      }
  //    }
  //  }
  //  // 7.74553e-5 ; naive x 1.75928 +/- 0.016
  //  inline void filter(C *__restrict out, const C *__restrict in,
  //                     size_t count) noexcept {
  //    static constexpr size_t alignBytes = Frame::alignBytes;
  //    static constexpr size_t ALIGN_ELEM = Frame::frameSize;
  //    const size_t COUNT = ALIGN_ELEM * count;
  //    C *y = assume_aligned<Frame::alignBytes, C>(out);
  //    const C *x = assume_aligned<Frame::alignBytes, const C>(in);
  //
  //    for (size_t n = ALIGN_ELEM * SKIP_ELEMENTS; n < COUNT; n+= ALIGN_ELEM) {
  //      C yN[CHANNELS];
  //      for (size_t i = 0; i < CHANNELS; i++) {
  //        yN[i] = c[0] * x[n + i];
  //      }
  //      size_t h = n - ALIGN_ELEM;
  //      for (size_t j = 1; j <= ORDER; j++, h-= ALIGN_ELEM) {
  //        for (size_t i = 0; i < CHANNELS; i++) {
  //          yN[i] += x[h + i] * c[j] +
  //                   y[h + i] * d[j];
  //        }
  //      }
  //      for (size_t i = 0; i < CHANNELS; i++) {
  //        y[n + i] = yN[i];
  //      }
  //    }
  //  }
  //  // 7.73091e-5 ; naive x 1.83941 +/- 0.25
  //  inline void filter(C *__restrict out, const C *__restrict in,
  //                     size_t count) noexcept {
  //    static constexpr size_t alignBytes = Frame::alignBytes;
  //    static constexpr size_t ALIGN_ELEM = Frame::frameSize;
  //    C *y = assume_aligned<Frame::alignBytes, C>(out);
  //    const C *x = assume_aligned<Frame::alignBytes, const C>(in);
  //
  //    for (size_t n = SKIP_ELEMENTS; n < count; n++) {
  //      C yN[CHANNELS];
  //      for (size_t i = 0; i < CHANNELS; i++) {
  //        yN[i] = c[0] * x[n * ALIGN_ELEM + i];
  //      }
  //      for (size_t j = 1; j <= ORDER; j++) {
  //        for (size_t i = 0; i < CHANNELS; i++) {
  //          yN[i] += x[n * ALIGN_ELEM - j * ALIGN_ELEM + i] * c[j] +
  //                   y[n * ALIGN_ELEM - j * ALIGN_ELEM + i] * d[j];
  //        }
  //      }
  //      for (size_t i = 0; i < CHANNELS; i++) {
  //        y[n * ALIGN_ELEM + i] = yN[i];
  //      }
  //    }
  //  }

private:
  Frame x[HISTORY_SIZE];
  Frame y[HISTORY_SIZE];
};

template <typename C, typename S, size_t ORDER, size_t CHANNELS,
          size_t ALIGN_SAMPLES = 4>
struct FixedOrderIirFrameFilterIO
    : public FixedOrderIirFrameFilterBase<C, ORDER, ALIGN_SAMPLES> {

  static_assert(CHANNELS > 0 && CHANNELS < 1024,
                "CHANNELS is not between 1 and 1024");

  using Frame = AlignedFrame<C, CHANNELS, ALIGN_SAMPLES>;

  void clearHistory() noexcept {
    for (size_t i = 0; i < HISTORY_SIZE; i++) {
      x[i].zero();
      y[i].zero();
    }
    t = 0;
  }

  inline void iir_filter_fixed() noexcept {
    output = c[0] * input;
    size_t idx = now();
    for (size_t i = 1; i <= ORDER; i++) {
      output += c[i] * x[idx];
      output += d[i] * y[idx];
      idx = past(idx);
    }
    idx = next();
    x[idx] = input;
    y[idx] = output;
  }

  Frame input;
  Frame output;

protected:
  using FixedOrderIirFrameFilterBase<C, ORDER, ALIGN_SAMPLES>::HISTORY_SIZE;
  using FixedOrderIirFrameFilterBase<C, ORDER, ALIGN_SAMPLES>::past;
  using FixedOrderIirFrameFilterBase<C, ORDER, ALIGN_SAMPLES>::now;
  using FixedOrderIirFrameFilterBase<C, ORDER, ALIGN_SAMPLES>::next;
  using FixedOrderIirFrameFilterBase<C, ORDER, ALIGN_SAMPLES>::c;
  using FixedOrderIirFrameFilterBase<C, ORDER, ALIGN_SAMPLES>::d;

private:
  Frame x[HISTORY_SIZE];
  Frame y[HISTORY_SIZE];
  size_t t = 0;
};

} // namespace tdap

#endif // TDAP_M_IIR_COEFFICIENTS_HPP
