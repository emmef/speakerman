/*
 * tdap/Delay.hpp
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

#ifndef TDAP_DELAY_HEADER_GUARD
#define TDAP_DELAY_HEADER_GUARD

#include <tdap/Power2.hpp>
#include <tdap/Array.hpp>

namespace tdap {
using namespace std;

template<typename S>
class Delay
{
	static_assert(is_floating_point<S>::value, "Expected floating-point type parameter");

	Array<S> buffer_;
	size_t mask_;
	size_t read_;
	size_t write_;

	static size_t validMaxDelay(size_t maxDelay)
	{
		if (maxDelay > 1 && maxDelay < Count<S>::max() / 2) {
			return Power2::next(maxDelay);
		}
		throw std::invalid_argument("Maximum delay must be positive and have next larger power of two");
	}

	size_t validDelay(size_t delay)
	{
		if (delay <= maxDelay()) {
			return delay;
		}
		throw std::invalid_argument("Delay ");
	}

public:
	Delay(size_t maxDelay) :
		buffer_(validMaxDelay(maxDelay)),
		mask_(buffer_.size() - 1),
		read_(0),
		write_(0)
	{

	}

	Delay() : Delay(4000, 0) {}

	size_t maxDelay() const
	{
		return buffer_.size() - 1;
	}

	size_t delay() const
	{
		return (write_ > read_ ? write_ : mask_ + write_ + 1) - read_;
	}

	void setDelay(size_t newDelay)
	{
		validDelay(newDelay);
		size_t oldDelay = delay();
		size_t newWrite = (read_ + newDelay) & mask_;
		if (newDelay < oldDelay) {
			write_ = newWrite;
		}
		else {
			while (write_ != newWrite) {
				buffer_[write_++] = 0;
				write_ &= mask_;
			}
		}
	}

	void zero()
	{
		size_t rd = read_;
		while (rd != write_)
		{
			buffer_[rd++] = 0;
			rd &= mask_;
		}
	}

	S setAndGet(S value)
	{
		buffer_[write_++] = value;
		S result = buffer_[read_++];
		write_ &= mask_;
		read_ &= mask_;
		return result;
	}

};

} /* End of name space tdap */

#endif /* TDAP_DELAY_HEADER_GUARD */
