/*
 * tdap/Integration.hpp
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

#ifndef TDAP_INTEGRATION_HEADER_GUARD
#define TDAP_INTEGRATION_HEADER_GUARD

#include <type_traits>
#include <cmath>
#include <limits>

#include <tdap/Count.hpp>
#include <tdap/ValueRange.hpp>

namespace tdap {

    struct Integration
    {

        template<typename F>
        static constexpr F min_samples()
        {
            static_assert(std::is_floating_point<F>::value, "Type parameter F should be floating point");
            return std::numeric_limits<F>::epsilon();
        }

        template<typename F>
        static constexpr F max_samples()
        {
            static_assert(std::is_floating_point<F>::value, "Type parameter F should be floating point");
            return 1.0 / min_samples<F>();
        }

        template<typename F>
        static const ValueRange<F> &range()
        {
            static_assert(std::is_floating_point<F>::value, "Type parameter F should be floating point");
            static const ValueRange<F> range_(min_samples<F>(), max_samples<F>());

            return range_;
        }

        template<typename F>
        static constexpr F limited_samples(F samples)
        {
            static_assert(std::is_floating_point<F>::value, "Type parameter F should be floating point");
            return Value<F>::force_between(samples, min_samples<F>(), max_samples<F>());
        }

        template<typename F>
        static F checked_samples(F samples)
        {
            static_assert(std::is_floating_point<F>::value, "Type parameter F should be floating point");
            return range<F>().getValid(samples);
        }

        template<typename F>
        static constexpr F get_unchecked_history_multiplier(F samples)
        {
            static_assert(std::is_floating_point<F>::value, "Type parameter F should be floating point");
            return exp(-1.0 / samples);
        }

        template<typename F>
        static constexpr F get_history_multiplier(F samples)
        {
            static_assert(std::is_floating_point<F>::value, "Type parameter F should be floating point");
            return samples < min_samples<F>()
                   ? 0.0
                   : get_unchecked_history_multiplier(
                            samples > max_samples<F>() ? max_samples<F>() : samples);
        }

        template<typename F>
        static constexpr F get_history_multiplier_limited(F samples)
        {
            static_assert(std::is_floating_point<F>::value, "Type parameter F should be floating point");
            return get_unchecked_history_multiplier(limited_samples(samples));
        }

        template<typename F>
        static F get_history_multiplier_checked(F samples)
        {
            static_assert(std::is_floating_point<F>::value, "Type parameter F should be floating point");
            return get_unchecked_history_multiplier(checked_samples(samples));
        }

        template<typename F>
        static constexpr F get_other_multiplier(F history_multiplier)
        {
            static_assert(std::is_floating_point<F>::value, "Type parameter F should be floating point");
            return 1.0 - history_multiplier;
        }

        template<typename F>
        static constexpr F get_input_multiplier(F samples)
        {
            static_assert(std::is_floating_point<F>::value, "Type parameter F should be floating point");
            return samples < min_samples<F>()
                   ? 1.0
                   : get_other_multiplier(get_unchecked_history_multiplier(
                            samples > max_samples<F>() ? max_samples<F>() : samples));
        }

        template<typename F>
        static constexpr F get_input_multiplier_limited(F samples)
        {
            static_assert(std::is_floating_point<F>::value, "Type parameter F should be floating point");
            return get_other_multiplier(get_unchecked_history_multiplier(limited_samples(samples)));
        }

        template<typename F>
        static F get_input_multiplier_checked(F samples)
        {
            static_assert(std::is_floating_point<F>::value, "Type parameter F should be floating point");
            return get_other_multiplier(get_unchecked_history_multiplier(checked_samples(samples)));
        }

        template<typename F>
        static constexpr F get_samples_from_history_multiply(double history_multiply)
        {
            static_assert(std::is_floating_point<F>::value, "Type parameter F should be floating point");
            return -1.0 / log(history_multiply);
        }

        template<typename F>
        static F constexpr get_samples_from_input_multiply(double input_multiply)
        {
            static_assert(std::is_floating_point<F>::value, "Type parameter F should be floating point");
            return -1.0 / log(1.0 - input_multiply);
        }

        template<typename F, typename S>
        static constexpr F integrate(F history_multiply, F input_multiply, S input, F history)
        {
            static_assert(std::is_floating_point<F>::value, "Type parameter F should be floating point");
            return input_multiply * input + history_multiply * history;
        }

        template<typename F, typename S>
        static constexpr F integrate(F history_multiply, S input, F history)
        {
            static_assert(std::is_floating_point<F>::value, "Type parameter F should be floating point");
            return (1.0 - history_multiply) * input + history_multiply * history;
        }

        template<typename F>
        static F valid_samples(double sampleRate, double seconds)
        {
            double samples = Value<double>::valid_positive(sampleRate) * Value<double>::valid_positive(seconds);
            if (samples < max_samples<F>() && samples < Count<char>::max()) {
                return samples;
            }
            throw std::invalid_argument(
                    "Average:: Combination of sampleRate_ and seconds yields too large sample count");
        }
    };

    template<typename Coefficient>
    class IntegrationCoefficients
    {
        static_assert(std::is_floating_point<Coefficient>::value, "Coefficient must be a floating-point type");

        Coefficient historyMultiply = 0;
        Coefficient inputMultiply = 1.0;
    public:
        IntegrationCoefficients()
        {}

        IntegrationCoefficients(double characteristicSamples) :
                historyMultiply(Integration::get_history_multiplier(characteristicSamples)),
                inputMultiply(1.0 - historyMultiply)
        {}

        IntegrationCoefficients(double sampleRate, double seconds) :
                IntegrationCoefficients(Integration::valid_samples<double>(sampleRate, seconds))
        {}

        Coefficient historyMultiplier() const
        { return historyMultiply; }

        Coefficient inputMultiplier() const
        { return inputMultiply; }

        void setCharacteristicSamples(double value)
        {
            historyMultiply = Integration::get_history_multiplier(value);
            inputMultiply = 1.0 - historyMultiply;
        }

        void setTimeAndRate(double seconds, double sampleRate)
        {
            setCharacteristicSamples(Integration::valid_samples<double>(sampleRate, seconds));
        }

        double getCharacteristicSamples() const
        {
            return historyMultiplyInverse(historyMultiply);
        }

        template<typename Value>
        Value getIntegrated(const Value input, const Value previousOutput) const
        {
            static_assert(std::is_floating_point<Value>::value,
                          "Value must be a floating-point type (stability condition)");
            return Integration::integrate(historyMultiply, inputMultiply, input, previousOutput);
        }

        template<typename Value>
        Value integrate(const Value input, Value &output) const
        {
            return (output = getIntegrated(input, output));
        }
    };

    template<typename Coefficient>
    struct IntegratorFilter
    {
        static_assert(std::is_floating_point<Coefficient>::value, "Coefficient must be a floating-point type");

        IntegrationCoefficients<Coefficient> coefficients;
        Coefficient output;

        template<typename Value>
        Value getIntegrated(Value input, Value previousOutput) const
        {
            return coefficients.getIntegrated(input, previousOutput);
        }

        template<typename Value>
        Value integrate(Value input)
        {
            return coefficients.integrate(input, output);
        }

        template<typename Value>
        void setOutput(Value newOutput)
        {
            output = newOutput;
        }
    };

    template<typename Coefficient>
    struct AttackReleaseFilter
    {
        static_assert(std::is_floating_point<Coefficient>::value, "Coefficient must be a floating-point type");

        IntegrationCoefficients<Coefficient> attackCoeffs;
        IntegrationCoefficients<Coefficient> releaseCoeffs;
        Coefficient output;

        AttackReleaseFilter(Coefficient attackSamples, Coefficient releaseSamples, Coefficient initialOutput = 0.0) :
                attackCoeffs(attackSamples), releaseCoeffs(releaseSamples), output(initialOutput)
        {}

        template<typename Value>
        Value integrate(const Value input)
        {
            return input > output ? attackCoeffs.integrate(input, output) : releaseCoeffs.integrate(input, output);
        }

        template<typename Value>
        void setOutput(Value newOutput)
        {
            output = newOutput;
        }
    };

    template<typename Coefficient>
    struct AttackReleaseSmoothFilter
    {
        static_assert(std::is_floating_point<Coefficient>::value, "Coefficient must be a floating-point type");

        AttackReleaseFilter<Coefficient> filter;
        Coefficient output;

        AttackReleaseSmoothFilter(Coefficient attackSamples, Coefficient releaseSamples,
                                  Coefficient initialOutput = 0.0) :
                filter(attackSamples, releaseSamples, initialOutput), output(initialOutput)
        {}


        template<typename Value>
        Value integrate(const Value input)
        {
            return filter.attackCoeffs.integrate(filter.integrate(input), output);
        }

        template<typename Value>
        void setOutput(Value newOutput)
        {
            filter.setOutput(newOutput);
            output = newOutput;
        }
    };


} /* End of name space tdap */

#endif /* TDAP_INTEGRATION_HEADER_GUARD */
