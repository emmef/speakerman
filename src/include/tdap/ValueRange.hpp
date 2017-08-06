/*
 * tdap/ValueRange.hpp
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

#ifndef TDAP_VALUERANGE_HEADER_GUARD
#define TDAP_VALUERANGE_HEADER_GUARD

#include <tdap/Value.hpp>

namespace tdap {

    template<typename T>
    class ValueRange
    {
        static_assert(std::is_arithmetic<T>::value, "Value type T must be arithmetic");

        ValueRange() :
                superRange_(*this),
                min_(std::numeric_limits<T>::lowest()),
                max_(std::numeric_limits<T>::max())
        {}

        const ValueRange<T> &superRange_;
        const T min_;
        const T max_;

    public:
        static const ValueRange<T> &absolute()
        {
            static ValueRange range;
            return range;
        }

        ValueRange(const ValueRange &superRange, T min, T max) :
                superRange_(superRange),
                min_(superRange_.getStartIfValid(min, max)),
                max_(max)
        {}

        ValueRange(T min, T max) : ValueRange(absolute(), min, max)
        {}

        const ValueRange<T> &superRange() const
        {
            return superRange_;
        }

        T getMinimum() const
        {
            return min_;
        }

        T getMaximum() const
        {
            return max_;
        }

        T getBetween(T value) const
        {
            return Value<T>::force_between(value, min_, max_);
        }

        bool isBetween(T value) const
        {
            return value >= min_ && value <= max_;
        }

        bool isSubRange(T start, T end) const
        {
            return start < end && start >= min_ && end <= max_;
        }

        T getValid(T value) const
        {
            if (isBetween(value)) {
                return value;
            }
            throw std::invalid_argument("ValueRange: time not within range");
        }

        T getStartIfValid(T start, T end) const
        {
            if (isSubRange(start, end)) {
                return start;
            }
            throw std::invalid_argument("ValueRange::getStartIfValid(): invalid range");
        }

        T getEndIfValid(T start, T end) const
        {
            if (isSubRange(start, end)) {
                return start;
            }
            throw std::invalid_argument("ValueRange::getStartIfValid(): invalid range");
        }
    };

} /* End of name space tdap */

#endif /* TDAP_VALUERANGE_HEADER_GUARD */
