/*
 * tdap/Rms.hpp
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

#ifndef TDAP_RMS_HEADER_GUARD
#define TDAP_RMS_HEADER_GUARD

#include <iostream>

#include <type_traits>
#include <cmath>
#include <cstdint>

#include <tdap/FixedSizeArray.hpp>
#include <tdap/Followers.hpp>
#include <tdap/Power2.hpp>

namespace tdap {
    using namespace std;

    class WindowedAveragesControllerUnsafe;

    class WindowedAverageIntegerBaseUnsafe
    {
        friend class WindowedAveragesControllerUnsafe;

        size_t window_size_ = 1;
        int64_t sum_ = 0;
        double scale_ = 1.0;
        size_t sample_ptr_ = 0;

        void configure(
                size_t window_size,
                size_t history_size,
                double initial_sum,
                double output_scale)
        {
            if (window_size < 1) {
                throw std::invalid_argument("Window size must be at least 1");
            }
            window_size_ = window_size;
            scale_ = output_scale / window_size;
            sum_ = initial_sum * window_size;
            sample_ptr_ = history_size - window_size;
//            cout << "WindowedAverageIntegerBaseUnsafe.configure(";
//            cout << "window_size=" << window_size;
//            cout << ", history_size=" << history_size;
//            cout << ", initial_sum=" << initial_sum;
//            cout << ", output_scale=" << output_scale;
//            cout << ")" << endl;
        }

        void add_input(int64_t input, size_t history_size,
                       const int64_t *history_data)
        {
            uint64_t oldest_sample = history_data[sample_ptr_++];
            if (sample_ptr_ >= history_size) {
                sample_ptr_ = 0;
            }
            sum_ = sum_ + input - oldest_sample;
        }

        int64_t add_get_sum(int64_t input, size_t history_size,
                            const int64_t *const history_data)
        {
            add_input(input, history_size, history_data);
            return peek_sum();
        }

        double add_get_average(int64_t input, size_t history_size,
                               const int64_t *const history_data)
        {
            add_input(input, history_size, history_data);
            return peek_average();
        }

    public:

        int64_t peek_sum() const
        {
            return sum_;
        }

        double peek_average() const
        {
            return scale_ * sum_;
        }
    };

    class WindowedAveragesControllerUnsafe
    {
        static int64_t null_buffer[1];

        int64_t *history_data_ = nullptr;
        size_t history_size_ = 0;
        size_t input_ptr_ = 0;

    public:
        void configure(int64_t *history_data, size_t history_size)
        {
            if (history_data == nullptr) {
                throw std::invalid_argument("History data may not be null");
            }
            if (history_size < 1) {
                throw std::invalid_argument("History size must be at least 1");
            }
            history_data_ = history_data;
            history_size_ = history_size;
        }

        void configure_average(WindowedAverageIntegerBaseUnsafe &subject,
                               size_t window_size,
                               double output_scale, double initial_sum)
        {
            if (window_size > history_size_) {
                throw std::invalid_argument(
                        "Window size cannot be greater than history size");
            }
            subject.configure(window_size, history_size_, initial_sum,
                              output_scale);
        }

        template<size_t COUNT>
        void add_input_fixed_count(int64_t input,
                                   WindowedAverageIntegerBaseUnsafe *subjects)
        {
            for (size_t i = 0; i < COUNT; i++) {
                subjects[i].add_input(input, history_size_, history_data_);
            }
            history_data_[input_ptr_++] = input;
            if (input_ptr_ >= history_size_) {
                input_ptr_ = 0;
            }
        }

        void add_input(int64_t input, size_t window_count,
                       WindowedAverageIntegerBaseUnsafe *subjects)
        {
            for (size_t i = 0; i < window_count; i++) {
                subjects[i].add_input(input, history_size_, history_data_);
            }
            history_data_[input_ptr_++] = input;
            if (input_ptr_ >= history_size_) {
                input_ptr_ = 0;
            }
        }
    };

    template<size_t WINDOW_COUNT>
    class WindowedRms
    {
        static constexpr double DEFAULT_SCALE = 65365;
        static constexpr double MAXIMUM_SCALE = 16777216;
        static_assert(WINDOW_COUNT > 0,
                      "WINDOW_COUNT must be a positive number1");

        size_t used_windows_ = WINDOW_COUNT;
        double integer_scale_ = DEFAULT_SCALE;
        double integer_squared_scale_ = DEFAULT_SCALE * DEFAULT_SCALE;
        WindowedAveragesControllerUnsafe controller_;
        WindowedAverageIntegerBaseUnsafe averages_[WINDOW_COUNT];
        IntegrationCoefficients<double> integrator_[WINDOW_COUNT];
        double int1_[WINDOW_COUNT];
        double int2_[WINDOW_COUNT];

        void add_input_raw(double input)
        {
            controller_.add_input(input, used_windows_, averages_);
        }

        void add_input(double input)
        {
            const double scaled = input * integer_scale_;
            add_input_raw(scaled * scaled);
        }

        void add_squared_input(double squared_input)
        {
            add_input_raw(squared_input * integer_squared_scale_);
        }

    public:
        void configure(int64_t *history_data, size_t history_size)
        {
            controller_.configure(history_data, history_size);
        }

        void set_integer_scale(double integer_scale)
        {
            if (integer_scale < 1) {
                throw std::invalid_argument("Integer scale must be at least 1");
            }
            if (integer_scale > MAXIMUM_SCALE) {
                throw std::invalid_argument(
                        "Integer scale cannot exceed 16777216");
            }
            integer_scale_ = integer_scale;
            integer_squared_scale_ = integer_scale_ * integer_scale_;
        }

        void set_used_windows(size_t used_windows)
        {
            if (used_windows < 1) {
                throw std::invalid_argument(
                        "Number of used windows must be at least 1");
            }
            if (used_windows > WINDOW_COUNT) {
                throw std::out_of_range(
                        "Maximum number of used windows exceeded");
            }
            used_windows_ = used_windows;
        }

        void configure_average(size_t window, size_t window_size,
                               size_t history_size,
                               double output_scale, double initial_sum)
        {
            if (window >= used_windows_) {
                throw std::out_of_range(
                        "Configuring window index out of range");
            }
            if (output_scale < 0) {
                throw std::invalid_argument("Output scale must be positive");
            }
            controller_.configure_average(averages_[window], window_size,
                                          output_scale * output_scale / integer_squared_scale_,
                                          initial_sum);
            int1_[window] = averages_[window].peek_average();
            int2_[window] = int1_[window];
            integrator_[window].setCharacteristicSamples(Values::max((size_t)1, window_size / 8));
        }

        double get_max_average(double minimum_squared)
        {
            double value = minimum_squared;
            for (ssize_t i = used_windows_ - 1; i >= 0; i--) {
                double average = averages_[i].peek_average();
                int1_[i] = integrator_[i].integrate(average, int1_[i]);
                double smooth1 = Values::max(int1_[i], value);
                int2_[i] = value = integrator_[i].integrate(smooth1, int2_[i]);
            }
            return value;
        }

        double get_max_rms(double minimum_squared)
        {
            return sqrt(get_max_average(minimum_squared));
        }

        double add_input_get_max_average(double input, double minimum)
        {
            add_input(input);
            return get_max_average(minimum * minimum);
        }

        double add_squared_input_get_max_average(double squared_input,
                                                 double squared_minimum)
        {
            add_squared_input(squared_input);
            return get_max_average(squared_minimum);
        }

        double add_squared_input_get_max_rms(double squared_input,
                                                 double squared_minimum)
        {
            add_squared_input(squared_input);
            return get_max_rms(squared_minimum);
        }
    };


    struct PerceptiveMetrics
    {
        static constexpr double PERCEPTIVE_SECONDS = 0.400;
        static constexpr double PEAK_SECONDS = 0.0003;
        static constexpr double PEAK_HOLD_SECONDS = 0.0016;//0.0050
        static constexpr double PEAK_RELEASE_SECONDS = 0.0032; // 0.0100
        static constexpr double MAX_SECONDS = 10.0000;
        static constexpr double PEAK_PERCEPTIVE_RATIO =
                PEAK_SECONDS / PERCEPTIVE_SECONDS;
    };

    template<typename S>
    class PerceptiveRms
    {
        static constexpr size_t LEVELS = 16;
        static_assert(Values::is_between(LEVELS, (size_t) 3, (size_t) 16),
                      "Levels must be between 3 and 16");

        WindowedRms<LEVELS> rms_;
        size_t used_levels_ = LEVELS;
        SmoothHoldMaxAttackRelease <S> follower_;

        S get_biggest_window_size(S biggest_window) const
        {
            S limited_window = Value<S>::min(PerceptiveMetrics::MAX_SECONDS,
                                             biggest_window);

            if (limited_window < PerceptiveMetrics::PERCEPTIVE_SECONDS * 1.4) {
                return PerceptiveMetrics::PERCEPTIVE_SECONDS;
            }
            return limited_window;
        }

        void determine_number_of_levels(double biggest_window,
                                        size_t &bigger_levels,
                                        size_t &smaller_levels)
        {
            if (bigger_levels == 0 ||
                biggest_window == PerceptiveMetrics::PERCEPTIVE_SECONDS) {
                smaller_levels = Value<size_t>::min(smaller_levels, LEVELS - 1);
                bigger_levels = 0;
                return;
            }
            double bigger_weight =
                    log(biggest_window) -
                    log(PerceptiveMetrics::PERCEPTIVE_SECONDS);
            double smaller_weight =
                    log(PerceptiveMetrics::PERCEPTIVE_SECONDS) -
                    log(PerceptiveMetrics::PEAK_SECONDS);

            while (bigger_levels + smaller_levels + 1 > LEVELS) {
                if (bigger_levels * smaller_weight >
                    smaller_levels * bigger_weight) {
                    bigger_levels--;
                }
                if (biggest_window >
                    PerceptiveMetrics::PERCEPTIVE_SECONDS * 1.3) {
                    bigger_levels++;
                }
                else {
                    smaller_levels++;
                }
            }
        }

    public:
        PerceptiveRms() : follower_(1, 1, 1, 1)
        {};

        void configure(size_t sample_rate,
                       int64_t *history_data, size_t history_size,
                       S peak_to_rms,
                       size_t steps_to_peak,
                       S biggest_window, size_t steps_to_biggest,
                       S initial_value = 0.0)
        {
            size_t smaller_levels = Value<size_t>::max(steps_to_peak, 1);
            double biggest = get_biggest_window_size(biggest_window);
            size_t bigger_levels =
                    biggest == PerceptiveMetrics::PERCEPTIVE_SECONDS ? 0
                                                                     : Value<
                            size_t>::max(steps_to_biggest, 1);
            cout << "Levels smaller " << smaller_levels << " bigger "
                 << bigger_levels << endl;
            if (smaller_levels + bigger_levels + 1 > LEVELS) {
                throw std::invalid_argument(
                        "Rms::configure_average: too many levels specified");
            }
            used_levels_ = smaller_levels + bigger_levels + 1;
            rms_.set_used_windows(used_levels_);
            double peak_scale =
                    1.0 / Value<S>::force_between(peak_to_rms, 2, 10);
            S initial_avererage = Value<S>::force_between(initial_value, 0.0,
                                                          100.0);

            size_t window_size[LEVELS];
            double scale[LEVELS];

            for (size_t level = 0; level < smaller_levels; level++) {
                double exponent =
                        1.0 * (smaller_levels - level) / smaller_levels;
                window_size[level] = 0.5 +
                                     sample_rate *
                                     PerceptiveMetrics::PERCEPTIVE_SECONDS *
                                     pow(
                                             PerceptiveMetrics::PEAK_PERCEPTIVE_RATIO,
                                             exponent);
                scale[level] = level == 0 ?
                               peak_scale :
                               pow(PerceptiveMetrics::PEAK_PERCEPTIVE_RATIO,
                                   exponent * 0.25);
            }

            window_size[smaller_levels] = 0.5 +
                                          sample_rate *
                                          PerceptiveMetrics::PERCEPTIVE_SECONDS;
            scale[smaller_levels] = 1.0;

            for (size_t level = smaller_levels + 1;
                 level < used_levels_; level++) {
                double exponent = 1.0 * (level - smaller_levels) /
                                  (used_levels_ - 1 - smaller_levels);
                window_size[level] = 0.5 +
                                     sample_rate *
                                     PerceptiveMetrics::PERCEPTIVE_SECONDS *
                                     pow(
                                             biggest /
                                             PerceptiveMetrics::PERCEPTIVE_SECONDS,
                                             exponent);
                scale[level] = 1.0;
            }

            size_t max_window_size = 0;
            for (size_t level = 0; level < used_levels_; level++) {
                max_window_size = Values::max(max_window_size,
                                              window_size[level]);
            }

            if (max_window_size >= history_size) {
                for (size_t level = 0; level < used_levels_; level++) {
                    window_size[level] = Values::min(max_window_size,
                                                     window_size[level]);
                }
            }

            rms_.configure(history_data, max_window_size);

            for (size_t level = 0; level < used_levels_; level++) {
                rms_.configure_average(level, window_size[level],
                                       max_window_size, scale[level],
                                       initial_avererage);
            }

            for (size_t level = 0; level < used_levels_; level++) {
                cout
                        << "RMS[" << level
                        << "] window=" << window_size[level]
                        << "; scale=" << scale[level] << endl;
            }

            follower_ = SmoothHoldMaxAttackRelease<S>(
                    PerceptiveMetrics::PEAK_HOLD_SECONDS * sample_rate,
                    0.5 + 0.5 * PerceptiveMetrics::PEAK_SECONDS * sample_rate,
                    PerceptiveMetrics::PEAK_RELEASE_SECONDS * sample_rate,
                    10);
        }

        S add_square_get_detection(S squared_input, S squared_minimum = 0)
        {
            return follower_.apply(rms_.add_squared_input_get_max_rms(squared_input, squared_minimum));
        }
    };

    template<typename S, size_t MAX_SIZE, size_t HISTORY_SIZE_PER_RMS>
    class PerceptiveRmsSet
    {
        static_assert(HISTORY_SIZE_PER_RMS >= 100000, "History size must be greater than 100000");
        static_assert(HISTORY_SIZE_PER_RMS <= 100000000, "History size must be smaller than 100000000");
        static_assert(MAX_SIZE > 0, "Number of RMSes must be positive");

        int64_t history_data_[MAX_SIZE * HISTORY_SIZE_PER_RMS];
        PerceptiveRms<S> rms_[MAX_SIZE];

    public:
        void configure(size_t index, size_t sample_rate,
                       S peak_to_rms,
                       size_t steps_to_peak,
                       S biggest_window, size_t steps_to_biggest,
                       S initial_value = 0.0)
        {
            if (index >= MAX_SIZE) {
                throw std::out_of_range("Configure RMS: RMS Index outof range");
            }
            if (biggest_window > 100) {
                throw std::invalid_argument("Window should not be larger than 100 seconds");
            }
            double max_seconds = 1.0 * HISTORY_SIZE_PER_RMS / sample_rate;
            if (biggest_window > max_seconds) {
                throw std::invalid_argument("Biggest window size too much fot history buffer");
            }
            size_t history_size = biggest_window * sample_rate;
            rms_[index].configure(
                    sample_rate,
                    history_data_ + history_size * index,
                    history_size,
                    peak_to_rms,
                    steps_to_peak,
                    biggest_window,
                    steps_to_biggest,
                    initial_value);
        }

        S add_square_get_detection(size_t index, S squared_input, S squared_minimum = 0)
        {
            if (index >= MAX_SIZE) {
                throw std::out_of_range("Configure RMS: RMS Index out of range");
            }
            return rms_[index].add_square_get_detection(squared_input, squared_minimum);
        }

    };



    template<typename T, size_t MAX_BUCKETS>
    struct BucketAverage
    {
        static constexpr size_t MAX_MAX_BUCKETS = 64;
        static constexpr size_t MIN_BUCKETS = 2;
        static_assert(Value<size_t>::is_between(MAX_BUCKETS, MIN_BUCKETS,
                                                MAX_MAX_BUCKETS),
                      "Number of buckets must be between 2 and 64");

        BucketAverage() = default;

        BucketAverage(size_t window_size, T scale, T initial_average)
        {
            set_approximate_window_size(window_size);
            set_output_scale(scale);
            set_average(initial_average);
        }

        size_t get_window_size() const
        { return bucket_count_ * bucket_size_; }

        size_t get_bucket_count() const
        { return bucket_count_; }

        size_t get_bucket_size() const
        { return bucket_size_; }


        size_t
        set_approximate_window_size(size_t window_samples,
                                    double max_relative_error = 0.01,
                                    size_t minimum_preferred_bucket_count = MIN_BUCKETS)
        {
            double errors[MAX_BUCKETS + 1 - MIN_BUCKETS];
            size_t max_error = window_samples;
            size_t min_buckets = Value<size_t>::max(MIN_BUCKETS,
                                                    minimum_preferred_bucket_count);

            double min_error = 1;

            for (size_t bucket_count = MAX_BUCKETS;
                 bucket_count >= min_buckets; bucket_count--) {

                size_t bucket_size = window_samples / bucket_count;
                size_t window_size = bucket_size * bucket_count;
                double error =
                        fabs(window_samples - window_size) / window_samples;
                if (error < max_relative_error) {
                    set_bucket_size_and_count(bucket_size, bucket_count);
                    return window_size;
                }
                errors[MAX_BUCKETS - bucket_count] = error;
                if (error < min_error) {
                    min_error = error;
                }
            }

            for (size_t bucket_count = min_buckets - 1;
                 bucket_count >= MIN_BUCKETS; bucket_count--) {

                size_t bucket_size = window_samples / bucket_count;
                size_t window_size = bucket_size * bucket_count;
                double error =
                        fabs(window_samples - window_size) / window_samples;
                if (error < max_relative_error) {
                    set_bucket_size_and_count(bucket_size, bucket_count);
                    return window_size;
                }
                errors[MAX_BUCKETS - bucket_count] = error;
                if (error < min_error) {
                    min_error = error;
                }
            }
            for (size_t bucket_count = MAX_BUCKETS;
                 bucket_count >= MIN_BUCKETS; bucket_count--) {

                if (errors[MAX_BUCKETS - bucket_count] == min_error) {
                    // There is always a bucket count that meets this condition
                    size_t bucket_size = window_samples / bucket_count;
                    size_t window_size = bucket_size * bucket_count;

                    set_bucket_size_and_count(bucket_size, bucket_count);
                    return window_size;
                }
            }
            // There is always a bucket count that meets the break condition
            // in the previous loop, so we never end up here.
            return 0;
        }

        bool set_bucket_count(size_t count)
        {
            return set_bucket_size_and_count(bucket_size_, count);
        }

        bool set_bucket_size(size_t size)
        {
            return set_bucket_size_and_count(size, bucket_count_);
        }

        bool set_bucket_size_and_count(size_t size, size_t count)
        {
            if (size == 0) {
                return false;
            }
            if (!Value<size_t>::is_between(count, MIN_BUCKETS, MAX_BUCKETS)) {
                return false;
            }
            bucket_count_ = count;
            bucket_size_ = size;
            size_t window_size = get_window_size();
            scale_ = output_scale_ / window_size;
            set_average(sum_ / window_size);
            return true;
        }

        bool set_output_scale(const T scale)
        {
            if (scale <= std::numeric_limits<T>::epsilon()) {
                return false;
            }
            output_scale_ = scale;
            scale_ = output_scale_ / get_window_size();
            return true;
        }

        T get_output_scale() const
        {
            return output_scale_;
        }

        void set_average(T average)
        {
            current_sample_ = bucket_size_;
            current_bucket_ = 0;
            T bucket_value = average * bucket_size_;
            for (size_t i = 0; i < bucket_count_; i++) {
                bucket_[i] = bucket_value;
            }
            sum_ = bucket_value * bucket_count_;
            old_bucket_sum_ = sum_ - bucket_value;
            new_bucket_sum = 0;
            average_square_sample_value = average;
        }

        T add_sample_get_average(const T sample)
        {
            if (current_sample_ > 0) {
                new_bucket_sum += sample;
                sum_ = old_bucket_sum_ + new_bucket_sum +
                       current_sample_ * average_square_sample_value;
                current_sample_--;
                return get_average();
            }
            sum_ = old_bucket_sum_ + new_bucket_sum;
            bucket_[current_bucket_] = new_bucket_sum;

            current_bucket_++;
            current_bucket_ %= bucket_count_;
            current_sample_ = bucket_size_;

            new_bucket_sum = 0;

            old_bucket_sum_ = 0;
            for (size_t bucket = current_bucket_ + 1;
                 bucket < bucket_count_; bucket++) {
                old_bucket_sum_ += bucket_[bucket];
            }
            for (size_t bucket = 0; bucket <= current_bucket_; bucket++) {
                old_bucket_sum_ += bucket_[bucket];
            }
            average_square_sample_value = get_average();
            return average_square_sample_value;
        }

        T get_average() const
        {
            return sum_ * scale_;
        }

        std::ostream &operator>>(std::ostream &out)
        {
            out
                    << "BucketAverage<T," << MAX_BUCKETS
                    << ">(bucket_size=" << get_bucket_size()
                    << ",bucket_count=" << get_bucket_count()
                    << ",window_size=" << get_window_size();

            return out;
        }

    private:
        T bucket_[MAX_BUCKETS];
        T old_bucket_sum_ = 0;
        T new_bucket_sum = 0;
        T average_square_sample_value = 0;
        T sum_ = 0;
        T output_scale_ = 1.0;
        T scale_ = 1.0 / MAX_BUCKETS;
        size_t bucket_size_ = 1;
        size_t bucket_count_ = MAX_BUCKETS;
        size_t current_bucket_ = MAX_BUCKETS;
        size_t current_sample_ = 0;
    };


    template<typename S, size_t N>
    class BucketRms
    {
        static_assert(is_arithmetic<S>::value,
                      "Expected floating-point type parameter");
        static_assert(N > 0 && N < 1024, "Bucket count out range");

        BucketAverage<S, N> average_;

    public:
        BucketRms() = default;

        BucketRms(size_t window_size, S scale, S initial_average) :
                average_(window_size, scale, initial_average)
        {}

        void zero()
        {
            average_.set_average(0);
        }

        void setvalue(S value)
        {
            average_.set_average(value * value);
        }

        void set_output_scale(S scale)
        {
            average_.set_output_scale(scale);
        }

        S get_output_scale() const
        {
            return average_.get_output_scale();
        }

        const size_t getWindowSize() const
        {
            return average_.get_window_size();
        }

        size_t setWindowSize(size_t windowSize)
        {
            return average_.set_approximate_window_size(windowSize);
        }

        S addSquareAndGet(S square)
        {
            return sqrt(addSquareAndGetSquare(square));
        }

        S addSquareAndGetSquare(S square)
        {
            return average_.add_sample_get_average(square);
        }
    };

    template<typename S, size_t BUCKET_COUNT>
    class BucketIntegratedRms
    {
        static_assert(BUCKET_COUNT >= 2 && BUCKET_COUNT <= 64,
                      "Invalid number of buckets");
        static_assert(is_floating_point<S>::value,
                      "Expected floating-point type parameter");

        BucketRms<S, BUCKET_COUNT> rms_;
        IntegrationCoefficients <S> coeffs_;
        S int1_, int2_;

    public:
        static constexpr double INTEGRATOR_WINDOW_SIZE_RATIO = 0.25;
        static constexpr double MINIMUM_INTEGRATION_TO_WINDOW_RATIO = 0.01;

        BucketIntegratedRms() = default;

        BucketIntegratedRms(size_t window_size, S scale, S initial_average) :
                rms_(window_size, scale, initial_average),
                int1_(initial_average),
                int2_(initial_average),
                coeffs_(INTEGRATOR_WINDOW_SIZE_RATIO * rms_.getWindowSize())
        {
        }

        size_t setWindowSizeAndRc(size_t newSize, size_t rcSize)
        {
            size_t windowSize = rms_.setWindowSize(newSize);
            size_t minRc = windowSize / 10;
            coeffs_.setCharacteristicSamples(Values::max(minRc, rcSize));
            return windowSize;
        }

        size_t setWindowSize(size_t newSize)
        {
            return setWindowSizeAndRc(newSize,
                                      INTEGRATOR_WINDOW_SIZE_RATIO * newSize);
        }

        size_t get_window_size() const
        {
            return rms_.getWindowSize();
        }

        size_t get_integration_time() const
        {
            return coeffs_.getCharacteristicSamples();
        }

        void zero(S value)
        {
            rms_.zero();
            int1_ = int2_ = 0;
        }

        void setValue(S value)
        {
            rms_.setvalue(value);
            int1_ = int2_ = value;
        }

        void set_output_scale(S scale)
        {
            rms_.set_output_scale(scale);
        }

        S get_output_scale() const
        {
            return rms_.get_output_scale();
        }

        S addAndGet(S value)
        {
            return addSquareAndGet(value * value);
        }

        S addSquareAndGet(S square)
        {
            S rms = rms_.addSquareAndGet(square);
            return coeffs_.integrate(coeffs_.integrate(rms, int1_), int2_);
        }

        S addSquareCompareAndGet(S square, S minimumRms)
        {
            S rms = rms_.addSquareAndGet(square);
            S rms_int = coeffs_.integrate(rms, int1_);
            S max = Values::max(minimumRms, rms_int);
            return coeffs_.integrate(max, int2_);
        }
    };

    template<typename S, size_t BUCKETS, size_t LEVELS>
    class MultiBucketMean
    {

    public:
        static constexpr size_t MINIMUM_BUCKETS = 4;
        static constexpr size_t MAXIMUM_BUCKETS = 64;
        static_assert(is_arithmetic<S>::value,
                      "Expected floating-point type parameter");
        static_assert(Power2::constant::is(BUCKETS),
                      "Bucket count must be valid power of two");
        static_assert(
                Values::is_between(BUCKETS, MINIMUM_BUCKETS, MAXIMUM_BUCKETS),
                "Bucket count must be between 4 and 256");
        static_assert(Values::is_between(LEVELS, (size_t) 1, (size_t) 16),
                      "Levels must be between 1 and 16");

        static constexpr size_t BUCKET_MASK = BUCKETS - 1;

    private:
        struct BucketEntry
        {
            size_t current_;
            S bucket_[BUCKETS];
            BucketEntry *next_;
            S mean_;
            S weight_;

            void addBucketValue(S value)
            {
                bucket_[current_] = value;
                if (next_ && (current_ & 1)) {
                    S sum = value;
                    next_->addBucketValue(value + bucket_[current_ - 1]);
                }
                current_++;
                current_ &= BUCKET_MASK;
                S sum = 0;
                for (size_t i = 0; i < BUCKETS; i++) {
                    sum += bucket_[i];
                }
                mean_ = sum * weight_;
            }

            void zero()
            {
                setValue(static_cast<S>(0));
            }

            void setValue(S value)
            {
                for (size_t i = 0; i < BUCKETS; i++) {
                    bucket_[i] = value;
                }
                current_ = 0;
                mean_ = value;
            }

            void operator=(const BucketEntry &source)
            {
                current_ = source.current_;
                mean_ = source.mean_;
                for (size_t i = 0; i < BUCKETS; i++) {
                    bucket_[i] = source.bucket_[i];
                }
            }
        };

        BucketEntry entry_[LEVELS];

        void init()
        {
            zero();
            size_t i;
            size_t effectiveBuckets = 1;
            for (i = 0; i < LEVELS - 1; i++, effectiveBuckets *= 2) {
                entry_[i].next_ = &entry_[i + 1];
                entry_[i].weight_ = 1.0 / (BUCKETS * effectiveBuckets);
            }
            entry_[i].next_ = nullptr;
            entry_[i].weight_ = 1.0 / (BUCKETS * effectiveBuckets);
        }

    public:
        MultiBucketMean()
        { init(); }

        void zero()
        {
            setValue(static_cast<S>(0));
        }

        void setValue(S value)
        {
            for (size_t i = 0; i < LEVELS; i++) {
                entry_[i].setValue(value);
            }
        }

        inline void addBucketValue(S value)
        {
            entry_[0].addBucketValue(value);
        }

        FixedSizeArray <S, LEVELS> getMeans() const
        {
            FixedSizeArray<S, LEVELS> means;
            for (size_t level = 0; level < LEVELS; level++) {
                means[level] = entry_[level].mean_;
            }
            return means;
        };

        FixedSizeArray <FixedSizeArray<S, BUCKETS>, LEVELS> getBuckets() const
        {
            FixedSizeArray<FixedSizeArray<S, BUCKETS>, LEVELS> buckets;
            for (size_t level = 0; level < LEVELS; level++) {
                for (size_t bucket = 0; bucket < BUCKETS; bucket++) {
                    buckets[level][bucket] = entry_[level].bucket_[bucket];
                }
            }
            return buckets;
        };

        inline S getMean(size_t level) const
        {
            return entry_[IndexPolicy::array(level, LEVELS)].mean_;
        }

        inline S getBucket(size_t level, size_t bucket) const
        {
            entry_[IndexPolicy::array(level,
                                      LEVELS)].bucket_[IndexPolicy::array(
                    bucket, BUCKETS)];
        }

        void operator=(const MultiBucketMean &source)
        {
            for (size_t i = 0; i < LEVELS; i++) {
                entry_[i] = source.entry_[i];
            }
        }
    };

    template<typename S, size_t BUCKETS, size_t LEVELS>
    class MultiRcRms
    {
        MultiBucketMean<S, BUCKETS, LEVELS> mean_;
        S scale_[LEVELS];
        S int1_[LEVELS], int2_[LEVELS];
        S int_;
        size_t samplesPerBucket, sample_;
        S sum;
        IntegrationCoefficients <S> coeffs_[LEVELS];

        void init()
        {
            for (size_t i = 0; i < LEVELS; i++) {
                scale_[i] = 1;
            }
            zero();
            samplesPerBucket = 1;
        }

        void addSquare(S square)
        {
            sum += square;
            sample_++;
            if (sample_ == samplesPerBucket) {
                sample_ = 0;
                mean_.addBucketValue(sum / samplesPerBucket);
                sum = 0;
            }

        }

    public:
        MultiRcRms()
        {
            for (size_t i = 0; i < LEVELS; i++) {
                scale_[i] = 1;
                int1_[i] = 0;
                int2_[i] = 0;
            }
            samplesPerBucket = 1;
            sample_ = 0;
            int_ = 0;
            sum = 0;
            setIntegrators(0);
        }

        size_t setSmallWindow(size_t newSize)
        {
            size_t proposal1 = Values::force_between(newSize, BUCKETS,
                                                     (const size_t) (1e6 *
                                                                     BUCKETS));
            samplesPerBucket = newSize / BUCKETS;
            size_t integrationSamples = samplesPerBucket * BUCKETS / 4;
            for (size_t i = 0; i < LEVELS; i++) {
                coeffs_[i].setCharacteristicSamples(integrationSamples);
                integrationSamples *= 2;
            }
            return samplesPerBucket * BUCKETS;
        }

        size_t
        setSmallWindowAndRc(size_t newSize, double smallRcIntegrationBuckets,
                            double largeRcIntegrationBuckets)
        {
            samplesPerBucket = newSize / BUCKETS;
            S integrationSamples = Value<size_t>::max(samplesPerBucket,
                                                      newSize /
                                                      Value<size_t>::max(2,
                                                                         BUCKETS /
                                                                         4));
            for (size_t i = 0; i < LEVELS; i++, integrationSamples *= 2) {
                coeffs_[i].setCharacteristicSamples(integrationSamples);
            }
            return samplesPerBucket * BUCKETS;
        }

        S set_scale(size_t level, S scale)
        {
            S sc = Values::force_between(scale, 1e-3, 1e6);
            scale_[IndexPolicy::array(level, LEVELS)] = sc * sc;
            return sc;
        }

        S addSquareGetValue(S square, S threshold, S &rawDetection)
        {
            addSquare(square);
            S value = threshold;
            ssize_t level;
            for (level = LEVELS - 1; level >= 0; level--) {
                S scaledMean = sqrt(scale_[level] * mean_.getMean(level));
                S integratedMax = coeffs_[level].integrate(scaledMean,
                                                           int1_[level]);
                S max = Values::max(value, integratedMax);
                value = coeffs_[level].integrate(max, int2_[level]);
            }
            rawDetection = coeffs_[0].integrate(value, int_);
            return value;
        }

        void zero()
        {
            setValue(static_cast<S>(0));
        }

        void setValue(S value)
        {
            S square = value * value;
            mean_.setValue(square);

            setIntegrators(value);
            sample_ = 0;
            sum = samplesPerBucket * square;
        }

        void setIntegrators(S value)
        {
            for (size_t level = 0; level < LEVELS; level++) {
                int1_[level] = int2_[level] = value;
            }
            int_ = value;
        }
    };

    template<typename S>
    using DefaultRms = BucketIntegratedRms<S, 16>;

} /* End of name space tdap */

#endif /* TDAP_RMS_HEADER_GUARD */
