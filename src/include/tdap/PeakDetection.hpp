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

#ifdef PEAK_DETECTION_LOGGING
    #include <cstdio>
#endif

#include <cmath>
#include <exception>
#include <tdap/Count.hpp>
#include <tdap/Value.hpp>
#include <tdap/Integration.hpp>

#define TRUE_RMS_QUOTE_1(x)#x
#define TRUE_RMS_QUOTE(t)TRUE_RMS_QUOTE_1(t)

namespace tdap {
    using namespace std;

    template <typename S>
    class PeakMemory
    {
        static constexpr size_t windowSizeForSamples(size_t samples)
        {
            return round(pow(samples * 2, 1.0/3));
        }

        static constexpr size_t windowCountForWindowSizeAndSamples(
                size_t windowSize, size_t samples)
        {
            return 1 + (samples - 1) / windowSize;
        }

        static constexpr size_t windowCountForSamples(size_t samples)
        {
            return windowCountForWindowSizeAndSamples(
                    windowSizeForSamples(samples), samples);
        }

        static constexpr size_t allocatedWindowsForWindowSize(
                size_t windowSize, size_t samples)
        {
            return 1 + windowCountForSamples(samples);
        }

        class Window
        {
            S maximum_;
            size_t startAt_;
            S *data_;

        public:

            /**
             * Called at initialisation or reconfiguration of PeakMemory.
             * @param windowSize The size of the window
             */
            void init(S *data)
            {
                data_ = data;
                markAsMostRecent();
            }

            void markAsMostRecent()
            {
                maximum_ = 0;
                startAt_ = 0;
            }

            void markAsOldest()
            {
                maximum_ = 0;
                startAt_ = 0;
            }

            /**
             * Add a sample to the (newest) window and recaluclate maximum.
             * @param sample The new sample
             */
            S addToMostRecentGetMaximum(S sample)
            {
                data_[startAt_++] = sample;
                maximum_ = Floats::max(maximum_, sample);
                return maximum_;
            }

