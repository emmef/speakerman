/*
 * tdap/FixedSizeArray.hpp
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

#ifndef TDAP_FIXEDSIZEARRAY_HEADER_GUARD
#define TDAP_FIXEDSIZEARRAY_HEADER_GUARD

#include <tdap/ArrayTraits.hpp>

namespace tdap {

    template<typename T, size_t SIZE>
    class alignas(Count<T>::align()) FixedSizeArray : public FixedSizeArrayTraits<T, SIZE, FixedSizeArray<T, SIZE>>
    {
        static_assert(std::is_trivially_copyable<T>::value,
                      "Type must be trivial to copy, move or destroy and have standard layout");

        friend class ArrayTraits<T, FixedSizeArray<T, SIZE>>;

        T data_[SIZE];

        size_t _traitGetSize() const
        { return SIZE; }

        size_t _traitGetCapacity() const
        { return SIZE; }

        T &_traitRefAt(size_t i)
        {
            return data_[i];
        }

        const T &_traitRefAt(size_t i) const
        {
            return data_[i];
        }

        T *_traitUnsafeData()
        {
            return data_;
        }

        const T *_traitUnsafeData() const
        {
            return data_;
        }

        T *_traitPlus(size_t i) const
        {
            return data_ + i;
        }

        static constexpr bool _traitHasTrivialAddressing()
        { return true; }

    public:
        template<typename ...A>
        FixedSizeArray(const FixedSizeArrayTraits<T, SIZE, A...> &source)
        {
            copy(source);
        }
//        FixedSizeArray(const T value)
//        {
//            if (value == 0) {
//                zero();
//            }
//            else {
//                for (size_t i = 0; i < SIZE; i++) {
//                    _traitRefAt(i) = value;
//                }
//            }
//        }
        FixedSizeArray() {}

        template<typename ...A>
        void operator =(const FixedSizeArrayTraits<T, SIZE, A...> &source)
        {
            copy(source);
        }
    };

} /* End of name space tdap */

#endif /* TDAP_FIXEDSIZEARRAY_HEADER_GUARD */
