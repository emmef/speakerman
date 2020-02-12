/*
 * tdap/TrueFloatingPointWindowAverage.hpp
 *
 * Part of TdAP
 * Time-domain Audio Processing
 * Copyright (C) 2015-2016 Michel Fleur.
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

#ifndef TDAP_TRUE_RMS_HEADER_GUARD
#define TDAP_TRUE_RMS_HEADER_GUARD

#include <iostream>

#include <cmath>
#include <type_traits>

#include <tdap/Array.hpp>
#include <tdap/Followers.hpp>
#include <tdap/Power2.hpp>

#define TRUE_RMS_QUOTE_1(x) #x
#define TRUE_RMS_QUOTE(t) TRUE_RMS_QUOTE_1(t)

namespace tdap {
using namespace std;

template <typename S> struct TrueMovingAverageErrors {

  static_assert(!std::is_floating_point<S>::value,
                "Sample type must be floating point");

  static constexpr double epsilon = std::numeric_limits<S>::epsilon();
  static constexpr double stabilityHeadroom = 0.01;

  [[nodiscard]] static constexpr double
  singleIntegrationError(size_t integrationSamples) {
    // error = epsilon / (1 - exp(-1/N))
    return epsilon / Integration::get_input_multiplier((S)integrationSamples);
  }

  [[nodiscard]] static constexpr size_t maximumStableIntegrationSamples() {
    return std::min(epsilon * stabilityHeadroom,
                    (double)std::numeric_limits<size_t>::max());
  }

  [[nodiscard]] static constexpr size_t sensibleIntegrationSamples(S samples) {
    return (size_t)std::min(samples, (S)maximumStableIntegrationSamples());
  }

  [[nodiscard]] static constexpr size_t
  sensibleIntegrationSamples(size_t samples) {
    return std::min(samples, maximumStableIntegrationSamples());
  }

  [[nodiscard]] static constexpr double
  integrationErrorForSamples(size_t integrationSamples) {
    // error1 = epsilon / (1 - exp(-1/N))
    double error1 = singleIntegrationError(integrationSamples);
    // consider errors as noise, RMS
    return sqrt(error1 * error1 * integrationSamples);
  }

  [[nodiscard]] static constexpr size_t
  samplesForIntegrationError(double error) {
    /**
     * Inverse of: error = sqrt((N * epsilon^2 / (1 - exp(-1/N)))^2)
     * Let's assume N is relatively and approximate:
     *                                 Error % for N =  100         10    +/-
     * exp(-1/N)              ~ 1 - 1/N                   0.005      0.5   +
     * => 1 - exp(-1/N)       ~ 1/N                       1          5     -
     * => (1 - exp(1/N))^2    ~ 1/N^2                     2         10     -
     * => 1/(1 - exp(1/N))^2  ~ N^2                       2         10     +
     * => error               ~ sqrt(N^3 * epsilon^2)     1          5     +
     * => error^2             ~ epsilon^2 * N^3           2         10     +
     * => N                   ~ pow(error/epsilon, 2/3)   2         10     -
     */
    double sampleEstimate = pow(fabs(error) / epsilon, 2.0 / 3.0);
    if (sampleEstimate < 1) {
      return 1;
    }
    /**
     * Knowing the approximate error, we can correct it with (1 + 1/N).
     */
    return sensibleIntegrationSamples(0.5 + sampleEstimate *
                                                (1.0 + 1.0 / sampleEstimate));
  }

  [[nodiscard]] static constexpr double
  additionErrorForSamples(size_t samplesToAdd) {
    return epsilon * samplesToAdd;
  }

  [[nodiscard]] static constexpr size_t samplesForAdditionError(double error) {
    return error / epsilon;
  }

  [[nodiscard]] static constexpr double errorForSamples(size_t samples) {
    return integrationErrorForSamples(samples) +
           additionErrorForSamples(samples);
  }

  [[nodiscard]] static constexpr size_t samplesForError(double error) {
    /*
     * See for approximations also #samplesForIntegrationError.
     * error               ~ sqrt(N^3 * epsilon^2) + epsilon * N
     * => error            ~ N * epsilon * (sqrt(N) + 1)
     *    sqrt(N) + 1      ~ sqrt(N)                       error = - N^(-1/2)
     * => error            ~ epsilon * N^(3/2)             error = - N^(-1/2)
     * => N                ~ pow(error/epsilon, 2/3)   error = ~ 2*N^(-1/2)/3
     */
    double sampleEstimate = pow(fabs(error) / epsilon, 2.0 / 3.0);
    if (sampleEstimate < 1) {
      return 1;
    }
    /**
     * Knowing the approximate error, we can correct it with
     * (lower bound) (1 + 2/3 * 1/sqrt(N))
     */
    return sensibleIntegrationSamples(0.5 * sampleEstimate *
                                      (1.0 + 0.66 / sqrt(sampleEstimate)));
  }
};