            S removeFromOldestGetMaximum(size_t windowSize)
            {
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

        class Data
        {
            // constraints and allocation
            size_t maxSamples_ = 0;
            size_t maxWindows_ = 0;
            S * data_;
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

            inline size_t relativePtr(size_t ptr, size_t delta)
            {
                return (ptr + delta) % windowCount_;
            }

            inline constexpr size_t nextWindowPtr(size_t ptr)
            {
                return relativePtr(ptr, 1);
            }

            void initNewSamples()
            {
                window_[recentWindowPtr].markAsMostRecent();
                window_[oldestWindowPtr].markAsOldest();
                S max = 0;
                for (
                        size_t ptr = relativePtr(oldestWindowPtr, 1);
                        ptr != recentWindowPtr;
                        ptr = nextWindowPtr(ptr)) {
                    max = Floats::max(max, window_[ptr].maximum());
                }
                betweenWindowMaximum = max;
            }


        public:
            Data(size_t validSamples)
            {
                size_t initialWindowSize = windowSizeForSamples(validSamples);
                size_t initialSamples = initialWindowSize * (validSamples / initialWindowSize);
                if (initialSamples > initialWindowSize) {
                    initialSamples -= initialWindowSize;
                }
                size_t maxSamples = 0;
                size_t maxWindows = 0;
                for (size_t samples = initialSamples ; samples <= validSamples; samples++) {
                    size_t windowSize = windowSizeForSamples(validSamples);
                    size_t windowCount = allocatedWindowsForWindowSize(
                            windowSize, samples);
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

            Window &operator[](size_t index)
            {
                return window_[IndexPolicy::array(index, maxWindows_)];
            }

            size_t setSamples(size_t sampleCount)
            {
                if (sampleCount > maxSamples_) {
                    throw invalid_argument(
                            "PeakMemory::Data: number of samples exceeds maximum");
                }
                windowSize_ = windowSizeForSamples(sampleCount);
                windowCount_ = 1 + windowCountForWindowSizeAndSamples(windowSize_, sampleCount);
                const size_t sampleDataSize = windowSize_ * windowCount_;
                sampleCount_ = sampleDataSize - windowSize_;
                for (size_t sample = 0; sample < sampleDataSize; sample++) {
                    data_[sample] = 0;
                }
                for (size_t window = 0, offs = 0; window < windowCount_; window++, offs += windowSize_) {
                    window_[window].init(data_ + offs);
                }
                betweenWindowMaximum = 0;
                inWindowPtr = 0;
                recentWindowPtr = 0;
                oldestWindowPtr = nextWindowPtr(recentWindowPtr);
                return sampleCount_;
            }

            void next()
            {
                recentWindowPtr = nextWindowPtr(recentWindowPtr);
                oldestWindowPtr = nextWindowPtr(oldestWindowPtr);
                inWindowPtr = 0;
                initNewSamples();
            }

            S addSampleGetPeak(S sample)
            {
                S recent = window_[recentWindowPtr].addToMostRecentGetMaximum(sample);
                S old = window_[oldestWindowPtr].removeFromOldestGetMaximum(windowSize_);
                S peak = Floats::max(Floats::max(recent, old), betweenWindowMaximum);

#if PEAK_DETECTION_LOGGING > 3
                if (is_floating_point<S>::value) {
                    printf("ADD: [%4zu] %8lf IN  %8lf PEAK  %8lf RECENT %8lf RECENT %8lf OLD \n",
                           inWindowPtr,
                           (double)sample, (double)peak, (double)recent, (double)betweenWindowMaximum, (double)old);
                }
                else {
                    printf("ADD: [%4zu] %5zi IN  %5zi PEAK  %5zi RECENT  %5zi BETWEEN  %5zi OLD\n",
                           inWindowPtr,
                           (ssize_t)sample, (ssize_t)peak, (ssize_t)recent, (ssize_t)betweenWindowMaximum, (ssize_t)old);
                }
#endif
                inWindowPtr++;
                if (inWindowPtr == windowSize_) {
                    next();
                }
                return peak;
            }

            ~Data()
            {
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

        static size_t validSamples(size_t samples)
        {
            if (samples > 5 && samples < Count<S>::max() / 2) {
                return samples;
            }
            throw invalid_argument("PeakMemory: maximum number of samples must be larger than 5 and fit in memory");
        }

        Data data_;

    public:
        PeakMemory(size_t maxSamples)
        :
        data_(validSamples(maxSamples))
        {
            setSamples(maxSamples);
        }

        size_t samples() const { return data_.samples(); }

        size_t setSamples(size_t samples)
        {
            return data_.setSamples(samples);
        }

        S addSampleGetPeak(S sample)
        {
            return data_.addSampleGetPeak(sample);
        }

    };

    template <typename S>
    class PeakDetector
    {
        PeakMemory<S> memory;
        S relativeAttackTimeConstant = 1.0;
        S relativeSmoothingTimeConstant = 1.0;
        S relativeReleaseTimeConstant = 1.0;
        IntegrationCoefficients<S> attackFollower;
        IntegrationCoefficients<S> releaseFollower;
        Integrator<S> smoothingFollower;
        S rawDetection = 1.0;
        S compensationFactor = 1.0;
        S threshold = 1.0;
    public:
        PeakDetector(size_t maxSamples, S relativeAttackTimeConstant, S relativeSmoothingTimeConstant, S relativeReleaseTimeConstant)
        :
        memory(maxSamples),
        relativeAttackTimeConstant(relativeAttackTimeConstant),
        relativeSmoothingTimeConstant(relativeSmoothingTimeConstant),
        relativeReleaseTimeConstant(relativeReleaseTimeConstant)
        {
            setSamplesAndThreshold(maxSamples, 1.0);
        }

        PeakDetector(size_t maxSamples) : PeakDetector(maxSamples, 1.0, 1.0, 1.0) {}

        size_t setSamplesAndThreshold(size_t samples, S peakThreshold)
        {
            size_t effectiveSamples = memory.setSamples(samples);
            size_t attackSamples = effectiveSamples * relativeAttackTimeConstant;
            size_t smoothSamples = effectiveSamples * relativeSmoothingTimeConstant;
            size_t releaseSamples = effectiveSamples * relativeReleaseTimeConstant;

            attackFollower.setCharacteristicSamples(attackSamples);
            smoothingFollower.coefficients_.setCharacteristicSamples(smoothSamples);
            releaseFollower.setCharacteristicSamples(releaseSamples);

            compensationFactor = 1;
            double newCompensationFactor = 10.0;
            size_t iterations = 0;
            while (newCompensationFactor > 1.0) {
                smoothingFollower.set_output(0.0);
                S output = 0;
                for (size_t i = 1; i < effectiveSamples; i++) {
                    output += attackFollower.inputMultiplier() * (compensationFactor - output);
                    smoothingFollower.integrate(output);
                }
                newCompensationFactor = 1.0 / smoothingFollower.output_;
                compensationFactor *= newCompensationFactor;
                iterations++;
            }
            compensationFactor *= exp(M_SQRT2 * smoothSamples / attackSamples);
#if PEAK_DETECTION_LOGGING > 0
            smoothingFollower.set_output(0.0);
            S out = 0;
            for (size_t i = 1; i < effectiveSamples; i++) {
                out += attackFollower.inputMultiplier() * (compensationFactor - out);
                smoothingFollower.integrate(out);
            }
            printf("setSamplesAndThreshold(%zu -> %zu): %lf compensation   %zu iterations  %lf attack   %lf smooth\n",
                    samples, effectiveSamples, compensationFactor, iterations, attackFollower.getCharacteristicSamples(), smoothingFollower.coefficients_.getCharacteristicSamples());
#endif
            threshold = peakThreshold;
            smoothingFollower.set_output(peakThreshold);
            rawDetection = peakThreshold;
            return effectiveSamples;
        }

        S addSampleGetDetection(S sample)
        {
            S peak = memory.addSampleGetPeak(Floats::max(threshold, sample));
            if (peak >= rawDetection) {
                rawDetection += attackFollower.inputMultiplier() * (compensationFactor * peak - rawDetection);
            }
            else {
                rawDetection += releaseFollower.inputMultiplier() * (peak - rawDetection);
            }
            return smoothingFollower.integrate(rawDetection);
        }
    };
    using PeakMemoryDoubles = PeakMemory<double>;

} /* End of name space tdap */

#endif /* PEAK_DETECTION_HEADER_GUARD */
