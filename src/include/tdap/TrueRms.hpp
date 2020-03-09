#ifndef SPEAKERMAN_TRUERMS_HPP
#define SPEAKERMAN_TRUERMS_HPP
/*
 * speakerman/TrueRms.hpp
 *
 * Added by michel on 2020-02-23
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

#include <algorithm>
#include <cstddef>
#include <limits>
#include <type_traits>

#include <tdap/Count.hpp>
#include <tdap/IndexPolicy.hpp>
#include <tdap/Power2.hpp>

namespace tdap {

namespace helper {
namespace {

class SingleWindowAveragePointers {
  size_t r = 0;

public:
  struct Ops {
    tdap_force_inline static bool
    setReadPtrZeroWrite(size_t &readPtr, size_t delay, size_t mask) {
      if (delay <= 0 || delay - 1 >= mask) {
        return false;
      }
      return readPtr = delay & mask;
    }

    tdap_force_inline static bool
    getWindowSamples(size_t readPtr, size_t writePtr, size_t mask) noexcept {
      if (writePtr < readPtr) {
        return readPtr - writePtr;
      } else if (writePtr != readPtr) {
        return readPtr + mask + 1 - writePtr;
      }
      return mask + 1;
    }

    tdap_force_inline static void nextPtr(size_t &ptr, size_t mask) noexcept {
      ptr = ++ptr & mask;
    }

    template <typename Scale>
    tdap_nodiscard static Scale getScaleFactor(Scale scale,
                                               size_t windowSamples) noexcept {
      static_assert(std::is_floating_point<Scale>::value,
                    "Scale is not a floating-point type.");
      return scale / windowSamples;
    }

    template <typename Scale>
    tdap_nodiscard static Scale getScale(Scale scaleFactor,
                                         size_t windowSamples) noexcept {
      static_assert(std::is_floating_point<Scale>::value,
                    "Scale is not a floating-point type.");
      return scaleFactor * windowSamples;
    }
  };

  tdap_nodiscard size_t read() const noexcept { return r; }

  tdap_nodiscard size_t getWindowSamples(size_t w, size_t mask) const noexcept {
    return Ops::getWindowSamples(r, w, mask);
  }

  template <typename Scale>
  tdap_nodiscard Scale getScaleFactor(Scale scale, size_t w,
                                      size_t mask) noexcept {
    return Ops::getScaleFactor(scale, getWindowSamples(w, mask));
  }

  template <typename Scale>
  tdap_nodiscard Scale getScale(Scale scaleFactor, size_t w,
                                size_t mask) noexcept {
    return Ops::getScale(scaleFactor, getWindowSamples(w, mask));
  }

  void next(size_t mask) { Ops::nextPtr(r, mask); }

  bool setWindowSamples(size_t samples, size_t mask) {
    if (Ops::setReadPtrZeroWrite(r, samples, mask)) {
      return true;
    }
    return false;
  }
};

template <typename Scale> class SingleWindowAverageScaleAndPointers {
  static_assert(std::is_floating_point<Scale>::value,
                "Scale is not a floating-point type.");

  SingleWindowAveragePointers pointers;
  Scale factor = 1.0;

public:
  tdap_nodiscard size_t read() const noexcept { return pointers.read(); }

  tdap_nodiscard size_t write() const noexcept { return pointers.write(); }

  void next(size_t mask) { pointers.next(); }

  tdap_nodiscard size_t getWindowSamples(size_t w, size_t mask) const noexcept {
    return pointers.getWindowSamples(w, mask);
  }

  tdap_nodiscard Scale getScale(size_t w, size_t mask) const noexcept {
    return pointers.getScale(factor, w, mask);
  }

  tdap_nodiscard Scale getScaleFactor() const noexcept { return factor; }

  bool setWindowSamplesAndScale(size_t samples, Scale scale,
                                size_t mask) noexcept {
    if (pointers.setWindowSamples(samples, mask)) {
      factor = SingleWindowAveragePointers::Ops::getScaleFactor(scale, samples);
      return true;
    }
    return false;
  }

  bool setWindowSamples(size_t samples, size_t mask) noexcept {
    Scale scale = getScale(mask);
    if (pointers.setWindowSamples(samples, mask)) {
      factor = SingleWindowAveragePointers::Ops::getScaleFactor(scale, samples);
      return true;
    }
    return false;
  }

  void setScale(Scale scale, size_t w, size_t mask) noexcept {
    factor = pointers.getScaleFactor(scale, w, mask);
  }

  template <typename Sum>
  tdap_nodiscard Sum getSum(const Sum *data, Sum sum, Sum sample) {
    return sum + sample - data[pointers.read()];
  }

  template <typename Sum>
  tdap_nodiscard Scale getAverage(const Sum *data, Sum sum, Sum sample) {
    return factor * getSum(data, sum, sample);
  }
};

template <typename Sum, bool isSigned> class BaseSumValueClamper;

template <typename Sum> class BaseSumValueClamper<Sum, true> {
  static constexpr Sum max = std::numeric_limits<Sum>::max();
  static constexpr Sum min = -std::numeric_limits<Sum>::lowest();
  static_assert(min < 0 && max > 0 && -max <= min,
                "Sum type does not have suitable behaviour for minimum and "
                "maximum values");

  Sum lo = min;
  Sum hi = max;

public:
  tdap_nodiscard Sum getLimit() const noexcept { return hi; }

  bool setLimit(Sum value) noexcept {
    if (value < 1) {
      return false;
    }
    hi = value;
    lo = -hi;
  }

  tdap_nodiscard Sum clamp(Sum value) const noexcept {
    return std::clamp(value, lo, hi);
  }
};

template <typename Sum> class BaseSumValueClamper<Sum, false> {
  static constexpr Sum max = std::numeric_limits<Sum>::max();

  Sum hi = max;

public:
  tdap_nodiscard Sum getLimit() const noexcept { return hi; }

  bool setLimit(Sum value) noexcept { hi = value; }

  tdap_nodiscard Sum clamp(Sum value) const noexcept {
    return std::min(value, hi);
  }
};

template <typename Sum> class SumValueClamper {
  BaseSumValueClamper<Sum, std::is_signed<Sum>::value> clamper;

public:
  inline bool setLimit(Sum value) noexcept { clamper.setLimit(value); }

  tdap_nodiscard inline Sum getLimit() const noexcept {
    return clamper.getLimit();
  }

  tdap_nodiscard inline Sum clamp(Sum value) const noexcept {
    return clamper.clamp(value);
  }
};

template <typename Sum> class AverageWindowMetrics {
  static_assert(std::is_integral<Sum>::value,
                "Sum is not of an integral type.");

public:
  static constexpr Sum max = std::numeric_limits<Sum>::max();

  static size_t getMaxWindowSamples(Sum maxSampleValue,
                                    Sum headRoomFactor = 1) noexcept {
    if (maxSampleValue <= 0 || headRoomFactor <= 0) {
      return 0;
    }
    Sum maxHrTimesW = max / maxSampleValue;
    if (maxHrTimesW < headRoomFactor) {
      return 0;
    }
    return maxHrTimesW / headRoomFactor;
  }

  static size_t getMaxSampleValue(Sum maxWindowSize,
                                  Sum headRoomFactor = 1) noexcept {
    return getMaxWindowSamples(maxWindowSize, headRoomFactor);
  }

  template <typename Scale>
  tdap_force_inline static Scale getScale(Scale scaleFactor,
                                          size_t sumSamples) noexcept {
    return scaleFactor * sumSamples;
  }

  template <typename Scale>
  tdap_force_inline static Scale getScaleFactor(Scale scale,
                                                size_t sumSamples) noexcept {
    return scale / sumSamples;
  }

  template <typename Scale>
  tdap_force_inline static Scale getAverage(Sum sum,
                                            Scale scaleFactor) noexcept {
    return scaleFactor * sum;
  }
};

} // namespace
} // namespace helper

template <typename Sum, typename Scale> struct SingleAverageWindow {
  size_t mask;
  size_t write = 0;
  Sum sum;
  Sum *data;
  helper::SingleWindowAverageScaleAndPointers<Scale> entry;
  helper::SumValueClamper<Sum> clamper;

  void initWindowSamples(size_t samples) {
    write = 0;
    clamper.setLimit(Metrics::getMaxSampleValue(samples));
    for (size_t i = 0; i < samples; i++) {
      data[i] = 0;
    }
  }

public:
  using Metrics = helper::AverageWindowMetrics<Sum>;

  SingleAverageWindow(size_t maxSamples)
      : mask(tdap::Power2::next(std::max(maxSamples, 3ul)) - 1), sum(0),
        data(new Sum[mask + 1]) {
    entry.setWindowSamplesAndScale(maxSamples, 1.0, mask);
  }

  ~SingleAverageWindow() {
    if (data) {
      delete[] data;
      mask = 0;
      data = 0;
    }
  }

  tdap_nodiscard size_t getMaximumSamplesPerWindow() const noexcept {
    return mask + 1;
  }

  tdap_nodiscard size_t getWindowSamples() const noexcept {
    return entry.getWindowSamples(mask);
  }

  tdap_nodiscard Scale getScale() const noexcept {
    return entry.getScale(mask);
  }

  tdap_nodiscard Sum getMaximumInputValue() const noexcept {
    return clamper.getLimit();
  }

  void setScale(Scale scale) noexcept { entry.setScale(scale, write, mask); }

  bool setWindowSamples(size_t samples) noexcept {
    if (entry.setWindowSamples(samples, mask)) {
      initWindowSamples(samples);
      return true;
    }
    return false;
  }

  bool setWindowSamplesAndScale(size_t samples, Scale scale) noexcept {
    if (entry.setWindowSamplesAndScale(samples, scale, mask)) {
      initWindowSamples(samples);
      return true;
    }
    return false;
  }

  tdap_nodiscard Scale setAndGet(Sum sample) noexcept {
    sum = entry.getSumUpdate(data, sum, clamper.clamp(sample));
    data[write] = sample;
    entry.next(mask);
    helper::SingleWindowAveragePointers::Ops::nextPtr(write, mask);
    return get();
  }

  tdap_nodiscard Scale get() const noexcept {
    return entry.getScaleFactor() * sum;
  }
};

template <typename Sum, typename Scale, size_t ALIGN> class MultiAverage {
  static constexpr size_t MAX_CHANNELS = 4096;
  static_assert(Power2::constant::is(ALIGN) && ALIGN < MAX_CHANNELS,
                "Alignment must be a power of two of 4096 or smaller.");

  static constexpr size_t alignBytes = sizeof(Sum) * ALIGN;

  using SumPtr = Sum *;
  enum class State { CONFIGURING, RUNNING };

  State state = State::CONFIGURING;
  size_t maxChannels;
  size_t maxWinSamples;
  size_t maxNumberOfWindowSizes;
  size_t memoryElements;

  size_t mask;
  size_t channels;
  size_t alignedChannels;
  size_t channelMask;
  size_t winSizes;
  size_t groups;
  helper::SumValueClamper<Sum> clamper;
  SumPtr start;
  SumPtr end;
  SumPtr write;
  SumPtr input;
  SumPtr sum;
  size_t *map;
  SumPtr *read;
  Scale *output;
  Scale *scaleFactor;
  Scale *scaledData;
  Sum *sumData;

  static size_t getAlignedChannels(size_t maxChannels) {
    return Power2::next(Power2::aligned_with(maxChannels, ALIGN));
  }

  void clearElements() {
    size_t elements = channels * (mask + 2);
    for (size_t i = 0; i < elements; i++) {
      sumData[i] = 0;
    }
  }

  void next(Sum *&ptr) {
    ptr += alignedChannels;
    if (ptr >= end) {
      ptr = start;
    }
  }

public:
  static constexpr size_t limitOfChannels = MAX_CHANNELS;
  static constexpr size_t limitOfWindowsSizes = 128;

  static bool isValidMaximumChannels(size_t maxChannels) {
    return !maxChannels || maxChannels <= limitOfChannels;
  }

  static bool isValidMaximumChannelsAndWindowSamples(size_t maxChannels,
                                                     size_t windowSamples) {
    return isValidMaximumChannels(maxChannels) &&
           Count<Sum>::max() / maxChannels / 4 >= windowSamples;
  }

  static bool isValidNumberOfWindowSizes(size_t sizes) {
    return sizes > 0 && sizes <= limitOfWindowsSizes;
  }

  static size_t validMaxChannels(size_t maxChannels) {
    if (isValidMaximumChannels(maxChannels)) {
      return getAlignedChannels(maxChannels);
    }
    throw std::out_of_range(
        "MultiAverage: maximum number of channels zero or larger than 4096.");
  }

  static size_t validMaxWindowSamples(size_t maxWindowSamples,
                                      size_t channels) {
    if (isValidMaximumChannelsAndWindowSamples(channels, maxWindowSamples)) {
      return Power2::next(maxWindowSamples);
    }
    throw std::out_of_range("MultiAverage: maximum number of samples per "
                            "window zero or too high in "
                            "combination with maximum number of channels.");
  }

  static size_t validNumberOfWindowSizes(size_t sizes) {
    if (isValidNumberOfWindowSizes(sizes)) {
      return sizes;
    }
    throw std::out_of_range("MultiAverage: maximum number of window sizes zero "
                            "or larger than 128.");
  }

  using Metrics = helper::AverageWindowMetrics<Sum>;

  MultiAverage(size_t channels, size_t maxWindowSamples,
               size_t numberOfWindowSizes)
      : maxChannels(validMaxChannels(channels)),
        maxWinSamples(validMaxWindowSamples(maxWindowSamples, maxChannels)),
        maxNumberOfWindowSizes(validNumberOfWindowSizes(numberOfWindowSizes)),
        memoryElements(maxChannels * 2 + maxChannels * maxNumberOfWindowSizes +
                       maxChannels * maxWinSamples),
        mask(maxChannels * maxWindowSamples - 1), channels(maxChannels),
        map(new size_t[maxChannels]), read(new SumPtr[maxNumberOfWindowSizes]),
        scaledData(new Scale[2 * maxChannels + ALIGN]),
        sumData(new Sum[memoryElements + ALIGN]),
        scaleFactor(new Scale[maxNumberOfWindowSizes]) {
    setDimensions(channels, maxWindowSamples, numberOfWindowSizes, 1.0);
  }

  void setDimensions(size_t newChannels, size_t windowSamples,
                     size_t windowSizes, Scale scale) {
    if (state != State::CONFIGURING) {
      throw std::runtime_error("MultiAverage: cannot change dimensions this "
                               "way when not configuring.");
    }

    if (newChannels == 0 || newChannels > maxChannels) {
      throw std::out_of_range("MultiAverage: number of channels is zero or "
                              "exceeds constructed maximum.");
    }
    if (windowSamples > maxWinSamples) {
      throw std::out_of_range("MultiAverage: number of window samples is zero "
                              "or exceeds constructed maximum.");
    }
    if (windowSizes > maxNumberOfWindowSizes) {
      throw std::out_of_range("MultiAverage: number of window sizes is zero "
                              "or exceeds constructed maximum.");
    }
    channels = getAlignedChannels(newChannels);
    alignedChannels = getAlignedChannels(channels);
    channelMask = alignedChannels - 1;
    size_t maxSamples = Power2::next(windowSamples);
    mask = maxSamples - 1;
    winSizes = windowSizes;
    output = tdap::Power2::ptr_aligned_with(scaledData, sizeof(Scale) * ALIGN);
    input = tdap::Power2::ptr_aligned_with(sumData, sizeof(Sum) * ALIGN);
    sum = input + alignedChannels;
    size_t sumCount = alignedChannels * winSizes;
    start = sum + sumCount;
    end = start + maxSamples * alignedChannels;
    for (SumPtr p = sumData; p < end; p++) {
      *p = 0;
    }
    for (size_t m = 0; m < channels; m++) {
      map[m] = 0;
    }
    groups = 0;
    write = start;
    Scale factor = helper::SingleWindowAveragePointers::Ops::getScaleFactor(
        scale, maxSamples);
    for (size_t s = 0; s < windowSizes; s++) {
      read[s] = write;
      scaleFactor[s] = factor;
    }
  }

  void setSamplesAndScale(size_t index, size_t samples, Scale scale) {
    if (state != State::CONFIGURING) {
      throw std::runtime_error(
          "MultiAverage: cannot change window samples or scales this way when "
          "not configuring.");
    }
    if (!samples || samples > mask + 1) {
      throw std::out_of_range("MultiAverage: number of window samples is zero "
                              "or exceeds configured maximum.");
    }
    if (index >= winSizes) {
      throw std::out_of_range("MultiAverage: index is too large for numbers of "
                              "configured window sizes.");
    }
    read[index] = start + samples * alignedChannels;
    scaleFactor[index] =
        helper::SingleWindowAveragePointers::Ops::getScaleFactor(scale,
                                                                 samples);
  }

  void mapChannelOnOutput(size_t channel, size_t output) {
    if (state != State::CONFIGURING) {
      throw std::runtime_error(
          "MultiAverage: cannot change channel mapping when not configuring.");
    }
    if (channel >= channels) {
      throw std::out_of_range(
          "MultiAverage: channel to map exceeds number of configured channels");
    }
    if (output >= channels) {
      throw std::out_of_range(
          "MultiAverage: output to map to exceeds number of configured channels");
    }
    map[channel] = output;
    groups = std::max(groups, output);
  }

  bool startRunning() {
    if (state == State::CONFIGURING) {
      state = State::RUNNING;
      return true;
    }
    return false;
  }

  bool stopRunning() {
    if (state != State::RUNNING) {
      return false;
    }
    state = State::CONFIGURING;
    return true;
  }

  bool setInput(size_t idx, Sum value) noexcept {
    if (idx < channels) {
      input[idx] = value;
      return true;
    }
    return false;
  }

  bool setInputs(Sum *inputValues, size_t &length) noexcept {
    if (!inputValues || length < channels) {
      return false;
    }
    for (size_t channel = 0; channel < channels; channel++) {
      input[channel] = inputValues[channel];
    }
    length = channels;
    return true;
  }


  bool getGroupOutputs(Scale *outputValues, size_t &length) noexcept {
    if (!outputValues || length < groups) {
      return false;
    }
    for (size_t group = 0; group < groups; group++) {
      outputValues[group] = output[group];
    }
    length = groups;
    return true;
  }

  bool calculateSums() noexcept {
    if (state != State::RUNNING) {
      return false;
    }
    Sum *in = assume_aligned<alignBytes, Sum *>(input);
    Sum *sm = sum;
    for (size_t time = 0; time < winSizes; time++, sm += alignedChannels) {
      Sum *localSum = assume_aligned<alignBytes, Sum *>(sm);
      Sum *r = assume_aligned<alignBytes, Sum *>(read[time]);
      for (size_t channel = 0; channel < channels; channel++) {
        localSum[channel] += in[channel];
        localSum[channel] -= r[channel];
      }
      next(read[time]);
    }
    Sum *wr = assume_aligned<alignBytes, Sum *>(write);
    for (size_t channel = 0; channel < channels; channel++) {
      wr[channel] = in[channel];
    }
    next(write);
    return true;
  }

  bool calculateOutputs() noexcept {
    if (state != State::RUNNING) {
      return false;
    }
    Scale *out = assume_aligned<sizeof(Scale)*ALIGN, Scale*>(output);
    Scale *timeSum = assume_aligned<sizeof(Scale)*ALIGN, Scale*>(output + alignedChannels);
    Sum *sm = sum;
    for (size_t group = 0; group < groups; group++) {
      out[group] = 0;
    }
    for (size_t time = 0; time < winSizes; time++, sm += alignedChannels) {
      Sum *localSum = assume_aligned<alignBytes, Sum *>(sm);
      Sum factor = scaleFactor[time];
      for (size_t group = 0; group < groups; group++) {
        timeSum[group] = 0;
      }
      for (size_t channel = 0; channel < channels; channel++) {
        timeSum[map[channel]] += factor * localSum[channel];
      }
      for (size_t group = 0; group < groups; group++) {
        out[group] = std::max(out[group], timeSum[group]);
      }
    }
    return true;
  }

  bool getMaxOfPerWindowSizeSum(Scale &result, size_t *channelIdx,
                                size_t length, Scale startValue = 0) {
    if (!channelIdx || length > channels) {
      return false;
    }
    // do trick with kind of channel groups
    for (size_t i = 0; i < length; i++) {
      if (channelIdx[i] >= channels) {
        return false;
      }
    }
    Sum *sm = sum;
    Scale max = startValue;
    for (size_t time = 0; time < winSizes; time++, sm += alignedChannels) {
      Sum *localSum = assume_aligned<alignBytes, Sum *>(sm);
      Sum *r = assume_aligned<alignBytes, Sum *>(read[time]);
      Scale channelsSum = 0;
      for (size_t channel = 0; channel < channels; channel++) {
        channelsSum += scaleFactor[time] * sm[channel];
      }
      next(read[time]);
      max = std::max(max, channelsSum);
    }
    return max;
  }

  Scale getAverage(size_t channel, size_t timeIdx) {
    if (timeIdx >= winSizes || channel >= channels) {
      throw std::out_of_range(
          "MultiAverage: channel or time window out of bounds.");
    }
    return scaleFactor[timeIdx] * sum[alignedChannels * timeIdx + channel];
  }

  bool getAverages(size_t channel, Scale *averages, size_t length) noexcept {
    if (!averages || length < winSizes || channel >= channels) {
      return false;
    }
    Sum *sm = sum + channel;
    for (size_t i = 0; i < winSizes; i++, sm += alignedChannels) {
      averages[i] = scaleFactor[i] * *sum;
    }
    return true;
  }

  Scale getChannelMax(size_t channel, Scale startAtValue = 0) noexcept {
    Scale max = startAtValue;
    Sum *sm = sum + (channel & channelMask);
    for (size_t i = 0; i < winSizes; i++, sm += alignedChannels) {
      max = std::max(max, scaleFactor[i] * *sm);
    }
    return max;
  }

  Scale getChannelValue(size_t idx, Scale (*fn)(Scale, Scale),
                        Scale startAtValue = 0) noexcept {
    Scale result = startAtValue;
    Sum *sm = sum + (idx & channelMask);
    for (size_t i = 0; i < winSizes; i++, sm += alignedChannels) {
      result = fn(result, scaleFactor[i] * *sm);
    }
    return result;
  }
};

} // namespace tdap

#endif // SPEAKERMAN_TRUERMS_HPP
