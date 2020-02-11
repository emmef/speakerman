/*
 * tdap/PeakDetection.hpp
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

#ifndef PEAK_DETECTION_HEADER_GUARD
#define PEAK_DETECTION_HEADER_GUARD

#if defined(PEAK_DETECTION_LOGGING) ||                                         \
    defined(PEAK_DETECTION_CHEAP_MEMORY_LOGGING) ||                            \
    defined(PEAK_DETECTION_CHEAP_METRICS_LOGGING)
#include <cstdio>
#endif

#include <cmath>
#include <exception>
#include <limits>
#include <tdap/Count.hpp>
#include <tdap/Followers.hpp>
#include <tdap/Integration.hpp>
#include <tdap/Value.hpp>
#include <type_traits>

#define TRUE_RMS_QUOTE_1(x) #x
#define TRUE_RMS_QUOTE(t) TRUE_RMS_QUOTE_1(t)

namespace tdap {
using namespace std;

template <typename S> class PeakMemory {
  static constexpr size_t windowSizeForSamples(size_t samples) {
    return round(pow(samples * 2, 1.0 / 3));
  }

  static constexpr size_t windowCountForWindowSizeAndSamples(size_t windowSize,
                                                             size_t samples) {
    return 1 + (samples - 1) / windowSize;
  }

  static constexpr size_t windowCountForSamples(size_t samples) {
    return windowCountForWindowSizeAndSamples(windowSizeForSamples(samples),
                                              samples);
  }

  static constexpr size_t allocatedWindowsForWindowSize(size_t windowSize,
                                                        size_t samples) {
    return 1 + windowCountForSamples(samples);
  }

  class Window {
    S maximum_;
    size_t startAt_;
    S *data_;

  public:
    /**
     * Called at initialisation or reconfiguration of PeakMemory.
     * @param windowSize The size of the window
     */
    void init(S *data) {
      data_ = data;
      markAsMostRecent();
    }

    void markAsMostRecent() {
      maximum_ = 0;
      startAt_ = 0;
    }

    void markAsOldest() {
      maximum_ = 0;
      startAt_ = 0;
    }

    /**
     * Add a sample to the (newest) window and recaluclate maximum.
     * @param sample The new sample
     */
    S addToMostRecentGetMaximum(S sample) {
      data_[startAt_++] = sample;
      maximum_ = Floats::max(maximum_, sample);
      return maximum_;
    }

    S removeFromOldestGetMaximum(size_t windowSize) {
      S mx = 0;
      for (size_t i = startAt_; i < windowSize; i++) {
        mx = max(mx, data_[i]);
      }
      startAt_++;
      maximum_ = mx;
      return maximum_;
    }

    S maximum() const { return maximum_; }
  };

  class Data {
    // constraints and allocation
    size_t maxSamples_ = 0;
    size_t maxWindows_ = 0;
    S *data_;
    Window *window_;

    // configuration
    size_t sampleCount_ = 0;
    size_t windowCount_ = 0;
    size_t windowSize_ = 0;

    // runtime-data
    size_t recentWindowPtr = 0;
    size_t oldestWindowPtr = 1;
    S betweenWindowMaximum = 0;
    size_t inWindowPtr = 0;

    inline size_t relativePtr(size_t ptr, size_t delta) {
      return (ptr + delta) % windowCount_;
    }

    inline constexpr size_t nextWindowPtr(size_t ptr) {
      return relativePtr(ptr, 1);
    }

    void initNewSamples() {
      window_[recentWindowPtr].markAsMostRecent();
      window_[oldestWindowPtr].markAsOldest();
      S max = 0;
      for (size_t ptr = relativePtr(oldestWindowPtr, 1); ptr != recentWindowPtr;
           ptr = nextWindowPtr(ptr)) {
        max = Floats::max(max, window_[ptr].maximum());
      }
      betweenWindowMaximum = max;
    }

  public:
    Data(size_t validSamples) {
      size_t initialWindowSize = windowSizeForSamples(validSamples);
      size_t initialSamples =
          initialWindowSize * (validSamples / initialWindowSize);
      if (initialSamples > initialWindowSize) {
        initialSamples -= initialWindowSize;
      }
      size_t maxSamples = 0;
      size_t maxWindows = 0;
      for (size_t samples = initialSamples; samples <= validSamples;
           samples++) {
        size_t windowSize = windowSizeForSamples(validSamples);
        size_t windowCount = allocatedWindowsForWindowSize(windowSize, samples);
        maxSamples = Sizes::max(maxSamples, windowCount * windowSize);
        maxWindows = Sizes::max(maxWindows, windowCount);
      }
      data_ = new S[maxSamples];
      window_ = new Window[maxWindows];
      maxSamples_ = maxSamples;
      maxWindows_ = maxWindows;

      setSamples(validSamples);
    }

    size_t samples() const { return sampleCount_; }

    Window &operator[](size_t index) {
      return window_[IndexPolicy::array(index, maxWindows_)];
    }

    size_t setSamples(size_t sampleCount) {
      if (sampleCount > maxSamples_) {
        throw invalid_argument(
            "PeakMemory::Data: number of samples exceeds maximum");
      }
      windowSize_ = windowSizeForSamples(sampleCount);
      windowCount_ =
          1 + windowCountForWindowSizeAndSamples(windowSize_, sampleCount);
      const size_t sampleDataSize = windowSize_ * windowCount_;
      sampleCount_ = sampleDataSize - windowSize_;
      for (size_t sample = 0; sample < sampleDataSize; sample++) {
        data_[sample] = 0;
      }
      for (size_t window = 0, offs = 0; window < windowCount_;
           window++, offs += windowSize_) {
        window_[window].init(data_ + offs);
      }
      betweenWindowMaximum = 0;
      inWindowPtr = 0;
      recentWindowPtr = 0;
      oldestWindowPtr = nextWindowPtr(recentWindowPtr);
      return sampleCount_;
    }

    void next() {
      recentWindowPtr = nextWindowPtr(recentWindowPtr);
      oldestWindowPtr = nextWindowPtr(oldestWindowPtr);
      inWindowPtr = 0;
      initNewSamples();
    }

    S addSampleGetPeak(S sample) {
      S recent = window_[recentWindowPtr].addToMostRecentGetMaximum(sample);
      S old = window_[oldestWindowPtr].removeFromOldestGetMaximum(windowSize_);
      S peak = Floats::max(Floats::max(recent, old), betweenWindowMaximum);

      //#if PEAK_DETECTION_LOGGING > 3
      //                if (is_floating_point<S>::value) {
      //                    printf("ADD: [%4zu] %8lf IN  %8lf PEAK  %8lf RECENT
      //                    %8lf RECENT %8lf OLD \n",
      //                           inWindowPtr,
      //                           (double)sample, (double)peak, (double)recent,
      //                           (double)betweenWindowMaximum, (double)old);
      //                }
      //                else {
      //                    printf("ADD: [%4zu] %5zi IN  %5zi PEAK  %5zi RECENT
      //                    %5zi BETWEEN  %5zi OLD\n",
      //                           inWindowPtr,
      //                           (ssize_t)sample, (ssize_t)peak,
      //                           (ssize_t)recent,
      //                           (ssize_t)betweenWindowMaximum, (ssize_t)old);
      //                }
      //#endif
      inWindowPtr++;
      if (inWindowPtr == windowSize_) {
        next();
      }
      return peak;
    }

    ~Data() {
      if (window_ != nullptr) {
        delete[] window_;
        window_ = nullptr;
      }
      if (data_ != nullptr) {
        delete[] data_;
        data_ = nullptr;
      }
      maxSamples_ = 0;
      maxWindows_ = 0;
    }
  };

  static size_t validSamples(size_t samples) {
    if (samples > 5 && samples < Count<S>::max() / 2) {
      return samples;
    }
    throw invalid_argument("PeakMemory: maximum number of samples must be "
                           "larger than 5 and fit in memory");
  }

  Data data_;

