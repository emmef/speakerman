/*
 * Matrix.hpp
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

#ifndef SMS_SPEAKERMAN_MATRIX_GUARD_H_
#define SMS_SPEAKERMAN_MATRIX_GUARD_H_

#include <simpledsp/Array.hpp>
#include <simpledsp/Precondition.hpp>

namespace speakerman {

// Your definitions

template<typename Sample> class Matrix
{

	Array<Sample *> ins;
	Array<Sample *> outs;
	Array<Sample> factors;
	Array<Sample> unconnected_ins;
	Array<Sample> unconnected_outs;
	Sample min;
	Sample max;

	inline size_t indexOf(size_t input, size_t output) const {
		return ins.length() * output + input;
	}

public:
	Matrix(size_t inputs, size_t outputs, Sample minimum, Sample maximum) :
		ins(Precondition::validPositiveCount<Sample>(inputs, "Matrix: inputs")),
		outs(Precondition::validPositiveCount<Sample>(outputs, "Matrix: outputs")),
		factors(Precondition::validPositiveProduct<Sample>(inputs, outputs, "Matrix: total size")),
		unconnected_ins(inputs),
		unconnected_outs(outputs),
		min(minimum < maximum ? minimum : maximum),
		max(minimum < maximum ? maximum : minimum)
	{
		for (size_t in = 0; in < ins.length(); in++) {
			unconnected_ins[in] = 0.0;
			ins[in] = &unconnected_ins[in];
		}
		for (size_t out = 0; out < outs.length(); out++) {
			unconnected_outs[out] = 0.0;
			outs[out] = &unconnected_outs[out];
		}
		for (size_t factor = 0; factor < factors.length(); factor++) {
			factors[factor] = min;
		}
	}

	size_t inputs() const
	{
		return ins.length();
	}

	size_t outputs() const
	{
		return outs.length();
	}

	void setInput(size_t index, Sample &input)
	{
		ins[index] = &input != nullptr ? &input : &unconnected_ins[index];
	}

	void setOutput(size_t index, Sample &output)
	{
		outs[index] = &output != nullptr ? &output : &unconnected_outs[index];
	}

	void resetInput(size_t index)
	{
		ins[index] = &unconnected_ins[index];
	}

	void resetOutput(size_t index)
	{
		outs[index] = &unconnected_outs[index];
	}

	void setFactor(size_t in, size_t out, Sample factor)
	{
		if (in < ins.length() && out < outs.length()) {
			factors[indexOf(in, out)] = factor <= min ? min : factor >= max ? max : factor;
		}
	}

	const Sample getFactor(size_t in, size_t out)
	{
		if (in < ins.length() && out < outs.length()) {
			return factors[indexOf(in, out)];
		}

		return min;
	}

	Sample input(size_t index) const
	{
		return *ins[index];
	}

	Sample output(size_t index) const
	{
		return *outs[index];
	}

	void setInputValue(size_t index, Sample value)
	{
		*ins[index] = value;
	}

	void setUnconnectedInputValues(Sample value) const
	{
		for (size_t in = 0; in < ins.length(); in++) {
			unconnected_ins[in] = value;
		}
	}

	Sample &unconnectedInput(size_t index) const
	{
		return unconnected_ins[index];
	}

	void multiply() const
	{
		for (size_t out = 0; out < outs.length(); out++) {
			Sample sum = 0.0;
			for (size_t in = 0; in < ins.length(); in++) {
				sum += *ins[in] * factors[indexOf(in, out)];
			}
			*outs[out] = sum;
		}
	}
};

static void bla() {
	Matrix<double> m(10, 10, 0, 1);

	m.resetOutput(1);
	m.input(2);
	m.multiply();
}

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_MATRIX_GUARD_H_ */