template <typename S, size_t SNR_BITS = 20,
          size_t MIN_ERROR_DECAY_TO_WINDOW_RATIO = 10>
struct MetricsForTrueFloatingPointMovingAverageMetyrics {
  static_assert(std::is_floating_point<S>::value,
                "Sample type must be floating point");

  static constexpr size_t MIN_SNR_BITS = 4;
  static constexpr size_t MAX_SNR_BITS = 44;
  static constexpr size_t MIN_MIN_ERROR_DECAY_TO_WINDOW_RATIO = 1;
  static constexpr size_t MAX_MIN_ERROR_DECAY_TO_WINDOW_RATIO = 1000;
  static_assert(
      SNR_BITS >= MIN_SNR_BITS && SNR_BITS <= MAX_SNR_BITS,
      "Number of signal-noise-ratio in bits must lie between" TRUE_RMS_QUOTE(
          MIN_SNR_BITS) " and " TRUE_RMS_QUOTE(MAX_SNR_BITS) ".");

  static_assert(Sizes::is_between(MIN_ERROR_DECAY_TO_WINDOW_RATIO,
                                  MIN_MIN_ERROR_DECAY_TO_WINDOW_RATIO,
                                  MAX_MIN_ERROR_DECAY_TO_WINDOW_RATIO),
                "Minimum error decay to window size ratio must lie "
                "between " TRUE_RMS_QUOTE(MIN_MIN_ERROR_DECAY_TO_WINDOW_RATIO) " and " TRUE_RMS_QUOTE(
                    MAX_MIN_ERROR_DECAY_TO_WINDOW_RATIO));

  static constexpr size_t MAX_ERR_MITIGATING_DECAY_SAMPLES = Value<double>::min(
      0.01 / numeric_limits<S>::epsilon(), numeric_limits<size_t>::max());
  static constexpr size_t MAX_WINDOWS_SIZE_BOUNDARY =
      MAX_ERR_MITIGATING_DECAY_SAMPLES / MIN_ERROR_DECAY_TO_WINDOW_RATIO;
  static constexpr const char *ERR_MITIGATING_DECAY_SAMPLES_EXCEEDED_MESSAGE =
      "The decay time-constant (in samples) for error mitigation must be "
      "smaller than " TRUE_RMS_QUOTE(MAX_ERR_MITIGATING_DECAY_SAMPLES) ".";
  static constexpr size_t MIN_MAX_WINDOW_SAMPLES = 64;
  static constexpr size_t MAX_MAX_WINDOW_SAMPLES =
      Floats::min(constexpr_power<double, SNR_BITS + 1>(0.5) /
                      std::numeric_limits<S>::epsilon(),
                  MAX_WINDOWS_SIZE_BOUNDARY);
  static constexpr size_t MIN_ERR_MITIGATING_DECAY_SAMPLES =
      MIN_ERROR_DECAY_TO_WINDOW_RATIO * MIN_MAX_WINDOW_SAMPLES;

public:
  static constexpr const size_t getMinimumWindowSizeInSamples() {
    return MIN_MAX_WINDOW_SAMPLES;
  }

  static constexpr const size_t getMaximumWindowSizeInSamples() {
    return MAX_MAX_WINDOW_SAMPLES;
  }

  static bool isValidWindowSizeInSamples(size_t samples) {
    return Sizes::is_between(samples, MIN_MAX_WINDOW_SAMPLES,
                             MAX_MAX_WINDOW_SAMPLES);
  }

  static constexpr const char *getWindowSizeInSamplesRangeMessage() {
    return "RMS window size in samples must lie between " TRUE_RMS_QUOTE(MIN_MAX_WINDOW_SAMPLES) " and " TRUE_RMS_QUOTE(
        MAX_MAX_WINDOW_SAMPLES) " for minimum of " TRUE_RMS_QUOTE(SNR_BITS) " b"
                                                                            "it"
                                                                            "s "
                                                                            "of"
                                                                            " s"
                                                                            "ig"
                                                                            "na"
                                                                            "l "
                                                                            "to"
                                                                            " e"
                                                                            "rr"
                                                                            "or"
                                                                            "-n"
                                                                            "oi"
                                                                            "se"
                                                                            " r"
                                                                            "at"
                                                                            "io"
                                                                            " a"
                                                                            "nd"
                                                                            " s"
                                                                            "am"
                                                                            "pl"
                                                                            "e "
                                                                            "ty"
                                                                            "pe"
                                                                            " " TRUE_RMS_QUOTE(
                                                                                typename S);
  }