public:
  PeakMemory(size_t maxSamples) : data_(validSamples(maxSamples)) {
    setSampleCount(maxSamples);
  }

  size_t samples() const { return data_.samples(); }

  size_t setSampleCount(size_t samples) { return data_.setSamples(samples); }

  void resetState() { setSampleCount(samples()); }

  S addSampleGetPeak(S sample) { return data_.addSampleGetPeak(sample); }
};

template <typename S> class CheapPeakMemoryMetrics {
  size_t requestedSamples_;
  size_t windowSize_;
  size_t windowCount_;
  size_t maxWindowCount_;
  size_t sampleCount_;

  static size_t validCount(size_t samples) {
    if (samples == 0) {
      throw std::invalid_argument(
          "CheapPeakMemoryMetrics::windowSizeForSamples: must have positive "
          "number of samples");
    }
    if (samples > Count<S>::max()) {
      throw std::invalid_argument(
          "CheapPeakMemoryMetrics::windowSizeForSamples: number of samples "
          "exceeds maximum for sample type");
    }
    return samples;
  }

  static size_t unsafeWindowSizeForSamples(size_t samples) {
    return Sizes::max(1, sqrt(samples));
  }

  static size_t unsafeWindowCountForSamplesAndSize(size_t samples,
                                                   size_t windowSize) {
    size_t count = samples / windowSize;
    size_t actualSamples = count * windowSize;
    if (actualSamples == samples) {
      return count;
    }
    count++;
    if (Count<S>::max() / count < windowSize) {
      throw std::invalid_argument(
          "CheapPeakMemoryMetrics::windowsForSamplesAndSize: actual number of "
          "samples would exceed maximum for sample type");
    }
    return count;
  }

public:
  static size_t windowSizeForSamples(size_t samples) {
    return unsafeWindowSizeForSamples(validCount(samples));
  }

  static size_t windowCountForSamplesAndSize(size_t samples,
                                             size_t windowSize) {
    return unsafeWindowCountForSamplesAndSize(validCount(samples),
                                              validCount(windowSize));
  }

  CheapPeakMemoryMetrics(size_t sampleCount) { setSampleCount(sampleCount); }

  CheapPeakMemoryMetrics() : CheapPeakMemoryMetrics(1) {}

  inline size_t requestedSamples() const { return requestedSamples_; }
  inline size_t sampleCount() const { return sampleCount_; }
  inline size_t windowSize() const { return windowSize_; }
  inline size_t windowCount() const { return windowCount_; }
  inline size_t maxWindowCount() const { return maxWindowCount_; }

  size_t setSampleCount(size_t sampleCount) {
    requestedSamples_ = validCount(sampleCount);
    windowSize_ = unsafeWindowSizeForSamples(requestedSamples_);
    windowCount_ =
        unsafeWindowCountForSamplesAndSize(requestedSamples_, windowSize_);
    maxWindowCount_ = windowCount_;
    if (windowCount_ > 1) {
      for (size_t i = 0, size = windowSize_ * (windowCount_ - 1);
           i < windowSize_; i++, size++) {
#if PEAK_DETECTION_CHEAP_METRICS_LOGGING > 3
        printf("\tsetSampleCount([%zu] size=%zu, maxWindowCount=%zu)\n", i,
               size, maxWindowCount_);
#endif
        maxWindowCount_ = Sizes::max(
            maxWindowCount_, unsafeWindowCountForSamplesAndSize(
                                 size, unsafeWindowSizeForSamples(size)));
      }
    }
    sampleCount_ = windowSize_ * windowCount_;
#if PEAK_DETECTION_CHEAP_METRICS_LOGGING > 0
    printf("CheapPeakMemoryMetrics("
           "requestedSamples=%zu, sampleCount=%zu, "
           "windowSize=%zu, windowCount=%zu, maxWindowCount=%zu)\n",
           requestedSamples_, sampleCount_, windowSize_, windowCount_,
           maxWindowCount_);
#endif
    return sampleCount_;
  }
};

