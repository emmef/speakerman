/*
 * tdap/Array.hpp
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

#ifndef TDAP_ARRAY_HEADER_GUARD
#define TDAP_ARRAY_HEADER_GUARD

#include <tdap/ArrayTraits.hpp>
#include "debug.hpp"

namespace tdap {

TDAP_DEBUG_DEF_COUNT(ArrayInitMaxSizeAndSize)
TDAP_DEBUG_DEF_COUNT(ArrayInitMaxSize)
TDAP_DEBUG_DEF_COUNT(ArrayInitCopy)
TDAP_DEBUG_DEF_COUNT(ArrayInitMove)
TDAP_DEBUG_DEF_COUNT(ArrayInitRefMove)
TDAP_DEBUG_DEF_COUNT(ArrayDestructor)
TDAP_DEBUG_DEF_COUNT(ArrayRef)
TDAP_DEBUG_DEF_COUNT(ArrayRead)
TDAP_DEBUG_DEF_COUNT(ArrayUnsafeRef)
TDAP_DEBUG_DEF_COUNT(ArrayUnsafeRead)
TDAP_DEBUG_DEF_COUNT(ArrayPlus)
TDAP_DEBUG_DEF_COUNT(ArrayGetSize)

template <typename T>
class RefArray;

template <typename T>
class Array : public ArrayTraits<T, Array<T>>
{
	static_assert(TriviallyCopyable<T>::value, "Type must be trivial to copy, move or destroy and have standard layout");
	friend class ArrayTraits<T, Array<T>>;
	using Parent = ArrayTraits<T, Array<T>>;
        using Data = typename std::aligned_storage<sizeof(T), alignof(T)>::type;

	size_t capacity_;
	size_t size_;
    	Data *data_;

	static size_t validCapacity(size_t size)
	{
		if (Count<T>::valid_positive(size)) {
			return size;
		}
		throw std::invalid_argument("Array: invalid capacity");
	}

	size_t _traitGetSize() const { return size_; }
	size_t _traitGetCapacity() const { return capacity_; }

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
		return reinterpret_cast<T *>(data_ + 1);
	}

	void _traitSetSize(size_t newSize)
	{
		size_ = newSize;
	}

	static constexpr bool _traitHasTrivialAddressing() { return true; }

	static T * const nonNull(T * const ptr)
	{
		if (ptr != nullptr) {
			return ptr;
		}
		throw new std::invalid_argument("Array Cannot reference null");
	}

public:
    using ArrayTraits<T, Array<T>>::copy;
    using ArrayTraits<T, Array<T>>::validSize;

	Array(size_t capacity) : capacity_(validCapacity(capacity)), size_(capacity_), data_(new Data[capacity_]) { }
	Array(size_t capacity, size_t size) : capacity_(validCapacity(capacity)), size_(Parent::validSize(size)), data_(new Data[capacity_]) { }

	Array(Array<T> &&source) : capacity_(source.capacity_), size_(source.size_), data_(source.data_)
	{
		source.capacity_ = 0;
		source.size_ = 0;
		source.data_ = nullptr;
	}

	template<typename ...A>
	Array(const ArrayTraits<T, A...> &source) : capacity_(source.size()), size_(capacity_), data_(new Data[capacity_])
	{
		copy(source);
	}

	Array(const Array<T> &source, ConstructionPolicy policy) :
		capacity_(policy == ConstructionPolicy::INHERIT_CAPACITY ? source.capacity_: source.size_), size_(source.size_), data_(new Data[capacity_])
	{
		copy(source);
	}

	Array(const Array<T> &source) : Array(source, ConstructionPolicy::SIZE_BECOMES_CAPACITY) {}

	void setSize(size_t newSize)
	{
		size_ = validSize(newSize);
	}

	template<typename ...A>
	void operator = (const ArrayTraits<T, A...> &source)
	{
		copy(source);
	}

	void operator = (const Array<T> &source)
	{
		copy(source);
	}

	~Array()
	{
		if (data_) {
			delete[] data_;
			data_ = nullptr;
		}
		size_ = 0;
		capacity_ = 0;
	}
};

template <typename T>
class RefArray : public ArrayTraits<T, RefArray<T>>
{
	static_assert(TriviallyCopyable<T>::value, "Type must be trivial to copy, move or destroy and have standard layout");
	friend class ArrayTraits<T, RefArray<T>>;

	size_t size_ = 0;
	T * data_ = 0;

	static size_t validSize(size_t size)
	{
		if (Count<T>::valid_positive(size)) {
			return size;
		}
		throw std::invalid_argument("RefArray: size too big");
	}

	size_t _traitGetSize() const { return size_; }
	size_t _traitGetCapacity() const { return size_; }

	T& _traitRefAt(size_t i)
	{
		return data_[i];
	}

	const T& _traitRefAt(size_t i) const
	{
		return data_[i];
	}

	T * _traitUnsafeData()
	{
		return &data_[0];
	}

	const T * _traitUnsafeData() const
	{
		return &data_[0];
	}

	T * _traitPlus(size_t i) const
	{
		return data_ + i;
	}

	static constexpr bool _traitHasTrivialAddressing() { return true; }

	static T * const nonNull(T * const ptr)
	{
		if (ptr != nullptr) {
			return ptr;
		}
		throw new std::invalid_argument("RefArray Cannot reference null");
	}

public:
	RefArray() = default;
	RefArray(T * const data, size_t size) : size_(validSize(size)), data_(nonNull(data)) { }

	void reset()
	{
		size_ = 0;
		data_ = 0;
	}
};


} /* End of name space tdap */

#endif /* TDAP_ARRAY_HEADER_GUARD */
