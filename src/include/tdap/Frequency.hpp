/*
 * tdap/Frequency.hpp
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

#ifndef TDAP_FREQUENCY_HEADER_GUARD
#define TDAP_FREQUENCY_HEADER_GUARD

#include <tdap/Value.hpp>

#include <math.h>
#include <string>
#include <limits>
#include <type_traits>
#include <stdexcept>

namespace tdap {
    namespace helper {

        template<typename F, typename R>
        struct FrequencyTraits
        {
            static_assert(std::is_arithmetic<F>::value, "Frequency type must be arithmetic");
            static_assert(std::is_floating_point<R>::value, "Return type must be floating point");

            static inline constexpr bool isValid(F frequency)
            {
                return frequency > 0;
            }

            static inline constexpr bool isPositive(F frequency)
            {
                return frequency > Value<F>::minimumPositive();
            }

            static inline constexpr R
            nycquist(F sampleRate)
            {
                return static_cast<R>(0.5) * sampleRate;
            }

            static inline constexpr R
            relative(F frequency, F sampleRate)
            {
                return static_cast<R>(frequency) / sampleRate;
            }

            static inline constexpr R
            relativeBetween(F frequency, F sampleRate, R minRelative, R maxRelative)
            {
                return Value<R>::force_between(relative(frequency, sampleRate), minRelative, maxRelative);
            }

            static inline constexpr R
            relativeNycquistLimited(F frequency, F sampleRate)
            {
                return Value<R>::force_between(relative(frequency, sampleRate), std::numeric_limits<R>::min(), 0.5);
            }

            static inline constexpr R
            period(F frequency)
            {
                return static_cast<R>(1.0) / frequency;
            }

            static inline constexpr R
            angularSpeed(F frequency)
            {
                return static_cast<R>(M_PI * 2) * frequency;
            }

            static inline constexpr R
            angularPeriod(F frequency)
            {
                return static_cast<R>(1.0) / angularSpeed(frequency);
            }
        };

        template<typename F, bool isFloat>
        struct FrequencyHelper
        {
        };

        template<typename F>
        struct FrequencyHelper<F, true> : public FrequencyTraits<F, F>
        {
            static_assert(std::is_floating_point<F>::value,
                          "Frequency type must be floating point for this specialization");

            using FrequencyTraits<F, F>::isValid;
            using FrequencyTraits<F, F>::isPositive;
            using FrequencyTraits<F, F>::nycquist;
            using FrequencyTraits<F, F>::relative;
            using FrequencyTraits<F, F>::relativeBetween;
            using FrequencyTraits<F, F>::relativeNycquistLimited;
            using FrequencyTraits<F, F>::period;
            using FrequencyTraits<F, F>::angularSpeed;
            using FrequencyTraits<F, F>::angularPeriod;
        };

        template<typename F>
        struct FrequencyHelper<F, false> : public FrequencyTraits<F, double>
        {
            static_assert(std::is_arithmetic<F>::value, "Frequency type must be arithmetic type");

            using FrequencyTraits<F, double>::isValid;
            using FrequencyTraits<F, double>::isPositive;
            using FrequencyTraits<F, double>::nycquist;
            using FrequencyTraits<F, double>::relative;
            using FrequencyTraits<F, double>::relativeBetween;
            using FrequencyTraits<F, double>::relativeNycquistLimited;
            using FrequencyTraits<F, double>::period;
            using FrequencyTraits<F, double>::angularSpeed;
            using FrequencyTraits<F, double>::angularPeriod;
        };

    } /* End of namespace helpers */

    template<typename F>
    struct Frequency : public helper::FrequencyHelper<F, std::is_floating_point<F>::value>
    {
        using helper::FrequencyHelper<F, std::is_floating_point<F>::value>::isValid;
        using helper::FrequencyHelper<F, std::is_floating_point<F>::value>::isPositive;
        using helper::FrequencyHelper<F, std::is_floating_point<F>::value>::nycquist;
        using helper::FrequencyHelper<F, std::is_floating_point<F>::value>::relative;
        using helper::FrequencyHelper<F, std::is_floating_point<F>::value>::relativeBetween;
        using helper::FrequencyHelper<F, std::is_floating_point<F>::value>::relativeNycquistLimited;
        using helper::FrequencyHelper<F, std::is_floating_point<F>::value>::period;
        using helper::FrequencyHelper<F, std::is_floating_point<F>::value>::angularSpeed;
        using helper::FrequencyHelper<F, std::is_floating_point<F>::value>::angularPeriod;

        static F checkPositive(F frequency, const char *name)
        {
            if (!isPositive(frequency)) {
                std::string message;
                message.append(name != nullptr ? name : "Frequency").append(" must be positive");
                throw std::invalid_argument(message);
            }
            return frequency;
        }
    };

} /* End of name space tdap */

#endif /* TDAP_FREQUENCY_HEADER_GUARD */