template <typename S> class CheapPeakMemory {
  static constexpr const S MINIMUM_VALUE = numeric_limits<S>::lowest();

  const CheapPeakMemoryMetrics<S> maxMetrics_;
  CheapPeakMemoryMetrics<S> metrics_;
  S *windowPeak_;
  size_t recentWindowPtr_;
  size_t samplePtr_;
  S oldWindowsPeak_;
  S recentPeak_;

  size_t wrap(size_t ptr) { return ptr % metrics_.windowSize(); }
  size_t next(size_t ptr) { return (ptr + 1) % metrics_.windowSize(); }

  S calculateOldWindowsMax() {
    S oldMax = MINIMUM_VALUE;
    for (size_t window = 0; window < metrics_.windowCount(); window++) {
      oldMax = Value<S>::max(oldMax, windowPeak_[window]);
    }
    return oldMax;
  }

public:
  CheapPeakMemory(size_t maxSampleCount)
      : maxMetrics_(maxSampleCount), metrics_(maxMetrics_),
        windowPeak_(new S[maxMetrics_.maxWindowCount()]) {
    resetState();
  }

  const CheapPeakMemoryMetrics<S> metrics() const { return metrics_; }

  size_t setSampleCount(size_t sampleCount) {
    if (sampleCount > maxMetrics_.sampleCount()) {
      throw std::invalid_argument(
          "CheapPeakMemory::setSampleCount: number of samples exceeds maximum "
          "set at construction");
    }
    metrics_.setSampleCount(sampleCount);
    resetState();
    return metrics_.sampleCount();
  }

  void resetState() {
    for (size_t window = 0; window < metrics_.windowCount(); window++) {
      windowPeak_[window] = MINIMUM_VALUE;
    }
    recentWindowPtr_ = 0;
    samplePtr_ = 0;
    oldWindowsPeak_ = MINIMUM_VALUE;
    recentPeak_ = MINIMUM_VALUE;
  }

  S addSampleGetPeak(S newSample) {
    recentPeak_ = Value<S>::max(recentPeak_, newSample);
    S peak = Value<S>::max(recentPeak_, oldWindowsPeak_);

#if PEAK_DETECTION_CHEAP_MEMORY_LOGGING > 3
    if (is_floating_point<S>::value) {
      printf("CheapPeakMemory::addSampleGetPeak   %11.04lg IN   %11.04lg PEAK  "
             " %11.04lg RECENT   %11.04lg OLD\n",
             (double)newSample, (double)peak, (double)recentPeak_,
             (double)oldWindowsPeak_);
    } else {
      printf("CheapPeakMemory::addSampleGetPeak   %11ld IN   %11ld PEAK   "
             "%11ld RECENT   %11ld OLD\n",
             (long signed)newSample, (long signed)peak,
             (long signed)recentPeak_, (long signed)oldWindowsPeak_);
    }
#endif

    samplePtr_++;
    if (samplePtr_ == metrics_.windowSize()) {
      windowPeak_[recentWindowPtr_] = recentPeak_;
      oldWindowsPeak_ = calculateOldWindowsMax();
      recentWindowPtr_ = next(recentWindowPtr_);
      samplePtr_ = 0;
      windowPeak_[recentWindowPtr_] = MINIMUM_VALUE;
    }
    return peak;
  }

  ~CheapPeakMemory() { delete[] windowPeak_; }
};

