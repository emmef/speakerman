/*
 * Mixer.hpp
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

#ifndef SMS_SPEAKERMAN_MIXER_GUARD_H_
#define SMS_SPEAKERMAN_MIXER_GUARD_H_

#include <speakerman/Frame.hpp>

namespace speakerman {

template<typename Sample> class Mixer
{
	Sample *c;
	Frame<Sample> &input;
	Frame<Sample> &output;

	Sample &at(size_t in, size_t out)
	{
		return c[input.size() * out + in];
	}

public:
	Mixer(Frame<Sample> &in, Frame<Sample> &out)
		: c(new Sample[Precondition::validPositiveProduct<Sample>(input.size(), output.size(), "Vector rows x columns too large")]), input(in), output(out)
	{
		clear();
	}

	Sample &operator()(size_t in, size_t out)
	{
		if (in < input.size() && out < output.size()) {
			return at(in, out);
		}
		if (in < input.size()) {
			throw out_of_range("Output out of range");
		}
		throw out_of_range("Input out of range");
	}

	void multiply() {
		for (size_t out = 0; out < output.size(); out++) {
			Sample sum = 0.0;
			for (size_t in = 0; in < input.size(); in++) {
				sum += at(in, out) * input.x_(in);
			}
			output.x_[out] = sum;
		}
	}

	bool isOutputConnected(size_t out) const
	{
		for (size_t in = 0; in < input.size(); in++) {
			if (at(in, out) != 0) {
				return true;
			}
		}

		return false;
	}

	bool isInputConnected(size_t in) const
	{
		for (size_t out = 0; out < input.size(); out++) {
			if (at(in, out) != 0) {
				return true;
			}
		}

		return false;
	}

	void clear()
	{
		for (size_t out = 0; out < output.size(); out++) {
			for (size_t in = 0; in < input.size(); in++) {
				at(in, out) = 0.0;
			}
		}
	}

	void identity(bool repeat)
	{
		clear();
		if (repeat) {
			size_t max = input.size() > output.size() ? input.size() : output.size();
			for (size_t i = 0; i < max; i++) {
				at(i % input.size(), i % output.size()) = 1.0;
			}
		}
		else {
			size_t min = input.size() < output.size() ? input.size() : output.size();
			for (size_t i = 0; i < min; i++) {
				at(i, i) = 1.0;
			}
		}
	}
};


} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_MIXER_GUARD_H_ */