  static size_t validWindowSizeInSamples(size_t samples) {
    if (isValidWindowSizeInSamples(samples)) {
      return samples;
    }
    throw std::invalid_argument(getWindowSizeInSamplesRangeMessage());
  }

  static constexpr const size_t getMaximumErrorMitigatingDecaySamples() {
    return MAX_ERR_MITIGATING_DECAY_SAMPLES;
  }

  static constexpr const size_t getMinimumErrorMitigatingDecaySamples() {
    return MIN_ERR_MITIGATING_DECAY_SAMPLES;
  }

  static bool isValidErrorMitigatingDecaySamples(size_t samples) {
    return Sizes::is_between(samples, MIN_ERR_MITIGATING_DECAY_SAMPLES,
                             MAX_ERR_MITIGATING_DECAY_SAMPLES);
  }

  static constexpr const char *getErrorMitigatingDecaySamplesRangeMessage() {
    return "Error mitigating decay samples must lie between " TRUE_RMS_QUOTE(MIN_ERR_MITIGATING_DECAY_SAMPLES) " and " TRUE_RMS_QUOTE(
        MAX_ERR_MITIGATING_DECAY_SAMPLES) " for sample type " TRUE_RMS_QUOTE(typename S) ".";
  }

  static size_t validErrorMitigatingDecaySamples(size_t samples) {
    if (isValidErrorMitigatingDecaySamples(samples)) {
      return samples;
    }
    throw std::invalid_argument(getErrorMitigatingDecaySamplesRangeMessage());
  }

  static constexpr size_t getMinimumErrorMitigatingDecayToWindowSizeRation() {
    return MIN_ERROR_DECAY_TO_WINDOW_RATIO;
  }
};

template <typename S> class WindowForTrueFloatingPointMovingAverage;

template <typename S> class BaseHistoryAndEmdForTrueFloatingPointMovingAverage {
  const size_t historySamples_;
  S *const history_;
  const size_t emdSamples_;
  const S emdFactor_;
  size_t optimizedHistorySamples_;
  size_t writePtr_ = 0;

protected:
  BaseHistoryAndEmdForTrueFloatingPointMovingAverage(
      const size_t historySamples, const size_t emdSamples)
      : historySamples_(historySamples), history_(new S[historySamples]),
        emdSamples_(emdSamples), emdFactor_(exp(-1.0 / emdSamples)),
        optimizedHistorySamples_(historySamples), writePtr_(0) {}
  inline void setNextPtr(size_t &ptr) const {
    if (ptr > 0) {
      ptr--;
    } else
      ptr = optimizedHistorySamples_;
  }

public:
  size_t historySize() const { return historySamples_; }
  size_t maxHistorySize() const { return historySamples_; }
  size_t emdSamples() const { return emdSamples_; }
  size_t writePtr() const { return writePtr_; }
  size_t maxWindowSamples() const { return optimizedHistorySamples_; }
  S emdFactor() const { return emdFactor_; }

  inline size_t getRelative(size_t delta) const {
    return (writePtr_ + delta) % optimizedHistorySamples_;
  }
  const S getHistoryValue(size_t &readPtr) const {
    S result = history_[readPtr];
    setNextPtr(readPtr);
    return result;
  }
  const S get(size_t index) const {
    return history_[IndexPolicy::method(index, optimizedHistorySamples_)];
  }
  const S get() const { return get(writePtr_); }
  const S operator[](size_t index) const {
    return history_[IndexPolicy::method(index, optimizedHistorySamples_)];
  }
  void set(size_t index, S value) {
    history_[IndexPolicy::method(index, optimizedHistorySamples_)] = value;
  }
  void write(S value) {
    history_[writePtr_] = value;
    setNextPtr(writePtr_);
  }
  S &operator[](size_t index) {
    return history_[IndexPolicy::array(index, optimizedHistorySamples_)];
  }
  void fillWithAverage(const S average) {
    for (size_t i = 0; i < historySamples_; i++) {
      history_[i] = average;
    }
  }
  const S *const history() const { return history_; }
  S *const history() { return history_; }

  bool optimiseForMaximumWindowSamples(size_t samples) {
    size_t newHistoryEnd = Sizes::force_between(samples, 4, historySamples_);
    if (newHistoryEnd != optimizedHistorySamples_) {
      optimizedHistorySamples_ = newHistoryEnd;
      return true;
    }
    return false;
  }

  ~BaseHistoryAndEmdForTrueFloatingPointMovingAverage() { delete[] history_; }
};

