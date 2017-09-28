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

#include <tdap/FixedSizeArray.hpp>
#include <tdap/Integration.hpp>
#include <tdap/Power2.hpp>

namespace tdap {
    using namespace std;

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
            size_t min_buckets = std::max(MIN_BUCKETS,
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

    struct PerceptiveMetrics
    {
        static constexpr double PERCEPTIVE_SECONDS = 0.400;
        static constexpr double PEAK_SECONDS = 0.0004;
        static constexpr double PEAK_HOLD_SECONDS = 0.0050;
        static constexpr double PEAK_RELEASE_SECONDS = 0.0100;
        static constexpr double MAX_SECONDS = 10.0000;
        static constexpr double PEAK_PERCEPTIVE_RATIO =
                PEAK_SECONDS / PERCEPTIVE_SECONDS;
    };

    template<typename S, size_t BUCKETS, size_t LEVELS>
    class PerceptiveRms : protected PerceptiveMetrics
    {
        static_assert(Values::is_between(LEVELS, (size_t) 3, (size_t) 16),
                      "Levels must be between 3 and 16");

        static constexpr double INTEGRATOR_WINDOW_SIZE_RATIO =
                BucketIntegratedRms<S, BUCKETS>::INTEGRATOR_WINDOW_SIZE_RATIO;

        using Rms = BucketIntegratedRms<S, BUCKETS>;
        FixedSizeArray <Rms, LEVELS> rms_;
        size_t used_levels_ = LEVELS;
        SmoothHoldMaxAttackRelease <S> follower_;

        S get_biggest_window_size(S biggest_window) const
        {
            S limited_window = std::min(MAX_SECONDS, limited_window);

            if (limited_window < PERCEPTIVE_SECONDS) {
                return PERCEPTIVE_SECONDS;
            }
            double big_to_perceptive_log =
                    (log(limited_window) - log(PERCEPTIVE_SECONDS)) /
                    M_LN2;
            if (big_to_perceptive_log < 0.5) {
                return PERCEPTIVE_SECONDS;
            }
            return limited_window;
        }

        void determine_number_of_levels(double biggest_window, size_t levels,
                                        size_t &bigger_levels,
                                        size_t &smaller_levels)
        {
            double bigger_weight =
                    log(biggest_window) - log(PERCEPTIVE_SECONDS);
            double smaller_weight =
                    log(PERCEPTIVE_SECONDS) - log(PEAK_SECONDS);
            // apart frm perceptive window, we always have used_levels - 1 available windows to divide for
            bigger_levels = bigger_weight * (LEVELS - 1) /
                            (smaller_weight + bigger_weight);
            smaller_levels = std::max(1.0, smaller_weight * (LEVELS - 1) /
                                         (smaller_weight +
                                          bigger_weight));
            size_t extra_levels = bigger_levels + smaller_levels;
            while (extra_levels < LEVELS - 1) {
                if (biggest_window > PERCEPTIVE_SECONDS) {
                    bigger_levels++;
                }
                else {
                    smaller_levels++;
                }
                extra_levels = bigger_levels + smaller_levels;
            }
        }

    public:
        PerceptiveRms() : follower_(1, 1, 1, 1) {};

        void configure(size_t sample_rate, S biggest_window, S peak_to_rms,
                       S integration_to_window_size = INTEGRATOR_WINDOW_SIZE_RATIO,
                       size_t levels = LEVELS)
        {
            used_levels_ = Value<size_t>::force_between(levels, 3, LEVELS);
            double peak_scale = 1.0 / Value<S>::force_between(peak_to_rms, 2, 10);
            double biggest = get_biggest_window_size(biggest_window);
            double integration_factor = Value<S>::force_between(
                    integration_to_window_size, 0.1, 1);
            size_t bigger_levels;
            size_t smaller_levels;
            determine_number_of_levels(biggest, levels, bigger_levels,
                                       smaller_levels);

            for (size_t level = 0; level < smaller_levels; level++) {
                double exponent =
                        1.0 * (smaller_levels - level) / smaller_levels;
                double window_size = PERCEPTIVE_SECONDS *
                                     pow(PEAK_PERCEPTIVE_RATIO, exponent);
                double scale = level == 0 ?
                               peak_scale :
                               pow(PEAK_PERCEPTIVE_RATIO, exponent * 0.25);
                double rc = window_size * integration_factor;
                rms_[level].setWindowSizeAndRc(window_size * sample_rate,
                                               rc * sample_rate);
                rms_[level].set_output_scale(scale);
            }

            rms_[smaller_levels].setWindowSizeAndRc(
                    PERCEPTIVE_SECONDS * sample_rate,
                    PERCEPTIVE_SECONDS * sample_rate * integration_factor);
            rms_[smaller_levels].set_output_scale(1.0);

            for (size_t level = smaller_levels + 1;
                 level < used_levels_; level++) {
                double exponent = 1.0 * (level - smaller_levels) /
                                  (used_levels_ - 1 - smaller_levels);
                double window_size = biggest *
                                     pow(biggest / PERCEPTIVE_SECONDS,
                                         exponent);
                double rc = window_size * integration_factor;
                rms_[level].setWindowSizeAndRc(window_size * sample_rate,
                                               rc * sample_rate);
            }

            for (size_t level = 0; level < used_levels_; level++) {
                double window_size =
                        1.0 * rms_[level].get_window_size() / sample_rate;
                double scale = rms_[level].get_output_scale();
                cout
                        << "RMS[" << level
                        << "] window=" << window_size
                        << "s; scale=" << scale << endl;
            }

            follower_ = SmoothHoldMaxAttackRelease<S>(
                    PEAK_HOLD_SECONDS * sample_rate,
                    0.5 + 0.5 * PEAK_SECONDS * sample_rate,
                    PEAK_RELEASE_SECONDS * sample_rate,
                    10);
        }

        S add_square_get_detection(S square, S minimum = 0)
        {
            S value = minimum;
            for (int level = used_levels_; --level > 0; ) {
                value = rms_[level].addSquareCompareAndGet(square, value);
            }
            return follower_.apply(value);
        }

        const FixedSizeArray <BucketIntegratedRms<S, BUCKETS>, LEVELS> &
        rms() const
        {
            return rms_;
        };


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
