/*
 * tdap/VolumeMatrix.hpp
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

#ifndef TDAP_VALUE_VOLUME_MATRIX_GUARD
#define TDAP_VALUE_VOLUME_MATRIX_GUARD

#include <tdap/Integration.hpp>
#include <tdap/IndexPolicy.hpp>
#include <tdap/ArrayTraits.hpp>

namespace tdap {

    template<typename T, size_t CHANNELS>
    class VolumeMatrix
    {
        static_assert(std::is_floating_point<T>::value, "Expecting floating point type parameter");
        static_assert(Count<T>::valid_positive(CHANNELS), "Invalid CHANNELS parameter");

        T volume_[CHANNELS][CHANNELS];

    public:
        VolumeMatrix(T value)
        {
            setAll(value);
        }

        VolumeMatrix() : VolumeMatrix(0)
        {}

        T validVolume(T volume)
        {
            if (volume >= -1e-5 && volume <= 1e-5) {
                return 0;
            }
            return Values::force_between(volume, -10.0, 10.0);
        }

        VolumeMatrix &set(size_t output, size_t input, T volume)
        {
            volume_[IndexPolicy::array(output, CHANNELS)][IndexPolicy::array(input, CHANNELS)] =
                    validVolume(volume);
            return *this;
        }

        VolumeMatrix &setAll(T volume)
        {
            T v = validVolume(volume);

            for (size_t i = 0; i < CHANNELS; i++) {
                for (size_t j = 0; i < CHANNELS; i++) {
                    volume_[i][j] = v;
                }
            }
            return *this;
        }

        VolumeMatrix &setGroup(size_t output, size_t input, T volume, size_t CHANNELS_PER_GROUP)
        {
            T v = validVolume(volume);

            size_t offset = output * CHANNELS_PER_GROUP;
            size_t endOffset = offset + CHANNELS_PER_GROUP;
            for (; offset < endOffset; offset++) {
                volume_[offset][offset] = v;
            }
            return *this;
        }

        void approach(const VolumeMatrix &source, const IntegrationCoefficients<T> &coefficients)
        {
            for (size_t i = 0; i < CHANNELS; i++) {
                for (size_t j = 0; j < CHANNELS; j++) {
                    coefficients.integrate(source.volume_[i][j], volume_[i][j]);
                }
            }
        }

        template<typename S, typename...A>
        void apply(const ArrayTraits<S, A...> &input, ArrayTraits<S, A...> &output, T noise)
        {
            for (size_t o = 0; o < CHANNELS; o++) {
                T sum = noise;
                for (size_t i = 0; i < CHANNELS; i++) {
                    sum += volume_[o][i] * input[i];
                }
                output[o] = sum;
            }
        }
    };

    template<typename T, size_t CHANNELS_PER_GROUP, size_t GROUPS>
    struct VolumeControl
    {
        static constexpr size_t CHANNELS = GROUPS * CHANNELS_PER_GROUP;

        using Matrix = VolumeMatrix<T, CHANNELS>;
        IntegrationCoefficients<double> integration;
        Matrix userVolume;
        Matrix actualVolume;

        VolumeControl()
        {
            userVolume.setAll(0);
            actualVolume.setAll(0);
            integration.setCharacteristicSamples(96000 * 0.05);
        }

        void configure(double sampleRate, double rc, Matrix initialVolumes)
        {
            integration.setCharacteristicSamples(sampleRate * rc);
            userVolume = initialVolumes;
            actualVolume.setAll(0);
        }

        void setVolume(Matrix newVolumes)
        {
            userVolume = newVolumes;
        }

        template<typename...A>
        void apply(const ArrayTraits<double, A...> &input, ArrayTraits<double, A...> &output, double noise)
        {
            actualVolume.approach(userVolume, integration);
            actualVolume.apply(input, output, noise);
        }
    };


} /* End of name space tdap */

#endif /* TDAP_VALUE_VOLUME_MATRIX_GUARD */
