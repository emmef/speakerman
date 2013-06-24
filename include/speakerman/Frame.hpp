/*
 * Frame.hpp
 *
 * Part of 'Speaker management system'
 *
 * Copyright (C) 2013 Michel Fleur.
 * https://github.com/emmef/simpledsp
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

#ifndef SMS_SPEAKERMAN_FRAME_GUARD_H_
#define SMS_SPEAKERMAN_FRAME_GUARD_H_

#include <stdexcept>
#include <simpledsp/Move.hpp>
#include <simpledsp/Precondition.hpp>
#include <simpledsp/Values.hpp>

namespace speakerman {

using namespace simpledsp;

template<typename Sample> class Frame
{
	size_t size_ = 0;
	Sample *x_ = nullptr;

protected:
	Frame()
	{
	}

	void init(size_t size, Sample *x)
	{
		x_ = x;
		size_ = size;
	}

public:
	Frame(Sample *x, size_t size) :
		x_(x), size_(Precondition::validPositiveCount<Sample>(size))
	{
	}

	inline Sample &operator[] (size_t i) const
	{
		if (i < size_) {
			return x_[i];
		}
		throw std::invalid_argument("Vector Index out of bound");
	}

	inline size_t size() const
	{
		return size_;
	}

	inline Sample * unsafe() const
	{
		return x_;
	}

	inline void clear()
	{
		Move::unsafeZero<Sample>(x_, size_);
	}

	inline void copy(const Frame<Sample> & source)
	{
		Move::unsafeCopy<Sample>(source.x_, source.size_, x_, size_, CopyZero::ZERO);
	}

	inline void add(const Frame<Sample> &source)
	{
		size_t moves = size_ < source.size_ ? size_ : source.size_;

		for (size_t i = 0; i < moves; i++) {
			x_[i] += source.x_[i];
		}
	}

	inline void subtract(const Frame<Sample> &source)
	{
		size_t moves = size_ < source.size_ ? size_ : source.size_;

		for (size_t i = 0; i < moves; i++) {
			x_[i] -= source.x_[i];
		}
	}
};

template<typename Sample> class VariableFrame : public Frame<Sample>
{
public:
	VariableFrame() {}

	void init(size_t size, Sample *x)
	{
		Frame<Sample>::init(size, x);
	}
};


template<typename Sample> class FixedFrame : public Frame<Sample>
{
public:
	FixedFrame(size_t size)
	{
		Frame<Sample>::init(size, new Sample[Precondition::validPositiveCount<Sample>(size)]);
	}

	~FixedFrame() {
		delete[] Frame<Sample>::unsafe();
	}
};

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_FRAME_GUARD_H_ */