#ifdef PEAK_DETECTION_ADD_GET_DETECTION_LOGGING
#define ADD_GET_DETECTION_LOGGING_(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define ADD_GET_DETECTION_LOGGING_
#endif

template <typename S, class Memory> class CompensatedAttackWithMemory {
  CompensatedAttack<S> follower;
  Memory memory;

public:
  CompensatedAttackWithMemory(size_t maxSampleCount) : memory(maxSampleCount) {}

  void setTimeConstantAndSamples(size_t timeConstantSamples, size_t samples,
                                 S initialValue) {
    size_t actualSamples = memory.setSampleCount(samples);
    follower.setTimeConstantAndSamples(timeConstantSamples, actualSamples,
                                       initialValue);
  }

  S follow(const S peak) {
    return follower.follow(memory.addSampleGetPeak(peak));
  }
};

template <typename S, class Memory> class TriangularFollowerWithMemory {
  TriangularFollower<S> follower;
  Memory memory;

public:
  TriangularFollowerWithMemory(size_t maxSampleCount)
      : follower(1 + maxSampleCount / 10), memory(2) {}

  void setTimeConstantAndSamples(size_t attackSamples, size_t releaseSamples,
                                 S thresHold) {
    //            memory.setSampleCount(attackSamples);
    follower.setTimeConstantAndSamples(attackSamples, releaseSamples,
                                       thresHold);
  }

  S follow(const S sample) { return follower.follow(sample); }
};

