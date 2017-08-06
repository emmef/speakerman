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

template <typename T, size_t SIZE>
class FixedSizeArray : public FixedSizeArrayTraits<T, SIZE, FixedSizeArray<T, SIZE>>
{
	static_assert(TriviallyCopyable<T>::value, "Type must be trivial to copy, move or destroy and have standard layout");

	friend class ArrayTraits<T, FixedSizeArray<T, SIZE>>;
    	typename std::aligned_storage<sizeof(T), alignof(T)>::type data_[SIZE];

	size_t _traitGetSize() const { return SIZE; }
	size_t _traitGetCapacity() const { return SIZE; }

	T& _traitRefAt(size_t i)
	{
		return *reinterpret_cast<T *>(data_ + i);
	}

	const T& _traitRefAt(size_t i) const
	{
		return *reinterpret_cast<const T *>(data_ + i);
	}

	T * _traitUnsafeData()
	{
		return reinterpret_cast<T *>(data_);
	}

	const T * _traitUnsafeData() const
	{
		return reinterpret_cast<const T *>(data_);
	}

	T * _traitPlus(size_t i) const
	{
		return reinterpret_cast<const T *>(data_ + i);
	}

	static constexpr bool _traitHasTrivialAddressing() { return true; }

public:

};

} /* End of name space tdap */

#endif /* TDAP_FIXEDSIZEARRAY_HEADER_GUARD */
