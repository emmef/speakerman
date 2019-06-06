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

    template<
            typename S, size_t SNR_BITS = 20, size_t MIN_ERROR_DECAY_TO_WINDOW_RATIO = 10>
    struct MetricsForTrueFloatingPointMovingAverageMetyrics
    {
        static_assert(
                std::is_floating_point<S>::value,
                "Sample type must be floating point");

        static constexpr size_t MIN_SNR_BITS = 4;
        static constexpr size_t MAX_SNR_BITS = 44;
        static_assert(
                SNR_BITS >= MIN_SNR_BITS && SNR_BITS <= MAX_SNR_BITS,
                "Number of signal-noise-ratio in bits must lie between"
                TRUE_RMS_QUOTE(MIN_SNR_BITS)
                " and "
                TRUE_RMS_QUOTE(MAX_SNR_BITS)
                ".");

        static constexpr size_t MIN_MIN_ERROR_DECAY_TO_WINDOW_RATIO = 1;
        static constexpr size_t MAX_MIN_ERROR_DECAY_TO_WINDOW_RATIO = 1000;
        static_assert(
                Sizes::is_between(
                        MIN_ERROR_DECAY_TO_WINDOW_RATIO,
                        MIN_MIN_ERROR_DECAY_TO_WINDOW_RATIO,
                        MAX_MIN_ERROR_DECAY_TO_WINDOW_RATIO
                ),
                "Minimum error decay to window size ratio must lie between "
                TRUE_RMS_QUOTE(MIN_MIN_ERROR_DECAY_TO_WINDOW_RATIO)
                " and "
                TRUE_RMS_QUOTE(MAX_MIN_ERROR_DECAY_TO_WINDOW_RATIO));


        static constexpr size_t MAX_ERR_MITIGATING_DECAY_SAMPLES =
                Value<double>::min(
                        0.01 / numeric_limits<S>::epsilon(),
                        numeric_limits<size_t>::max());
        static constexpr size_t MAX_WINDOWS_SIZE_BOUNDARY =
                MAX_ERR_MITIGATING_DECAY_SAMPLES /
                MIN_ERROR_DECAY_TO_WINDOW_RATIO;
        static constexpr const char *
                ERR_MITIGATING_DECAY_SAMPLES_EXCEEDED_MESSAGE =
                "The decay time-constant (in samples) for error mitigation must be smaller than "
                TRUE_RMS_QUOTE(MAX_ERR_MITIGATING_DECAY_SAMPLES)
                ".";
        static constexpr size_t MIN_MAX_WINDOW_SAMPLES = 64;
        static constexpr size_t MAX_MAX_WINDOW_SAMPLES = Floats::min(
                pow(0.5, SNR_BITS + 1) / std::numeric_limits<S>::epsilon(),
                MAX_WINDOWS_SIZE_BOUNDARY);
        static constexpr size_t MIN_ERR_MITIGATING_DECAY_SAMPLES =
                MIN_ERROR_DECAY_TO_WINDOW_RATIO * MIN_MAX_WINDOW_SAMPLES;


    public:
        static constexpr const size_t getMinimumWindowSizeInSamples()
        {
            return MIN_MAX_WINDOW_SAMPLES;
        }

        static constexpr const size_t getMaximumWindowSizeInSamples()
        {
            return MAX_MAX_WINDOW_SAMPLES;
        }

        static bool isValidWindowSizeInSamples(size_t samples)
        {
            return Sizes::is_between(
                    samples,
                    MIN_MAX_WINDOW_SAMPLES,
                    MAX_MAX_WINDOW_SAMPLES);
        }

        static constexpr const char *getWindowSizeInSamplesRangeMessage()
        {
            return
                    "RMS window size in samples must lie between "
                    TRUE_RMS_QUOTE(MIN_MAX_WINDOW_SAMPLES)
                    " and "
                    TRUE_RMS_QUOTE(MAX_MAX_WINDOW_SAMPLES)
                    " for minimum of "
                    TRUE_RMS_QUOTE(SNR_BITS)
                    " bits of signal to error-noise ratio and sample type "
                    TRUE_RMS_QUOTE(typename S);
        }

        static size_t validWindowSizeInSamples(size_t samples)
        {
            if (isValidWindowSizeInSamples(samples)) {
                return samples;
            }
            throw std::invalid_argument(getWindowSizeInSamplesRangeMessage());
        }

        static constexpr const size_t getMaximumErrorMitigatingDecaySamples()
        {
            return MAX_ERR_MITIGATING_DECAY_SAMPLES;
        }

        static constexpr const size_t getMinimumErrorMitigatingDecaySamples()
        {
            return MIN_ERR_MITIGATING_DECAY_SAMPLES;
        }

        static bool isValidErrorMitigatingDecaySamples(size_t samples)
        {
            return Sizes::is_between(
                    samples,
                    MIN_ERR_MITIGATING_DECAY_SAMPLES,
                    MAX_ERR_MITIGATING_DECAY_SAMPLES);
        }

        static constexpr const char *
        getErrorMitigatingDecaySamplesRangeMessage()
        {
            return
                    "Error mitigating decay samples must lie between "
                    TRUE_RMS_QUOTE(MIN_ERR_MITIGATING_DECAY_SAMPLES)
                    " and "
                    TRUE_RMS_QUOTE(MAX_ERR_MITIGATING_DECAY_SAMPLES)
                    " for sample type "
                    TRUE_RMS_QUOTE(typename S)
                    ".";
        }

        static size_t validErrorMitigatingDecaySamples(size_t samples)
        {
            if (isValidErrorMitigatingDecaySamples(samples)) {
                return samples;
            }
            throw std::invalid_argument(
                    getErrorMitigatingDecaySamplesRangeMessage());
        }

        static constexpr size_t
        getMinimumErrorMitigatingDecayToWindowSizeRation()
        {
            return MIN_ERROR_DECAY_TO_WINDOW_RATIO;
        }
    };

    template<typename S>
    class WindowForTrueFloatingPointMovingAverage
    {
        size_t windowSize_ = 1;
        S emdFactor_ = 1;
        S inputFactor_ = 1;
        S historyFactor_ = 1;
        size_t readPtr_ = 1;
        S average_ = 0;

    public:

        static inline size_t
        getNextPtr(size_t ptr, size_t min, size_t max)
        {
            return ptr > min ? ptr - 1 : max;
        }

        static inline void
        setNextPtr(size_t &ptr, size_t min, size_t max)
        {
            if (ptr > min) {
                ptr--;
            }
            else
                ptr = max;
        }

        static inline size_t
        getRelative(size_t ptr, size_t delta, size_t min, size_t max)
        {
            size_t newPtr = ptr + delta;
            return newPtr <= max ? newPtr : min +
                                            ((newPtr - min) % (max + 1 - min));
        }

        S getAverage() const { return average_; }

        size_t getWindowSize() const { return windowSize_; }

        S getEmdFactor() const { return emdFactor_; }

        S getInputFactor() const { return inputFactor_; }

        S getHistoryFactor() const { return historyFactor_; }

        S getScale() const { return 1; }

        size_t getReadPtr() const { return readPtr_; }

        void setAverage(S average)
        {
            average_ = average;
        }

        void setWindowSamples(size_t windowSamples, size_t minPtr,
                                        size_t maxPtr, size_t writePtr,
                                        size_t emdSamples)
        {
            windowSize_ = windowSamples;
            emdFactor_ = exp(-1.0 / emdSamples);
            const double unscaledHistoryDecayFactor =
                    exp(-1.0 * this->windowSize_ / emdSamples);
            inputFactor_ = (1.0 - emdFactor_) / (1.0 - unscaledHistoryDecayFactor);
            historyFactor_ = inputFactor_ * unscaledHistoryDecayFactor;
            readPtr_ = getRelative(writePtr, windowSize_, minPtr, maxPtr);
        }

        S
        addInput(S input, const S *const historyBuffer, size_t minPtr, size_t maxPtr)
        {
            double history = historyBuffer[readPtr_];
            setNextPtr(readPtr_, minPtr, maxPtr);
            average_ =
                    emdFactor_ * average_ +
                    inputFactor_ * input -
                    historyFactor_ * history;
//            printf("addInput(%lf, %p, %zu, %zu) : %.18lg = (%.18lf*avg + %.10lg*%lf - %.10lg*%.18lf) -> %.18lg\n",
//                   input, historyBuffer, minPtr, maxPtr, average_, emdFactor_, inputFactor_, input, historyFactor_, history, getAverage());
            return getAverage();
        }

    };

    template<typename S, size_t SNR_BITS = 20, size_t MIN_ERROR_DECAY_TO_WINDOW_RATIO = 10>
    class HistoryAndEmdForTrueFloatingPointMovingAverage
    {
        using Metrics_ = MetricsForTrueFloatingPointMovingAverageMetyrics
                <S, SNR_BITS, MIN_ERROR_DECAY_TO_WINDOW_RATIO>;
        using Entry_ = WindowForTrueFloatingPointMovingAverage<S>;
        const size_t emdSamples_;
        const size_t endHistory_;
        S * const history_;
        size_t writePtr_ = 0;

        static size_t validWindowSize(size_t emdSamples, size_t windowSize)
        {
            if (Metrics_::validWindowSizeInSamples(windowSize) < emdSamples / Metrics_::MIN_MIN_ERROR_DECAY_TO_WINDOW_RATIO) {
                return windowSize;
            }
            throw std::invalid_argument("Invalid combination of window size and ratio between that and error mitigating decay samples.");
        }

    public:
        HistoryAndEmdForTrueFloatingPointMovingAverage(
                const size_t historySamples, const size_t emdSamples) :
                emdSamples_(Metrics_::validErrorMitigatingDecaySamples(emdSamples)),
                endHistory_(validWindowSize(emdSamples, historySamples) - 1),
                history_(new S[endHistory_ + 1])
        {}

        size_t historySize() const { return endHistory_ + 1; }
        size_t emdSamples() const { return emdSamples_; }
        size_t startPtr() const { return 0; }
        size_t endPtr() const { return endHistory_; }
        size_t writePtr() const { return writePtr_; }

        const S get(size_t index) const
        {
            return history_[IndexPolicy::NotGreater::method(index, endHistory_)];
        }
        const S get() const
        {
            return get(writePtr_);
        }
        const S operator[](size_t index) const
        {
            return history_[IndexPolicy::NotGreater::array(index, endHistory_)];
        }
        size_t getNextPtr() const
        {
            return Entry_::getNextPtr(writePtr_, 0, endHistory_);
        }
        void set(size_t index, S value)
        {
            history_[IndexPolicy::NotGreater::method(index, endHistory_)] = value;
        }
        void write(S value)
        {
            history_[writePtr_] = value;
            Entry_::setNextPtr(writePtr_, 0, endHistory_);
        }
        S &operator[](size_t index)
        {
            return history_[IndexPolicy::NotGreater::array(index, endHistory_)];
        }
        void fillWithAverage(const S average)
        {
            for (size_t i = 0; i <= endHistory_; i++) {
                history_[i] = average;
            }
        }
        const S * const history() const { return history_; }
        S * const history() { return history_; }

        ~HistoryAndEmdForTrueFloatingPointMovingAverage()
        {
            delete[] history_;
        }
    };

    template<typename S, size_t SNR_BITS = 20, size_t MIN_ERROR_DECAY_TO_WINDOW_RATIO=10>
    class TrueFloatingPointWeightedMovingAverage
    {
        using History =
                HistoryAndEmdForTrueFloatingPointMovingAverage
                <S, SNR_BITS, MIN_ERROR_DECAY_TO_WINDOW_RATIO>;
        using Window =
                WindowForTrueFloatingPointMovingAverage
                <S>;

        History history;
        Window window;
    public:
        TrueFloatingPointWeightedMovingAverage(
                const size_t maxWindowSize,
                const size_t emdSamples)
                :
                history(maxWindowSize, emdSamples)
        {
        }

        void setAverage(const double average)
        {
            window.setAverage(average);
            history.fillWithAverage(average);
        }

        void setWindowSize(const size_t windowSamples)
        {
            window.setWindowSamples(windowSamples, history.startPtr(), history.endPtr(), history.writePtr(), history.emdSamples());
        }

        void addInput(const double input)
        {
            window.addInput(input, history.history(), history.startPtr(), history.endPtr());
            history.write(input);
        }

        const S getAverage() const { return window.getAverage(); }

        const size_t getReadPtr() const { return window.getReadPtr(); }
        const size_t getWritePtr() const { return history.writePtr(); }
        const S getNextHistoryValue() const { return history.history()[window.getReadPtr()]; }
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
     * @tparam MAX_SAMPLE_HISTORY the maximum sample history, determining the maximum RMS window size
     * @tparam MAX_RCS the maximum number of characteristic times in this array
     */
    template<typename S, size_t SNR_BITS = 20, size_t MIN_ERROR_DECAY_TO_WINDOW_RATIO=10>
    class TrueFloatingPointWeightedMovingAverageSet
    {
        using History =
        HistoryAndEmdForTrueFloatingPointMovingAverage
                <S, SNR_BITS, MIN_ERROR_DECAY_TO_WINDOW_RATIO>;
        using Window =
        WindowForTrueFloatingPointMovingAverage
                <S>;

        static constexpr size_t MINIMUM_TIME_CONSTANTS = 1;
        static constexpr size_t MAXIMUM_TIME_CONSTANTS = 32;
        static constexpr const char * TIME_CONSTANT_MESSAGE =
                "The (maximum) number of time-constants must lie between "
                TRUE_RMS_QUOTE(MINIMUM_TIME_CONSTANTS) " and "
                TRUE_RMS_QUOTE(MAXIMUM_TIME_CONSTANTS) ".";

        using Metrics = MetricsForTrueFloatingPointMovingAverageMetyrics<S, SNR_BITS, MIN_ERROR_DECAY_TO_WINDOW_RATIO>;

        struct Entry {
            Window win;
            S scale;
        };
        const size_t entries_;
        size_t usedWindows_;
        Entry * const entry_;
        History history_;

        static size_t validMaxTimeConstants(size_t constants)
        {
            if (Sizes::is_between(constants, MINIMUM_TIME_CONSTANTS, MAXIMUM_TIME_CONSTANTS)) {
                return constants;
            }
            throw std::invalid_argument(TIME_CONSTANT_MESSAGE);
        }

        size_t checkWindowIndex(size_t index) const
        {
            if (index <= getUsedWindows()) {
                return index;
            }
            throw std::out_of_range("Window index greater than configured windows to use");
        }


    public:

        TrueFloatingPointWeightedMovingAverageSet(
                size_t maxWindowSamples, size_t errorMitigatingTimeConstant, size_t maxTimeConstants, S average) :
                entries_(validMaxTimeConstants(maxTimeConstants)),
                usedWindows_(entries_),
                entry_(new Entry[entries_]),
                history_(maxWindowSamples, errorMitigatingTimeConstant)
        {
            history_.fillWithAverage(average);
            for (size_t i = 0; i < entries_; i++) {
                entry_[i].win.setAverage(0);
                setWindowSizeAndScale(i, i * maxWindowSamples / entries_, 1.0);
            }
        }

        size_t getMaxWindows() const { return entries_; }
        size_t getUsedWindows() const { return usedWindows_; }
        size_t getMaxWindowSamples() const { return history_.historySize(); }

        void setUsedWindows(size_t windows)
        {
            if (windows > 0 && windows <= getMaxWindows()) {
                usedWindows_ = windows;
            }
            else {
                throw std::out_of_range(
                        "Number of used windows zero or larger than condigured maximum at construction");
            }
        }

        void setWindowSizeAndScale(size_t index, size_t windowSamples, S scale)
        {
            if (windowSamples > getMaxWindowSamples()) {
                throw std::out_of_range("Window size in samples is larger than configured maximum at construction.");
            }
            Entry &entry = entry_[checkWindowIndex(index)];
            entry.win.setWindowSamples(
                    windowSamples, history_.startPtr(), history_.endPtr(),
                    history_.writePtr(), history_.emdSamples());
            entry.scale = Floats::force_between(scale, 0, 1e6);
        }

        void setAverages(S average)
        {
            for (size_t i = 0; i < entries_; i++) {
                entry_[i].win.setAverage(average);
            }
            for (size_t i = 0; i < history_.historySize(); i++) {
                history_.set(i, average);
            }
        }

        S getAverage(size_t index) const
        {
            const Entry &entry = entry_[checkWindowIndex(index)];
            return entry.scale * entry.win.getAverage();
        }

        size_t getWindowSize(size_t index) const
        {
            return entry_[checkWindowIndex(index)].win.getWindowSize();
        }

        S getWindowScale(size_t index) const
        {
            checkWindowIndex(index);
            return entry_[index].scale;
        }

        const S get() const
        {
            return history_.get();
        }

        void addInput(S input) {
            for (size_t i = 0; i < getUsedWindows(); i++) {
                entry_[i].win.addInput(input, history_.history(), history_.startPtr(), history_.endPtr());
            }
            history_.write(input);
        }

        S addInputGetMax(S input, S minimumValue)
        {
            S average = minimumValue;
            for (size_t i = 0; i < getUsedWindows(); i++) {
                Entry &entry = entry_[i];
                entry.win.addInput(input, history_.history(), history_.startPtr(), history_.endPtr());
                average = Values::max(entry.scale * entry.win.getAverage(), average);
            }
            history_.write(input);
            return average;
        }

        size_t getWritePtr() const
        {
            return history_.writePtr();
        }

        size_t getReadPtr(size_t i) const
        {
            return entry_[i].win.getReadPtr();
        }

        ~TrueFloatingPointWeightedMovingAverageSet()
        {
            delete[] entry_;
        }
    };

} /* End of name space tdap */

#endif /* TDAP_TRUE_RMS_HEADER_GUARD */