template <typename S, class Memory> class PeakDetectorBase {
  TriangularFollowerWithMemory<S, Memory> attackMemory;
  CompensatedAttackWithMemory<S, Memory> smoothMemory;
  S relativeAttackTimeConstant = 1.0;
  S relativeSmoothingTimeConstant = 1.0;
  S relativeReleaseTimeConstant = 1.0;
  S releaseDetection = 1.0;
  S attackCompensation = 1.0;
  S smoothCompensation = 1.0;

  S threshold = 1.0;
#ifdef PEAK_DETECTION_ADD_GET_DETECTION_LOGGING
  size_t index = 0;
#endif

  S validRelativeAttackTimeConstant(S relativeAttackTimeConstant,
                                    S relativeSmoothingTimeConstant) {
    if (!Value<S>::is_between(relativeAttackTimeConstant, 0.1, 0.9)) {
      throw invalid_argument("Attack time constant must be between 10 and 90 "
                             "percent of the number of samples");
    }
    if (!Value<S>::is_between(relativeSmoothingTimeConstant, 0.01,
                              1.0 - relativeAttackTimeConstant)) {
      throw invalid_argument(
          "Smoothing time constant must be larger than 1 percent of number of "
          "samples while sum of attack and smoothing cannot exceed the total "
          "number of samples");
    }
    return relativeAttackTimeConstant;
  }

public:
  PeakDetectorBase(size_t maxSamples, S relativeAttackTimeConstant,
                   S relativeSmoothingTimeConstant,
                   S relativeReleaseTimeConstant)
      : attackMemory(maxSamples), smoothMemory(maxSamples),
        relativeAttackTimeConstant(validRelativeAttackTimeConstant(
            relativeAttackTimeConstant, relativeSmoothingTimeConstant)),
        relativeSmoothingTimeConstant(relativeSmoothingTimeConstant),
        relativeReleaseTimeConstant(relativeReleaseTimeConstant) {
    setSamplesAndThreshold(maxSamples, 1.0);
  }

  PeakDetectorBase(size_t maxSamples)
      : PeakDetectorBase(maxSamples, 1.0, 1.0, 1.0) {}

  size_t setSamplesAndThreshold(size_t samples, S peakThreshold) {
    size_t attackSamples = samples * relativeAttackTimeConstant;
    size_t smoothSamples = samples * relativeSmoothingTimeConstant;
    size_t attackMemorySamples = samples - smoothSamples;
    size_t releaseSamples = samples * relativeReleaseTimeConstant;

    attackMemory.setTimeConstantAndSamples(attackSamples, attackMemorySamples,
                                           peakThreshold);
    smoothMemory.setTimeConstantAndSamples(smoothSamples, smoothSamples + 1,
                                           peakThreshold);

    threshold = peakThreshold;
    return attackSamples + smoothSamples;
  }

  S addSampleGetDetection(S sample) {
    return attackMemory.follow(Floats::max(threshold, sample));
    //            S peak = attackMemory.follow(Floats::max(threshold, sample));
    //
    //            S detection = smoothMemory.follow(peak);
    //
    //            return detection;
  }

  void resetState() { attackMemory.resetState(); }
};

template <typename S> using PeakDetector = PeakDetectorBase<S, PeakMemory<S>>;

template <typename S>
using CheapPeakDetector = PeakDetectorBase<S, CheapPeakMemory<S>>;

} // namespace tdap

#endif /* PEAK_DETECTION_HEADER_GUARD */