template <typename S> class WindowForTrueFloatingPointMovingAverage {
  const BaseHistoryAndEmdForTrueFloatingPointMovingAverage<S> *history_ =
      nullptr;
  size_t windowSamples_ = 1;
  S inputFactor_ = 1;
  S historyFactor_ = 1;
  size_t readPtr_ = 1;
  S average_ = 0;

public:
  WindowForTrueFloatingPointMovingAverage() {}
  WindowForTrueFloatingPointMovingAverage(
      const BaseHistoryAndEmdForTrueFloatingPointMovingAverage<S> *history)
      : history_(history) {}

  void setOwner(
      const BaseHistoryAndEmdForTrueFloatingPointMovingAverage<S> *history) {
    if (history_ == nullptr) {
      history_ = history;
      return;
    }
    throw std::runtime_error("Window already owned by other history");
  }

  bool isOwnedBy(const BaseHistoryAndEmdForTrueFloatingPointMovingAverage<S>
                     *owner) const {
    bool b = owner == history_;
    return b;
  }

  S getAverage() const { return average_; }

  size_t windowSamples() const { return windowSamples_; }

  size_t getReadPtr() const { return readPtr_; }

  const BaseHistoryAndEmdForTrueFloatingPointMovingAverage<S> *owner() const {
    return history_;
  }

  void setAverage(S average) { average_ = average; }
  void setWindowSamples(size_t windowSamples) {
    if (history_ == nullptr) {
      throw std::runtime_error(
          "WindowForTrueFloatingPointMovingAverage::setWindowSamples(): window "
          "not related to history data");
    }
    if (!Sizes::is_between(windowSamples, 1, history_->historySize())) {
      throw std::runtime_error(
          "WindowForTrueFloatingPointMovingAverage: window samples must lie "
          "between 1 and history's maximum size");
    }
    windowSamples_ = windowSamples;
    const double unscaledHistoryDecayFactor =
        exp(-1.0 * this->windowSamples_ / history_->emdSamples());
    inputFactor_ =
        (1.0 - history_->emdFactor()) / (1.0 - unscaledHistoryDecayFactor);
    historyFactor_ = inputFactor_ * unscaledHistoryDecayFactor;
    setReadPtr();
  }
  void setReadPtr() {
    if (windowSamples_ <= history_->maxWindowSamples()) {
      readPtr_ = history_->getRelative(windowSamples_);
      return;
    }
    throw std::runtime_error("RMS window size cannot be bigger than buffer");
  }

  void addInput(S input) {
    S history = history_->getHistoryValue(readPtr_);
    average_ = history_->emdFactor() * average_ + inputFactor_ * input -
               historyFactor_ * history;
  }
};

template <typename S>
class ScaledWindowForTrueFloatingPointMovingAverage
    : public WindowForTrueFloatingPointMovingAverage<S> {
  using Super = WindowForTrueFloatingPointMovingAverage<S>;
  S scale_ = 1;

public:
  ScaledWindowForTrueFloatingPointMovingAverage() {}

  ScaledWindowForTrueFloatingPointMovingAverage(
      const BaseHistoryAndEmdForTrueFloatingPointMovingAverage<S> &history)
      : Super(&history) {}

  S setScale(S scale) {
    if (fabs(scale) < 1e-12) {
      scale_ = 0.0;
    } else if (scale > 1e12) {
      scale_ = scale;
    } else if (scale < -1e12) {
      scale_ = -1e12;
    } else {
      scale_ = scale;
    }
    return scale;
  }

  const S scale() const { return scale_; }

  void setWindowSamplesAndScale(size_t windowSamples, S scale) {
    Super::setWindowSamples(windowSamples);
    setScale(scale);
  }

  const S getAverage() const { return scale_ * Super::getAverage(); }

  void setOutput(S outputValue) {
    Super::setAverage(scale_ != 0.0 ? outputValue / scale_ : outputValue);
  }
};

