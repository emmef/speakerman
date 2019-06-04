/*
 * tdap/TrueRms.hpp
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

#include <type_traits>
#include <cmath>

#include <tdap/Array.hpp>
#include <tdap/Followers.hpp>
#include <tdap/Power2.hpp>

#define TRUE_RMS_QUOTE_1(x)#x
#define TRUE_RMS_QUOTE(t)TRUE_RMS_QUOTE_1(t)

namespace tdap {
    using namespace std;

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
     * @tparam MAX_SAMPLE_HISTORY the maximum sample history, determining the maximum RMS window size
     * @tparam MAX_RCS the maximum number of characteristic times in this array
     */
    template<typename S>
    class TrueFloatingPointMovingAverage
    {
        static_assert(std::is_floating_point<S>::value, "Sample type must be floating point");
        static constexpr double MINIMUM_RELATIVE_ERROR_NOISE_LEVEL = 1e-20;
        static constexpr double MAXIMUM_RELATIVE_ERROR_NOISE_LEVEL = 1e-2;
        static constexpr const char * ERROR_NOISE_LEVEL_MESSAGE =
                "The error noise level must lie between "
                TRUE_RMS_QUOTE(MINIMUM_RELATIVE_ERROR_NOISE_LEVEL)
                " and "
                TRUE_RMS_QUOTE(MAXIMUM_RELATIVE_ERROR_NOISE_LEVEL)
                ".";

        static constexpr size_t MINIMUM_MAXIMUM_WINDOW_SAMPLES = 64;

        static constexpr const char * MAXIMUM_WINDOW_SAMPLES_MESSAGE =
                "Maximum number/window of RMS samples is not "
                TRUE_RMS_QUOTE(MINIMUM_MAXIMUM_WINDOW_SAMPLES)
                " or larger or is too big, resulting in a bigger measurement"
                " noise threshold than the required (provided) one";
        static constexpr const char * ERROR_TIME_CONSTANT_MESSAGE =
                "The decay time-constant (in samples) for error mitigation must be larger than the maximum number/window of RMS samples";

        static constexpr size_t MINIMUM_TIME_CONSTANTS = 1;
        static constexpr size_t MAXIMUM_TIME_CONSTANTS = 32;
        static constexpr const char * TIME_CONSTANT_MESSAGE =
                "The (maximum) number of time-constants must lie between "
                TRUE_RMS_QUOTE(MINIMUM_TIME_CONSTANTS) " and "
                TRUE_RMS_QUOTE(MAXIMUM_TIME_CONSTANTS) ".";

        struct WindowEntry
        {
            size_t windowSize_ = 1;
            S scale_ = 1;
            S inputFactor_ = 1;
            S historyDecayFactor_ = 1;
            size_t readPtr_ = 1;
            S average_ = 0;

            void setAverage(S average)
            {
                average_ = average * (1.0 - inputFactor_ + historyDecayFactor_);
            }

            S getAverage() const
            {
                return average_ * scale_;
            }
        };

        Array<S> history_;
        Array<WindowEntry> window_;
        size_t usedWindows_;
        size_t maxErrorMitigatingTimeContant_;
        S averageDecayFactor_;
        size_t writePointer_;
        size_t maxPtrValue_;

        static double validRelativeErrorNoise(double relativeError) {
            if (Values::is_between(relativeError, MINIMUM_RELATIVE_ERROR_NOISE_LEVEL, MAXIMUM_RELATIVE_ERROR_NOISE_LEVEL)) {
                return relativeError;
            }
            throw std::invalid_argument(ERROR_NOISE_LEVEL_MESSAGE);
        }

        static size_t validMaxWindowSamples(size_t maxWindowSamples, double relativeErrorNoise)
        {
            size_t maxOperations = Value<double>::min(
                    validRelativeErrorNoise(relativeErrorNoise) / std::numeric_limits<S>::epsilon(),
                    Count<S>::max());

            if (Values::is_between(maxWindowSamples, MINIMUM_MAXIMUM_WINDOW_SAMPLES, maxOperations)) {
                return maxWindowSamples;
            }
            throw std::invalid_argument(MAXIMUM_WINDOW_SAMPLES_MESSAGE);
        }

        static size_t validMaxTimeConstants(size_t maxTimeConstants)
        {
            if (!Values::is_between(maxTimeConstants, MINIMUM_TIME_CONSTANTS, MAXIMUM_TIME_CONSTANTS)) {
                throw std::invalid_argument(TIME_CONSTANT_MESSAGE);
            }
            return maxTimeConstants;
        }

        static size_t validErrorMitigatingTimeConstant(size_t errorMitigatingTimeConstant, size_t maxWindowSamples)
        {
            size_t hardMaximum = Value<double>::min(0.01 / numeric_limits<S>::epsilon(), numeric_limits<size_t>::max());
            if (errorMitigatingTimeConstant < maxWindowSamples * 10) {
                throw std::invalid_argument(
                        "Error mitigating decay time constant must be larger than ten times the maximum RMS window");
            }
            if (errorMitigatingTimeConstant > hardMaximum) {
                throw std::invalid_argument("Error mitigating decay time constant is too large to be properly represented, given the sample type precision");
            }
            return errorMitigatingTimeConstant;
        }

        void nextPtr(size_t &ptr)
        {
            if (ptr > 0) {
                ptr--;
            }
            else {
                ptr = maxPtrValue_;
            }
        }

        void setWindowEntrySizeAndScale(WindowEntry &window, size_t windowSamples, S scale) const
        {
            window.windowSize_ = windowSamples;
            const double unscaledHistoryDecayFactor =
                    Integration::get_history_multiplier(
                            1.0 * maxErrorMitigatingTimeContant_ / windowSamples);

            window.scale_ = scale;
            window.inputFactor_ = (1.0 - averageDecayFactor_) *
                                  (1.0 / (1.0 - unscaledHistoryDecayFactor));
            window.historyDecayFactor_ =
                    unscaledHistoryDecayFactor * window.inputFactor_;
            printf("Window: samples=%-8zu ; scale=%10lg; history-decay=%20.18lf ; input-multiply=%20.18lf ; history-factor=%20.18lf\n",
                    windowSamples, window.scale_, unscaledHistoryDecayFactor, window.inputFactor_, window.historyDecayFactor_);
            window.readPtr_ =
                    (writePointer_ + windowSamples) % history_.capacity();
            window.setAverage(window.average_);
        }

        void addInput(WindowEntry &window, S input)
        {
            double history = history_[window.readPtr_];
            nextPtr(window.readPtr_);
            window.average_ =
                    averageDecayFactor_ * window.average_ +
                    window.inputFactor_ * input -
                    window.historyDecayFactor_ * history;
        }

        void checkWindowIndex(size_t index) const
        {
            if (index > getUsedWindows()) {
                throw std::out_of_range("Window index greater than configured windows to use");
            }
        }

    public:
        TrueFloatingPointMovingAverage(
                size_t maxWindowSamples, size_t errorMitigatingTimeConstant, size_t maxTimeConstants, double relativeErrorNoiseLevel)
                :
                history_(validMaxWindowSamples(maxWindowSamples,relativeErrorNoiseLevel)),
                window_(validMaxTimeConstants(maxTimeConstants)),
                maxErrorMitigatingTimeContant_(validErrorMitigatingTimeConstant(errorMitigatingTimeConstant, history_.capacity())),
                maxPtrValue_(history_.capacity() - 1),
                usedWindows_(window_.capacity()),
                writePointer_(0)
        {
            averageDecayFactor_ = Integration::get_history_multiplier(1.0 * maxErrorMitigatingTimeContant_);
            printf("Average-decay (%zu samples): %20.18lf\n", maxErrorMitigatingTimeContant_, averageDecayFactor_);
        }

        size_t getMaxWindows() const { return window_.capacity(); }
        size_t getUsedWindows() const { return usedWindows_; }
        size_t getMaxWindowSamples() const { return history_.capacity(); }

        void setUsedWindows(size_t windows)
        {
            if (windows > 0 && windows <= window_.capacity()) {
                usedWindows_ = windows;
                for (size_t i = 0; i < usedWindows_; i++) {
                    WindowEntry &window = window_[i];
                    window.readPtr_ =
                            (writePointer_ + window.windowSize_) % history_.capacity();
                }
            }
            else {
                throw std::out_of_range(
                        "Number of used windows zero or larger than condigured maximum at construction");
            }
        }

        void setWindowSizeAndScale(size_t index, size_t windowSamples, S scale)
        {
            checkWindowIndex(index);
            if (windowSamples > history_.capacity()) {
                throw std::out_of_range("Window size in samples is larger than configured maximum at construction.");
            }
            setWindowEntrySizeAndScale(window_[index], windowSamples, scale);
        }

        void setAverage(S average)
        {
            for (size_t i = 0; i < window_.capacity(); i++) {
                window_[i].setAverage(average);
            }
            for (size_t i = 0; i < history_.capacity(); i++) {
                history_[i] = average;
            }
        }

        S getAverage(size_t index) const
        {
            checkWindowIndex(index);
            return window_.unsafeData()[index].getAverage();
        }

        size_t getWindowSize(size_t index) const
        {
            checkWindowIndex(index);
            return window_[index].windowSize_;
        }

        S getWindowScale(size_t index) const
        {
            checkWindowIndex(index);
            return window_[index].scale_;
        }

        void addInput(S input) {
            for (size_t i = 0; i < getUsedWindows(); i++) {
                addInput(window_[i], input);
            }
            history_[writePointer_] = input;
            nextPtr(writePointer_);
        }

        S addInputGetMax(S input, S minimumValue)
        {
            S average = minimumValue;
            for (size_t i = 0; i < getUsedWindows(); i++) {
                addInput(window_[i], input);
                average = Values::max(window_[i].getAverage(), average);
            }
            history_[writePointer_] = input;
            nextPtr(writePointer_);
            return average;
        }

        S referenceGetRms(S minimumValue)
        {
            S average = minimumValue;
            for (size_t i = 0; i < getUsedWindows(); i++) {
                WindowEntry &window = window_[i];
                S sum = 0.0;
                for (size_t j = 0; j < window.windowSize_ - 1; j++) {
                    sum += history_[(window.readPtr + j) % history_.capacity()];
                }
                sum /= window.windowSize_;
                average = Values::max(average, sum);
            }
            return average;
        }

        void reset(bool resetHistory, double averageValue, bool resetWindows)
        {
            writePointer_ = 0;
            for (size_t i = 0; i < history_.capacity(); i++) {
                history_[i] = averageValue;
            }
            if (resetWindows) {
                for (size_t i = 0; i < window_; i++) {
                    window_.reset(averageValue);
                }
            }
        }
    };

    template<typename S, size_t MAX_SAMPLES, size_t LEVELS>
    using TrueMultiRcRms = TrueFloatingPointMovingAverage<S>;

} /* End of name space tdap */

#endif /* TDAP_TRUE_RMS_HEADER_GUARD */
