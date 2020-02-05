/*
 * tdap/Weighting.hpp
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

#ifndef TDAP_WEIGHTING_HEADER_GUARD
#define TDAP_WEIGHTING_HEADER_GUARD

#include <iostream>
#include <cmath>

#include <tdap/IirBiquad.hpp>
#include <tdap/IirButterworth.hpp>



namespace tdap {
    using namespace std;


    struct ACurves
    {
        // Defines the bulk of the curve with a parameterized equalizer
#ifdef TDAP_FULL_ACURVE
        static constexpr double PARAM_CENTER = 2516.0;
        static constexpr double PARAM_GAIN = 19.1;
        static constexpr double PARAM_BANDWIDTH = 8.12;

        // Cuts off higher and lower parts
        static constexpr double HIGH_PASS_FREQUENCY = 125;
        static constexpr double LOW_PASS_FREQUENCY = 21443;

        // Overall filter gain for 0dB @ 1kHz
        static constexpr double OVERALL_GAIN = 0.0597736;
#else
  static constexpr double PARAM_CENTER = 8000.0;
  static constexpr double PARAM_GAIN = 16; // 24 dB
  static constexpr double PARAM_BANDWIDTH = 8; // 3dB / octave on average
  // Overall filter gain for 0dB @ 1kHz
  static constexpr double OVERALL_GAIN = 0.5 * M_SQRT1_2; // 9dB

#endif
        static void setFirstOrder(IirCoefficients &coeffs)
        {
            if (coeffs.order() != 1) {
                if (coeffs.hasFixedOrder()) {
                    throw std::invalid_argument("Coefficient argument cannot be set to order 1");
                }
                coeffs.setOrder(1);
            }
        }

        static void setCurveParameters(IirCoefficients &coeffs, double sampleRate)
        {
            BiQuad::setParametric(coeffs, sampleRate, PARAM_CENTER, PARAM_GAIN, PARAM_BANDWIDTH);
        }

#ifdef TDAP_FULL_ACURVE
        static void setLowPassParameters(IirCoefficients &coeffs, double sampleRate)
        {
            setFirstOrder(coeffs);
            Butterworth::create(coeffs, sampleRate, LOW_PASS_FREQUENCY, Butterworth::Pass::LOW, 1.0);
        }

        static void setHighPassParameters(IirCoefficients &coeffs, double sampleRate)
        {
            setFirstOrder(coeffs);
            Butterworth::create(coeffs, sampleRate, HIGH_PASS_FREQUENCY, Butterworth::Pass::HIGH, 1.0);
        }
#endif
        template<typename SAMPLE>
        class Coefficients
        {
            FixedSizeIirCoefficients<SAMPLE, 2> curve_;
#ifdef TDAP_FULL_ACURVE
            FixedSizeIirCoefficients<SAMPLE, 1> highPass_;
            FixedSizeIirCoefficients<SAMPLE, 1> lowPass_;
#endif
        public:
            Coefficients() = default;

            Coefficients(double sampleRate)
            {
                setSampleRate(sampleRate);
            }

            void setSampleRate(double sampleRate)
            {
                auto coeffs1 = curve_.wrap();
                setCurveParameters(coeffs1, sampleRate);
#ifdef TDAP_FULL_ACURVE
                auto coeffs2 = highPass_.wrap();
                setHighPassParameters(coeffs2, sampleRate);
                auto coeffs3 = lowPass_.wrap();
                setHighPassParameters(coeffs3, sampleRate);
#endif
            }

            const FixedSizeIirCoefficients<SAMPLE, 2> &curve() const
            {
                return curve_;
            }

#ifdef TDAP_FULL_ACURVE

            const FixedSizeIirCoefficients<SAMPLE, 1> &highPass() const
            {
                return highPass_;
            }

            const FixedSizeIirCoefficients<SAMPLE, 1> &lowPass() const
            {
                return lowPass_;
            }
#endif
        };

        template<typename SAMPLE, size_t CHANNELS>
        class Filter
        {
            Coefficients<SAMPLE> coefficients_;

            struct History
            {
                SAMPLE curveX[IirCoefficients::historyForOrder(2)];
                SAMPLE curveY[IirCoefficients::historyForOrder(2)];
#ifdef TDAP_FULL_ACURVE
                SAMPLE lowX[IirCoefficients::historyForOrder(1)];
                SAMPLE lowY[IirCoefficients::historyForOrder(1)];
                SAMPLE highX[IirCoefficients::historyForOrder(1)];
                SAMPLE highY[IirCoefficients::historyForOrder(1)];
#endif
                void reset()
                {
                    for (size_t i = 0; i < IirCoefficients::historyForOrder(2); i++) {
                        curveX[i] = 0;
                        curveY[i] = 0;
                    }
#ifdef TDAP_FULL_ACURVE
                    for (size_t i = 0; i < IirCoefficients::historyForOrder(1); i++) {
                        lowX[i] = 0;
                        lowY[i] = 0;
                        highX[i] = 0;
                        highY[i] = 0;
                    }
#endif
                }
            };

            History history_[CHANNELS];

            struct SingleChannelFilter : public tdap::Filter<SAMPLE>
            {
                Filter<SAMPLE, CHANNELS> &wrapped_;

                virtual void reset()
                { wrapped_.reset(); }

                virtual SAMPLE filter(SAMPLE input)
                {
                    return wrapped_.filter(0, input);
                }

                SingleChannelFilter(Filter<SAMPLE, CHANNELS> &wrapped) :
                        wrapped_(wrapped)
                {}

                SingleChannelFilter(SingleChannelFilter &&source) : wrapped_(source.wrapped_)
                {}
            };

            struct MultiChannelFilter : public tdap::MultiFilter<SAMPLE>
            {
                Filter<SAMPLE, CHANNELS> &wrapped_;

                virtual size_t channels() const override
                { return CHANNELS; }

                virtual void reset() override
                { wrapped_.reset(); }

                virtual SAMPLE filter(size_t idx, SAMPLE input) override
                {
                    return wrapped_.filter(idx, input);
                }

                MultiChannelFilter(Filter<SAMPLE, CHANNELS> &wrapped) :
                        wrapped_(wrapped)
                {}

                MultiChannelFilter(MultiChannelFilter &&source) : wrapped_(source.wrapped_)
                {}
            };

        public :
            Filter() = default;

            Filter(const Coefficients<SAMPLE> &coefficients) : coefficients_(coefficients)
            {}

            Filter(double sampleRate) : coefficients_(sampleRate)
            {}

            void setSampleRate(double sampleRate)
            {
                coefficients_.setSampleRate(sampleRate);
            }

            void reset()
            {
                for (size_t i = 0; i < CHANNELS; i++) {
                    history_[i].reset();
                }
            }

            template<bool flushToZero>
            SAMPLE do_filter(size_t channel, SAMPLE input)
            {
                IndexPolicy::array(channel, CHANNELS);
                SAMPLE y = input;
                y = coefficients_.curve().template do_filter<SAMPLE, flushToZero>(history_[channel].curveX,
                                                                                  history_[channel].curveY, y);
#ifdef TDAP_FULL_ACURVE
                y = coefficients_.lowPass().template do_filter<SAMPLE, flushToZero>(history_[channel].lowX,
                                                                                    history_[channel].lowY, y);
                y = coefficients_.highPass().template do_filter<SAMPLE, flushToZero>(history_[channel].highX,
                                                                                     history_[channel].highY, y);
#endif
                return OVERALL_GAIN * y;
            }

            SAMPLE filter(size_t channel, SAMPLE input)
            {
                return do_filter<false>(channel, input);
            }

            template<size_t N, typename ...A>
            void filterArray(const FixedSizeArrayTraits<SAMPLE, N, A...> &input,
                             FixedSizeArrayTraits<SAMPLE, N, A...> &output)
            {
                for (size_t channel = 0; channel < Value<size_t>::min(CHANNELS, N); channel++) {
                    output[channel] = filter(channel, input[channel]);
                }
            }

            template<typename ...A>
            void filterArray(const ArrayTraits<SAMPLE, A...> &input, ArrayTraits<SAMPLE, A...> &output)
            {
                for (size_t channel = 0;
                     channel < Value<size_t>::min(CHANNELS, input.size(), output.size()); channel++) {
                    output[channel] = filter(channel, input[channel]);
                }
            }

            SingleChannelFilter wrapSingle()
            {
                return SingleChannelFilter(*this);
            }

            MultiChannelFilter wrapMulti()
            {
                return MultiChannelFilter(*this);
            }

            tdap::Filter<SAMPLE> *createFilter()
            {
                return new SingleChannelFilter(*this);
            }

            tdap::MultiFilter<SAMPLE> *createMultiFilter()
            {
                return new MultiChannelFilter(*this);
            }

        };
    };


} /* End of name space tdap */

#endif /* TDAP_WEIGHTING_HEADER_GUARD */
