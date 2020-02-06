/*
 * tdap/Crossovers.hpp
 *
 * Part of TdAP
 * Time-domain Audio Processing
 * Copyright (C) 2015 Michel Fleur.
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

#ifndef TDAP_CROSSOVER_HEADER_GUARD
#define TDAP_CROSSOVER_HEADER_GUARD

#include <tdap/Value.hpp>
#include <tdap/IirButterworth.hpp>
#include <tdap/FixedSizeArray.hpp>
#include <tdap/Noise.hpp>
#include <tdap/Weighting.hpp>

namespace tdap {

    struct Crossovers
    {
        template<typename T, size_t CROSSOVERS, typename S, typename ...A>
        static FixedSizeArray<T, CROSSOVERS>
        validatedCrossoverFrequencies(const ArrayTraits<S, A...> &crossovers)
        {
            FixedSizeArray<T, CROSSOVERS> result;
            result[0] = Value<T>::max(crossovers[0], 40.0);
            T max = Value<T>::min(crossovers[CROSSOVERS - 1], 10000.0);
            result[CROSSOVERS - 1] = max;
            for (size_t i = 1; i < CROSSOVERS - 1; i++) {
                T c1 = Value<T>::max(crossovers[i], crossovers[i - 1] * 1.5);
                if (c1 * 1.5 >= max) {
                    throw std::invalid_argument("Too many crossovers for range");
                }
                result[i] = c1;
            }
            return result;
        };

        template<typename T, size_t CHANNELS>
        struct LinkwitzRiley
        {
            FixedSizeIirCoefficientFilter<T, 2 * CHANNELS, 2> lowPass;
            FixedSizeIirCoefficientFilter<T, 2 * CHANNELS, 2> highPass;

            void configure(T sampleRate, T frequency)
            {
                auto wrapLo = lowPass.coefficients_.wrap();
                Butterworth::create(wrapLo, sampleRate, frequency, Butterworth::Pass::LOW, 1.0);
                auto wrapHi = highPass.coefficients_.wrap();
                Butterworth::create(wrapHi, sampleRate, frequency, Butterworth::Pass::HIGH, 1.0);
                lowPass.reset();
                highPass.reset();
            }
        };

        template<typename T, size_t CHANNELS, size_t CROSSOVERS>
        struct CrossoverExecutor
        {

        };

        template<typename T, size_t CHANNELS>
        struct CrossoverExecutor<T, CHANNELS, 1>
        {
            template<typename S, typename... A>
            static void filter(
                    const FixedSizeArrayTraits<S, CHANNELS, A...> &input,
                    FixedSizeArray<S, 2 * CHANNELS> &output,
                    FixedSizeArray<LinkwitzRiley<T, CHANNELS>, 1> &filter)
            {
                for (size_t channel = 0, idx = 0; channel < CHANNELS; channel++, idx += 2) {
                    T in = input[channel];
                    output[channel] =
                            filter[0].lowPass.filter(idx, filter[0].lowPass.filter(idx + 1, in));
                    output[channel + CHANNELS] =
                            filter[0].highPass.filter(idx, filter[0].highPass.filter(idx + 1, in));
                }
            }
        };

        template<typename T, size_t CHANNELS>
        struct CrossoverExecutor<T, CHANNELS, 2>
        {
            template<typename S, typename... A>
            static void filter(
                    const FixedSizeArrayTraits<S, CHANNELS, A...> &input,
                    FixedSizeArray<S, 3 * CHANNELS> &output,
                    FixedSizeArray<LinkwitzRiley<T, CHANNELS>, 2> &filter)
            {
                for (size_t channel = 0, idx = 0; channel < CHANNELS; channel++, idx += 2) {
                    T in = input[channel];
                    T middle =
                            filter[1].lowPass.filter(idx, filter[1].lowPass.filter(idx + 1, in));
                    output[channel + 2 * CHANNELS] =
                            filter[1].highPass.filter(idx, filter[1].highPass.filter(idx + 1, in));
                    output[channel] =
                            filter[0].lowPass.filter(idx, filter[0].lowPass.filter(idx + 1, middle));
                    output[channel + CHANNELS] =
                            filter[0].highPass.filter(idx, filter[0].highPass.filter(idx + 1, middle));
                }
            }
        };

        template<typename T, size_t CHANNELS>
        struct CrossoverExecutor<T, CHANNELS, 3>
        {
            template<typename S, typename... A>
            static void filter(
                    const FixedSizeArrayTraits<S, CHANNELS, A...> &input,
                    FixedSizeArray<S, 4 * CHANNELS> &output,
                    FixedSizeArray<LinkwitzRiley<T, CHANNELS>, 3> &filter)
            {
                for (size_t channel = 0, idx = 0; channel < CHANNELS; channel++, idx += 2) {
                    T in = input[channel];
                    T low =
                            filter[1].lowPass.filter(idx, filter[1].lowPass.filter(idx + 1, in));
                    T high =
                            filter[1].highPass.filter(idx, filter[1].highPass.filter(idx + 1, in));

                    output[channel + 3 * CHANNELS] =
                            filter[2].highPass.filter(idx, filter[2].highPass.filter(idx + 1, high));
                    output[channel + 2 * CHANNELS] =
                            filter[2].lowPass.filter(idx, filter[2].lowPass.filter(idx + 1, high));
                    output[channel + CHANNELS] =
                            filter[0].highPass.filter(idx, filter[0].highPass.filter(idx + 1, low));
                    output[channel] =
                            filter[0].lowPass.filter(idx, filter[0].lowPass.filter(idx + 1, low));
                }
            }
        };

        template<typename T, typename S, size_t CHANNELS, size_t CROSSOVERS>
        class Filter
        {
            static constexpr size_t NODES = CHANNELS * (CROSSOVERS + 1);
            using Executor = CrossoverExecutor<T, CHANNELS, CROSSOVERS>;

            FixedSizeArray<LinkwitzRiley<T, CHANNELS>, CROSSOVERS> filter_;
            FixedSizeArray<S, NODES> output_;

        public:

            template<typename S1, typename S2, typename ...A>
            void configure(S1 sampleRate, const ArrayTraits<S2, A...> &crossovers)
            {
                FixedSizeArray<T, CROSSOVERS> frequencies = validatedCrossoverFrequencies<T, CROSSOVERS, S2, A...>(
                        crossovers);

                for (size_t crossover = 0; crossover < CROSSOVERS; crossover++) {
                    filter_[crossover].configure(sampleRate, frequencies[crossover]);
                }
            }

            template<typename ...A>
            const FixedSizeArray<S, NODES> &filter(const FixedSizeArrayTraits<S, CHANNELS, A...> &input)
            {
                Executor::filter(input, output_, filter_);
                return output_;
            }

        };

        template<typename T, size_t CROSSOVERS, typename... A>
        static const FixedSizeArray<T, 2 * CROSSOVERS + 2>
        weights(const FixedSizeArrayTraits<T, CROSSOVERS, A...> &crossovers, double sampleRate)
        {

            size_t samples = 2 * sampleRate;
            FixedSizeArray<T, 2 * CROSSOVERS + 2> y;
            PinkNoise::Default noise(1.0, sampleRate / 20);
            Filter<T, double, 2, CROSSOVERS> crossover;
            crossover.configure(sampleRate, crossovers);
            ACurves::Filter<T, 1> curves;
            curves.setSampleRate(sampleRate);
            curves.reset();

            // Cut off irrelevant low frequencies
            FixedSizeIirCoefficientFilter<double, 1, 4> cutoffLow;
            auto llCoeffs = cutoffLow.coefficients_.wrap();
            Butterworth::create(llCoeffs, sampleRate, 20.0, Butterworth::Pass::HIGH, 1.0);
            FixedSizeIirCoefficientFilter<double, 1, 4> cutoffHigh;
            auto hhCoeffs = cutoffHigh.coefficients_.wrap();
            Butterworth::create(hhCoeffs, sampleRate, 8000.0, Butterworth::Pass::LOW, 1.0);

            // initialization
            cutoffLow.reset();
            cutoffHigh.reset();
            double unweightedTotal = 0.0;
            for (size_t band = 0; band < y.capacity(); band++) {
                y[band] = 0.0;
            }
            FixedSizeArray<double, 2> filtered;
            for (size_t sample = 0; sample < samples; sample++) {
              double noiseValue = noise();
              T input = cutoffHigh.filter(0, cutoffLow.filter(0, noiseValue));       // bandwidth limited pink noise
                unweightedTotal += input * input;          // unweighted full-range measurement
                filtered[0] = input;// apply keying filter
                filtered[1] = curves.filter(0, input);// apply keying filter
                const FixedSizeArray<double, 2 * CROSSOVERS + 2> &w = crossover.filter(filtered);
                for (size_t band = 0; band < 2 * CROSSOVERS + 2; band++) {
                  const T x = w[band];
                  y[band] += x * x;          // weighted-keyed measurement per band
                }
            }
            for (size_t band = 0; band < 2 * CROSSOVERS + 2; band++) {
              T sqrtRelativeWeight = sqrt(y[band] / unweightedTotal);
              y[band] = sqrtRelativeWeight; // correct with unweighted full-range measurement
            }

            return y;
        };


    };


} /* End of name space tdap */

#endif /* TDAP_CROSSOVER_HEADER_GUARD */