template <typename S, size_t SNR_BITS = 20,
          size_t MIN_ERROR_DECAY_TO_WINDOW_RATIO = 10>
class HistoryAndEmdForTrueFloatingPointMovingAverage
    : public BaseHistoryAndEmdForTrueFloatingPointMovingAverage<S> {
  using Metrics_ = MetricsForTrueFloatingPointMovingAverageMetyrics<
      S, SNR_BITS, MIN_ERROR_DECAY_TO_WINDOW_RATIO>;
  using Super = BaseHistoryAndEmdForTrueFloatingPointMovingAverage<S>;

  static size_t validWindowSize(size_t emdSamples, size_t windowSize) {
    if (Metrics_::validWindowSizeInSamples(windowSize) <
        emdSamples / Metrics_::MIN_MIN_ERROR_DECAY_TO_WINDOW_RATIO) {
      return windowSize;
    }
    throw std::invalid_argument(
        "Invalid combination of window size and ratio between that and error "
        "mitigating decay samples.");
  }

public:
  HistoryAndEmdForTrueFloatingPointMovingAverage(const size_t historySamples,
                                                 const size_t emdSamples)
      : Super(validWindowSize(
                  Metrics_::validErrorMitigatingDecaySamples(emdSamples),
                  historySamples),
              emdSamples) {}
};

template <typename S, size_t SNR_BITS = 20,
          size_t MIN_ERROR_DECAY_TO_WINDOW_RATIO = 10>
class TrueFloatingPointWeightedMovingAverage {
  using History = HistoryAndEmdForTrueFloatingPointMovingAverage<
      S, SNR_BITS, MIN_ERROR_DECAY_TO_WINDOW_RATIO>;
  using Window = WindowForTrueFloatingPointMovingAverage<S>;

  History history;
  Window window;

  void optimiseForMaximumSamples() {
    if (history.optimiseForMaximumWindowSamples(window.windowSamples())) {
      window.setReadPtr();
    }
  }

public:
  TrueFloatingPointWeightedMovingAverage(const size_t maxWindowSize,
                                         const size_t emdSamples)
      : history(maxWindowSize, emdSamples), window(&history) {
    window.setWindowSamples(maxWindowSize);
  }

  void setAverage(const double average) {
    window.setAverage(average);
    history.fillWithAverage(average);
  }

  void setWindowSize(const size_t windowSamples) {
    window.setWindowSamples(windowSamples);
    optimiseForMaximumSamples();
  }

  void addInput(const double input) {
    window.addInput(input);
    history.write(input);
  }

  const S getAverage() const { return window.getAverage(); }

  const size_t getReadPtr() const { return window.getReadPtr(); }
  const size_t getWritePtr() const { return history.writePtr(); }
  const S getNextHistoryValue() const {
    return history.history()[window.getReadPtr()];
  }
};
/**
 * Implements a true windowed average. This is obtained by adding a new
 * sample to a running average and subtracting the value of exactly the
 * window size in the past - kept in history.
 *
 * This algorithm is efficient and it is easy to combine an array of
 * different window sizes. However, the efficiency comes with an inherent
 * problem of addition/subtraction errors as a result of limited
 * floating-point precision. To mitigate this, both the running average and
 * all history values have an appropriate "natural decay" applied to them,
 * effectively zeroing vaues that are much older than the window size.
 *
 * This mitigating decay also suffers from imprecision and causes a
 * measurement "noise". As a rule of thumb, this noise should stay
 * approximately three orders of magnitude below average input.
 * In order to
 * @tparam S the type of samples used, normally "double"
 * @tparam MAX_SAMPLE_HISTORY the maximum sample history, determining the
 * maximum RMS window size
 * @tparam MAX_RCS the maximum number of characteristic times in this array
 */
template <typename S, size_t SNR_BITS = 20,
          size_t MIN_ERROR_DECAY_TO_WINDOW_RATIO = 10>
class TrueFloatingPointWeightedMovingAverageSet {
  using History = HistoryAndEmdForTrueFloatingPointMovingAverage<
      S, SNR_BITS, MIN_ERROR_DECAY_TO_WINDOW_RATIO>;
  using Window = ScaledWindowForTrueFloatingPointMovingAverage<S>;

  static constexpr size_t MINIMUM_TIME_CONSTANTS = 1;
  static constexpr size_t MAXIMUM_TIME_CONSTANTS = 32;
  static constexpr const char *TIME_CONSTANT_MESSAGE =
      "The (maximum) number of time-constants must lie between " TRUE_RMS_QUOTE(
          MINIMUM_TIME_CONSTANTS) " and " TRUE_RMS_QUOTE(MAXIMUM_TIME_CONSTANTS) ".";

