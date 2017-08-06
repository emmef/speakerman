/*
 * tdap/ArrayTraits.hpp
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

#ifndef TDAP_ARRAYTRAITS_HEADER_GUARD
#define TDAP_ARRAYTRAITS_HEADER_GUARD

#include <cstring>

#include <tdap/Count.hpp>
#include <tdap/Value.hpp>
#include <tdap/IndexPolicy.hpp>

namespace tdap {

template<typename T, class Sub>
struct ArrayTraits
{
	size_t size() const
	{
		return static_cast<const Sub *>(this)->_traitGetSize();
	}

	size_t capacity() const
	{
		return static_cast<const Sub *>(this)->_traitGetCapacity();
	}

	size_t validSize(size_t size)
	{
		if (size <= capacity()) {
			return size;
		}
		throw std::invalid_argument("Array: invalid size ");
	}

	size_t elementSize() const
	{
		return sizeof(T);
	}

	T& operator [] (size_t i)
	{
		return static_cast<Sub *>(this)->_traitRefAt(IndexPolicy::array(i, size()));
	}

	const T& operator [] (size_t i) const
	{
		return static_cast<const Sub *>(this)->_traitRefAt(IndexPolicy::array(i, size()));
	}

	inline T* operator + (size_t i) const
	{
		return static_cast<const Sub *>(this)->_traitPlus(IndexPolicy::array(i, size()));
	}

	T& ref(size_t i)
	{
		return static_cast<Sub *>(this)->_traitRefAt(IndexPolicy::method(i, size()));
	}

	const T& get(size_t i) const
	{
		return static_cast<const Sub *>(this)->_traitRefAt(IndexPolicy::method(i, size()));
	}

	inline T& refUnchecked(size_t i)
	{
		return static_cast<Sub *>(this)->_traitRefAt(i);
	}

	inline const T& getUnchecked(size_t i) const
	{
		return static_cast<const Sub *>(this)->_traitRefAt(i);
	}

	inline T& refChecked(size_t i)
	{
		return static_cast<Sub *>(this)->_traitRefAt(IndexPolicy::force(i, size()));
	}

	inline const T& getCchecked(size_t i) const
	{
		return static_cast<const Sub *>(this)->_traitRefAt(IndexPolicy::force(i, size()));
	}

	inline T* offset(size_t i) const
	{
		return static_cast<const Sub *>(this)->_traitPlus(IndexPolicy::method(i, size()));
	}

	static constexpr bool hasTrivialAddressing()
	{
		return Sub::_traitHasTrivialAddressing();
	}

	template<typename ...A>
	void copy(size_t offset, const ArrayTraits<T, A...> &source, size_t sourceOffset, size_t length)
	{
		size_t end = traitCheckOffsetParamsReturnEndOffset(
				offset, source, sourceOffset, length);

		if (	TriviallyCopyable<T>::value &&
				hasTrivialAddressing() &&
				source.hasTrivialAddressing())
		{
			const void * src = static_cast<const void *>(source.unsafeData() + sourceOffset);
			void * dst = static_cast<void *>(unsafeData() + offset);
			std::memmove(dst, src, sizeof(T) * length);
		}
		else {
			for (size_t src = sourceOffset, dst = offset; dst < end; dst++, src++) {
				operator[] (dst) = source.operator [](src);
			}
		}
	}

	template<typename ...A>
	void move(size_t offset, ArrayTraits<T, A...> &source, size_t sourceOffset, size_t length)
	{
		size_t end = traitCheckOffsetParamsReturnEndOffset(
				offset, source, sourceOffset, length);

		if (	TriviallyCopyable<T>::value &&
				hasTrivialAddressing() &&
				source.hasTrivialAddressing())
		{
			// Move constructors not necessary on these type of objects
			void * src = static_cast<void *>(source.unsafeData() + sourceOffset);
			void * dst = static_cast<void *>(unsafeData() + offset);
			std::memmove(dst, src, sizeof(T) * length);
		}
		else {
			for (size_t src = sourceOffset, dst = offset; dst < end; dst++, src++) {
				operator[] (dst) = std::move(source.operator [](src));
			}
		}
	}

	template<typename ...A>
	void copy(const ArrayTraits<T, A...> &source)
	{
		if (source.size() != size()) {
			throw std::invalid_argument("ArrayTraits::copy(): source has different size");
		}
		if (TriviallyCopyable<T>::value && hasTrivialAddressing() && source.hasTrivialAddressing()) {
			const void * src = static_cast<const void *>(source.unsafeData());
			void * dst = static_cast<void *>(unsafeData());
			std::memmove(dst, src, sizeof(T) * size());
		}
		else {
			for (size_t i = 0; i < size(); i++) {
				operator[] (i) = source.operator [](i);
			}
		}
	}

	template<typename ...A>
	void move(ArrayTraits<T, A...> &source)
	{
		if (source.size() != size()) {
			throw std::invalid_argument("ArrayTraits::copy(): source has different size");
		}

		if (TriviallyCopyable<T>::value && hasTrivialAddressing() && source.traitHasTrivialLayout()) {
			const void * src = static_cast<const void *>(source.unsafeData());
			void * dst = static_cast<void *>(unsafeData());
			std::memmove(dst, src, sizeof(T) * size());
		}
		else {
			for (size_t i = 0; i < size(); i++) {
				operator[] (i) = std::move(source.operator [](i));
			}
		}
	}

	const T * const unsafeData() const
	{
		if (hasTrivialAddressing()) {
			return static_cast<const Sub *>(this)->_traitUnsafeData();
		}
		else {
			throw std::logic_error("ArrayTraits: Cannot return address of array with non-trivial addressing");
		}
	}

	T * unsafeData()
	{
		if (hasTrivialAddressing()) {
			return static_cast<Sub *>(this)->_traitUnsafeData();
		}
		else {
			throw std::logic_error("ArrayTraits: Cannot return address of array with non-trivial addressing");
		}
	}

	void zero()
	{
		static_assert(
				std::is_scalar<T>::value,
				"Type-parameter must be a scalar");

		if (!hasTrivialAddressing()) {
			for (size_t i = 0; i < size(); i++) {
				operator[](i) = static_cast<T>(0);
			}
		}
		else {
			std::memset(unsafeData(), 0, sizeof(T) * size());
		}
	}

private:
	template<typename A>
	size_t traitCheckOffsetParamsReturnEndOffset(
			size_t offset, const ArrayTraits<T, A> &source,
			size_t sourceOffset, size_t length)
	{
		if (!Count<T>::is_valid_sum(offset, length)) {
			throw std::invalid_argument("ArrayTraits::copy(): offset and length too big (numeric)");
		}
		size_t end = offset + length;
		if (end > size()) {
			throw std::invalid_argument("ArrayTraits::copy(): offset and length too big (size)");
		}
		if (!Count<T>::is_valid_sum(sourceOffset, length) || sourceOffset + length > source.size()) {
			throw std::invalid_argument("ArrayTraits::copy(): source offset and length too big");
		}
		return end;
	}
};

	template<typename T, size_t SIZE, class Sub>
	struct FixedSizeArrayTraits : public ArrayTraits<T, Sub>
	{
		constexpr size_t size() const
		{
			return SIZE;
		}
	};

	template<typename T, size_t CAPACITY, class Sub>
	struct FixedCapArrayTraits : public ArrayTraits<T, Sub>
	{
		void setSize(size_t newSize)
		{
			static_cast<Sub *>(this)->_traitSetSize(validSize(newSize));
		}

		static constexpr size_t capacity()
		{
			return CAPACITY;
		}

		static size_t validSize(size_t size)
		{
			if (size <= capacity()) {
				return size;
			}
			throw std::invalid_argument("Array: invalid size ");
		}

	};

enum class ConstructionPolicy
{
	SIZE_BECOMES_CAPACITY,
	INHERIT_CAPACITY
};


} /* End of name space tdap */

#endif /* TDAP_ARRAYTRAITS_HEADER_GUARD */
