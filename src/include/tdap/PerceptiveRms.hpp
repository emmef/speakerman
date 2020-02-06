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
#include <tdap/Followers.hpp>
#include <tdap/Power2.hpp>
#include <tdap/TrueFloatingPointWindowAverage.hpp>

namespace tdap {
    using namespace std;


    struct PerceptiveMetrics
    {
        static constexpr double PERCEPTIVE_SECONDS = 0.400;
        static constexpr double PEAK_SECONDS = 0.001;
        static constexpr double PEAK_HOLD_SECONDS = 0.002;//0.0050
        static constexpr double PEAK_RELEASE_SECONDS = 0.002; // 0.0100
        static constexpr double MAX_SECONDS = 10.0000;
        static constexpr double PEAK_PERCEPTIVE_RATIO =
                PEAK_SECONDS / PERCEPTIVE_SECONDS;
    };

    template<typename S, size_t MAX_WINDOW_SAMPLES, size_t LEVELS>
    class PerceptiveRms
    {
        static_assert(Values::is_between(LEVELS, (size_t) 3, (size_t) 16),
                      "Levels must be between 3 and 16");


        TrueFloatingPointWeightedMovingAverageSet<S> rms_;

        size_t used_levels_ = LEVELS;
        SmoothHoldMaxAttackRelease <S> follower_;

        S get_biggest_window_size(S biggest_window) const
        {
            S limited_window = Value<S>::min(PerceptiveMetrics::MAX_SECONDS, biggest_window);

            if (limited_window < PerceptiveMetrics::PERCEPTIVE_SECONDS * 1.4) {
                return PerceptiveMetrics::PERCEPTIVE_SECONDS;
            }
            return limited_window;
        }

        void determine_number_of_levels(double biggest_window,
                                        size_t &bigger_levels,
                                        size_t &smaller_levels)
        {
            if (bigger_levels == 0 || biggest_window == PerceptiveMetrics::PERCEPTIVE_SECONDS) {
                smaller_levels = Value<size_t>::min(smaller_levels, LEVELS - 1);
                bigger_levels = 0;
                return;
            }
            double bigger_weight =
                    log(biggest_window) - log(PerceptiveMetrics::PERCEPTIVE_SECONDS);
            double smaller_weight =
                    log(PerceptiveMetrics::PERCEPTIVE_SECONDS) - log(PerceptiveMetrics::PEAK_SECONDS);

            while (bigger_levels + smaller_levels + 1 > LEVELS) {
                if (bigger_levels * smaller_weight > smaller_levels * bigger_weight) {
                    bigger_levels--;
                }
                if (biggest_window > PerceptiveMetrics::PERCEPTIVE_SECONDS * 1.3) {
                    bigger_levels++;
                }
                else {
                    smaller_levels++;
                }
            }
        }

    public:
        PerceptiveRms() : follower_(1, 1, 1, 1), rms_(MAX_WINDOW_SAMPLES, MAX_WINDOW_SAMPLES*10, LEVELS, 0) {};

