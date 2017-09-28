/*
 * tdap/AdvancedRmsDetector.hpp
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

#ifndef TDAP_ADVANCED_RMS_DETECTOR_HEADER_GUARD
#define TDAP_ADVANCED_RMS_DETECTOR_HEADER_GUARD

#include <type_traits>
#include <cmath>
#include <limits>

#include <tdap/Integration.hpp>
#include <tdap/Followers.hpp>
#include <tdap/Rms.hpp>

namespace tdap {

    struct AdvancedRms
    {
        static constexpr double PERCEPTIVE_FAST_WINDOWSIZE = 0.050;
        static constexpr double PERCEPTIVE_SLOW_WINDOWSIZE = 0.400;
        static constexpr double MAX_BUCKET_INTEGRATION_TIME = 0.025;

        static ValueRange<double> &peakWeightRange()
        {
            static ValueRange<double> range(0.25, 1.0);
            return range;
        }

        static ValueRange<double> &slowWeightRange()
        {
            static ValueRange<double> range(0.5, 2.0);
            return range;
        }

        static ValueRange<double> &minRcRange()
        {
            static ValueRange<double> range(0.0002, 0.02);
            return range;
        }

        static ValueRange<double> &maxRcRange()
        {
            static ValueRange<double> range(0.100, 4.0);
            return range;
        }

        static ValueRange<double> &followRcRange()
        {
            static ValueRange<double> range(0.0005, 0.025);
            return range;
        }

        static ValueRange<double> &followHoldTimeRange()
        {
            static ValueRange<double> range(0.001, 0.050);
            return range;
        }

        struct UserConfig
        {
            double minRc;
            double maxRc;
            double peakWeight;
            double slowWeight;

            UserConfig validate()
            {
                return {
                        minRcRange().getBetween(Value<double>::min(minRc, maxRc / 2)),
                        maxRcRange().getBetween(Value<double>::max(maxRc, minRc * 2)),
                        peakWeightRange().getBetween(peakWeight),
                        slowWeightRange().getBetween(slowWeight)
                };
            }

            UserConfig standard()
            {
                return {0.0005, 0.4, 0.5, 1.5};
            }
        };

        template<typename T>
        struct RuntimeConfig
        {
            static_assert(is_floating_point<T>::value, "Need floating point type");
            static constexpr size_t RC_TIMES = 11;
            size_t smallWindowSamples;
            FixedSizeArray<T, RC_TIMES> scale;
            size_t followAttackSamples;
            size_t followReleaseSamples;
            size_t followHoldSamples;

            void calculate(UserConfig userConfig, double sampleRate)
            {
                UserConfig config = userConfig.validate();
                double largeRc = PERCEPTIVE_SLOW_WINDOWSIZE;
                followAttackSamples = 0.5 + sampleRate * userConfig.minRc;
                followHoldSamples = 0.5 + sampleRate * 0.005;
                followReleaseSamples = 0.5 + sampleRate * 0.010;

                double smallRc = largeRc * pow(0.5, RC_TIMES - 1);
                smallWindowSamples = smallRc * sampleRate;
                size_t i = 0;
                double rc = smallRc;

                for (int i = 0; i < RC_TIMES; i++, rc *= 2.0) {
                    if (i == 0) {
                        scale[i] = 0.25;
                    }
                    else if (Values::relative_distance(rc, PERCEPTIVE_SLOW_WINDOWSIZE) < 0.1) {
                        scale[i] = 1.0;
                    }
                    else if (rc < PERCEPTIVE_SLOW_WINDOWSIZE) {
                        scale[i] = pow(rc / PERCEPTIVE_SLOW_WINDOWSIZE, 0.25);
                    }
                    else  {
                        scale[i] = std::min(pow(PERCEPTIVE_SLOW_WINDOWSIZE / rc, 0.5), 1.0);
                    }
                }
                rc = smallRc;
                for (size_t i = 0; i < RC_TIMES; i++, rc *= 2.0) {
                        std::cout << "RC " << (1000 * rc) << " ms. level=" << scale[i] << std::endl;
                }
            }
        };

        template<typename T>
        class Detector
        {
            static ValueRange<double> &peakWeightRange()
            {
                static ValueRange<double> range(0.25, 1.0);
                return range;
            }

            static ValueRange<double> &minRcRange()
            {
                static ValueRange<double> range(0.0002, 0.02);
                return range;
            }

            static ValueRange<double> &maxRcRange()
            {
                static ValueRange<double> range(0.05, 4.0);
                return range;
            }

            static ValueRange<double> &followRcRange()
            {
                static ValueRange<double> range(0.001, 0.010);
                return range;
            }

            MultiRcRms<T, (size_t) 16, RuntimeConfig<T>::RC_TIMES> filter_;
            SmoothHoldMaxAttackRelease<T> follower_;

        public:
            Detector() : follower_(1, 1, 1, 1)
            {
            }

            void userConfigure(UserConfig userConfig, double sampleRate)
            {
                RuntimeConfig<T> runtimeConfig;
                runtimeConfig.calculate(userConfig, sampleRate);
                configure(runtimeConfig);
            }

            void configure(RuntimeConfig<T> config)
            {
                follower_ = SmoothHoldMaxAttackRelease<T>(
                        config.followHoldSamples,
                        config.followAttackSamples,
                        config.followReleaseSamples,
                        0);
                filter_.setSmallWindowAndRc(config.smallWindowSamples, 4, 2);
                filter_.setIntegrators(0.01);
                for (size_t i = 0; i < RuntimeConfig<T>::RC_TIMES; i++) {
                    filter_.set_scale(i, config.scale[i]);
                }
                filter_.setValue(10);
                follower_.setValue(10);
            }

            void setValue(T x)
            {
                follower_.setValue(x);
            }

            size_t getHoldSamples() const
            {
                return follower_.getHoldSamples();
            }

            T integrate_smooth(T squareInput, T minOutput, T &squaredSignal)
            {
                T value = filter_.addSquareGetValue(squareInput, minOutput, squaredSignal);
                return follower_.apply(value);
            }
        };

    };


} /* End of name space tdap */

#endif /* TDAP_ADVANCED_RMS_DETECTOR_HEADER_GUARD */