  using Metrics = MetricsForTrueFloatingPointMovingAverageMetyrics<
      S, SNR_BITS, MIN_ERROR_DECAY_TO_WINDOW_RATIO>;

  const size_t entries_;
  Window *entry_;
  size_t usedWindows_;
  History history_;

  static size_t validMaxTimeConstants(size_t constants) {
    if (Sizes::is_between(constants, MINIMUM_TIME_CONSTANTS,
                          MAXIMUM_TIME_CONSTANTS)) {
      return constants;
    }
    throw std::invalid_argument(TIME_CONSTANT_MESSAGE);
  }

  size_t checkWindowIndex(size_t index) const {
    if (index < getUsedWindows()) {
      return index;
    }
    throw std::out_of_range(
        "Window index greater than configured windows to use");
  }

  void optimiseForMaximumSamples() {
    size_t maximumSamples = 0;
    for (size_t i = 0; i < usedWindows_; i++) {
      maximumSamples = Sizes::max(maximumSamples, entry_[i].windowSamples());
    }
    if (history_.optimiseForMaximumWindowSamples(maximumSamples)) {
      for (size_t i = 0; i < usedWindows_; i++) {
        entry_[i].setReadPtr();
      }
    }
  }

public:
  TrueFloatingPointWeightedMovingAverageSet(size_t maxWindowSamples,
                                            size_t errorMitigatingTimeConstant,
                                            size_t maxTimeConstants, S average)
      : entries_(validMaxTimeConstants(maxTimeConstants)),
        entry_(new Window[entries_]), usedWindows_(entries_),
        history_(maxWindowSamples, errorMitigatingTimeConstant) {
    history_.fillWithAverage(average);
    for (size_t i = 0; i < entries_; i++) {
      entry_[i].setOwner(&history_);
      entry_[i].setAverage(0);
      entry_[i].setWindowSamplesAndScale((i + 1) * maxWindowSamples / entries_,
                                         1.0);
    }
  }

  size_t getMaxWindows() const { return entries_; }
  size_t getUsedWindows() const { return usedWindows_; }
  size_t getMaxWindowSamples() const { return history_.historySize(); }

  void setUsedWindows(size_t windows) {
    if (windows > 0 && windows <= getMaxWindows()) {
      usedWindows_ = windows;
      optimiseForMaximumSamples();
    } else {
      throw std::out_of_range("Number of used windows zero or larger than "
                              "condigured maximum at construction");
    }
  }

  void setWindowSizeAndScale(size_t index, size_t windowSamples, S scale) {
    if (windowSamples > getMaxWindowSamples()) {
      throw std::out_of_range("Window size in samples is larger than "
                              "configured maximum at construction.");
    }
    entry_[checkWindowIndex(index)].setWindowSamplesAndScale(windowSamples,
                                                             scale);
    optimiseForMaximumSamples();
  }

  void setAverages(S average) {
    for (size_t i = 0; i < entries_; i++) {
      entry_[i].setAverage(average);
    }
    history_.fillWithAverage(average);
  }

  S getAverage(size_t index) const {
    return entry_[checkWindowIndex(index)].getAverage();
  }

  size_t getWindowSize(size_t index) const {
    return entry_[checkWindowIndex(index)].windowSamples();
  }

  S getWindowScale(size_t index) const {
    checkWindowIndex(index);
    return entry_[index].scale();
  }

  const S get() const { return history_.get(); }

  void addInput(S input) {
    for (size_t i = 0; i < getUsedWindows(); i++) {
      entry_[i].addInput(input);
    }
    history_.write(input);
  }

  S addInputGetMax(S const input, S minimumValue) {
    S average = minimumValue;
    for (size_t i = 0; i < getUsedWindows(); i++) {
      Window &entry = entry_[i];
      entry.addInput(input);
      const S v1 = entry.getAverage();
      average = Values::max(v1, average);
    }
    history_.write(input);
    return average;
  }

  size_t getWritePtr() const { return history_.writePtr(); }

  size_t getReadPtr(size_t i) const { return entry_[i].getReadPtr(); }

  ~TrueFloatingPointWeightedMovingAverageSet() { delete[] entry_; }
};

} // namespace tdap

#endif /* TDAP_TRUE_RMS_HEADER_GUARD */