        void configure(size_t sample_rate, S peak_to_rms,
                       size_t steps_to_peak,
                       S biggest_window, size_t steps_to_biggest,
                       S initial_value = 0.0)
        {
            size_t smaller_levels = Value<size_t >::max(steps_to_peak, 1);
            double biggest = get_biggest_window_size(biggest_window);
            size_t bigger_levels = biggest == PerceptiveMetrics::PERCEPTIVE_SECONDS ? 0 : Value<size_t >::max(steps_to_biggest, 1);
            cout << this << " Levels smaller " << smaller_levels << " bigger " << bigger_levels << endl;
            if (smaller_levels + bigger_levels + 1 > LEVELS) {
                throw std::invalid_argument("Rms::configure: too many levels specified");
            }
            used_levels_ = smaller_levels + bigger_levels + 1;
            rms_.setUsedWindows(used_levels_);
            double peak_scale = 1.0 / Value<S>::force_between(peak_to_rms, 2, 10);
            S initial_avererage = Value<S>::force_between(initial_value, 0.0, 100.0);

            for (size_t level = 0; level < smaller_levels; level++) {
                double exponent =
                        1.0 * (smaller_levels - level) / smaller_levels;
                double window_size = PerceptiveMetrics::PERCEPTIVE_SECONDS *
                                     pow(PerceptiveMetrics::PEAK_PERCEPTIVE_RATIO, exponent);
                double scale = pow(
                    PerceptiveMetrics::PEAK_PERCEPTIVE_RATIO, exponent * 0.25);
                rms_.setWindowSizeAndScale(level, 0.5 + window_size * sample_rate, scale * scale);
            }

            rms_.setWindowSizeAndScale(smaller_levels, 0.5 + PerceptiveMetrics::PERCEPTIVE_SECONDS * sample_rate, 1.0);

            for (size_t level = smaller_levels + 1;
                 level < used_levels_; level++) {
                double exponent = 1.0 * (level - smaller_levels) /
                                  (used_levels_ - 1 - smaller_levels);
                double window_size = PerceptiveMetrics::PERCEPTIVE_SECONDS *
                                     pow(biggest / PerceptiveMetrics::PERCEPTIVE_SECONDS,
                                         exponent);
                rms_.setWindowSizeAndScale(level, 0.5 + window_size * sample_rate, 1.0);
            }

          rms_.setAverages(0.0);

            for (size_t level = 0; level < used_levels_; level++) {
                double window_size =
                        1.0 * rms_.getWindowSize(level) / sample_rate;
                double scale = sqrt(rms_.getWindowScale(level));
                cout
                        << "RMS[" << level
                        << "] window=" << window_size
                        << "; scaler=" << scale << endl;
            }

            follower_ = SmoothHoldMaxAttackRelease<S>(
                    PerceptiveMetrics::PEAK_HOLD_SECONDS * sample_rate,
                    0.5 + 0.25 * PerceptiveMetrics::PEAK_SECONDS * sample_rate,
                    PerceptiveMetrics::PEAK_RELEASE_SECONDS * sample_rate,
                    10);
        }

        S add_square_get_detection(S square, S minimum = 0)
        {
            S value = sqrt(rms_.addInputGetMax(square, minimum));
            return follower_.apply(value);
        }

        S add_square_get_unsmoothed(S square, S minimum = 0)
        {
            return sqrt(rms_.addInputGetMax(square, minimum));
        }


    };

    template<typename S, size_t MAX_WINDOW_SAMPLES, size_t LEVELS, size_t CHANNELS>
    class PerceptiveRmsGroup
    {
        PerceptiveRms<S, MAX_WINDOW_SAMPLES, LEVELS>* rms_;
        S maximum_unsmoothed_detection;
        SmoothHoldMaxAttackRelease <S> follower_;

    public:
        PerceptiveRmsGroup() :
        rms_(new PerceptiveRms<S, MAX_WINDOW_SAMPLES, LEVELS>[CHANNELS]),
        maximum_unsmoothed_detection(0.0),
        follower_(1, 1, 1, 1)
        {
            if (rms_ == nullptr) {
                cout << "RMS not allocated!" << endl;
            }
        }

        void configure(size_t sample_rate, S peak_to_rms,
                       size_t steps_to_peak,
                       S biggest_window, size_t steps_to_biggest,
                       S initial_value = 0.0)
        {
            for (size_t channel = 0; channel < CHANNELS; channel++) {
                rms_[IndexPolicy::force(channel, CHANNELS)].configure(
                        sample_rate, peak_to_rms,steps_to_peak, biggest_window,
                        steps_to_biggest, initial_value);
            }
            follower_ = SmoothHoldMaxAttackRelease<S>(
                    PerceptiveMetrics::PEAK_HOLD_SECONDS * sample_rate,
                    0.5 + 0.5 * PerceptiveMetrics::PEAK_SECONDS * sample_rate,
                    PerceptiveMetrics::PEAK_RELEASE_SECONDS * sample_rate,
                    10);

        }

        void reset_frame_detection()
        {
            maximum_unsmoothed_detection = 0.0;
        }

        void add_square_for_channel(size_t channel, S square, S minimim = 0.0)
        {
            maximum_unsmoothed_detection = Values::max(
                    maximum_unsmoothed_detection,
                    rms_[IndexPolicy::force(channel, CHANNELS)].add_square_get_unsmoothed(square, minimim));
        }

        S get_detection()
        {
            return follower_.apply(maximum_unsmoothed_detection);
        }

        ~PerceptiveRmsGroup()
        {
            delete[] rms_;
        }
    };


} /* End of name space tdap */

#endif /* TDAP_RMS_HEADER_GUARD */
