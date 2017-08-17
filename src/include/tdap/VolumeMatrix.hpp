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
#include <tdap/Samples.hpp>
#include <tdap/IndexPolicy.hpp>

namespace tdap {

    template<typename T, size_t OUTPUTS, size_t INPUTS, size_t ALIGN = Count<T>::align()>
    class VolumeMatrix : protected SampleMatrix<T, OUTPUTS, INPUTS, ALIGN>
    {
        static_assert(std::is_floating_point<T>::value, "Expecting floating point type parameter");
        static_assert(Count<T>::valid_positive(INPUTS), "Invalid INPUTS parameter");
        static_assert(Count<T>::valid_positive(OUTPUTS), "Invalid OUTPUTS parameter");
        static constexpr size_t MAX_CHANNELS = std::max(OUTPUTS, INPUTS);
        static constexpr size_t MIN_CHANNELS = std::min(OUTPUTS, INPUTS);
        using SampleMatrix<T, OUTPUTS, INPUTS, ALIGN>::operator[];

    public:
        VolumeMatrix(T value)
        {
            set_all(value);
        }

        VolumeMatrix()
        {
            identity();
        }
        static T validVolume(T volume)
        {
            if (volume >= -1e-5 && volume <= 1e-5) {
                return 0;
            }
            return Values::force_between(volume, -10.0, 10.0);
        }

        void set(size_t output, size_t input, T volume)
        {
            int x = this->operator[](output);
        }

        void set_all(T volume)
        {
            set_all(volume);
        }
        void set_default(T scale = 1.0)
        {
            set_all(0);
            for (size_t i = 0; i < MIN_CHANNELS; i++) {
                (*this)[i][i] = scale;
            }
        }
        void set_default_wrapped(T scale = 1.0)
        {
            set_all(0);
            for (size_t i = 0; i < MAX_CHANNELS; i++) {
                (*this)[i % OUTPUTS][i % INPUTS] = scale;
            }
        }
        void approach(const VolumeMatrix &source, const IntegrationCoefficients<T> &coefficients)
        {
            for (size_t i = 0; i < OUTPUTS; i++) {
                for (size_t j = 0; j < INPUTS; j++) {
                    coefficients.integrate(source[i][j], (*this)[i][j]);
                }
            }
        }
        template <size_t AL1, size_t AL2>
        void apply(Samples<T, OUTPUTS, AL1> &output, const Samples<T, INPUTS, AL2>&input) const
        {
            multiply_in(output, input);
        }
        template <size_t AL1, size_t AL2>
        void apply_with_noise(Samples<T, OUTPUTS, AL1> &output, const Samples<T, INPUTS, AL2>&input, T noise) const
        {
            multiply_in(output, input);
            for (size_t out = 0; out < OUTPUTS; out++) {
                output[out] += noise;
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
            userVolume.set_all(0);
            actualVolume.set_all(0);
            integration.setCharacteristicSamples(96000 * 0.05);
        }

        void configure(double sampleRate, double rc, Matrix initialVolumes)
        {
            integration.setCharacteristicSamples(sampleRate * rc);
            userVolume = initialVolumes;
            actualVolume.set_all(0);
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
