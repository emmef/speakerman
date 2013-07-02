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

using namespace simpledsp;
// Your definitions

template<typename Sample> class Matrix
{
	const size_t ins;
	const size_t outs;
	Array<Sample> factors;
	Array<Sample> unconnected_in;
	Array<Sample> unconnected_out;
	Array<Sample> *inSamples;
	Array<Sample> *outSamples;
	Sample min;
	Sample max;

	inline size_t indexOf(size_t input, size_t output) const {
		return ins * output + input;
	}

public:
	Matrix(size_t inputs, size_t outputs, Sample minimum, Sample maximum) :
		ins(Precondition::validPositiveCount<Sample>(inputs, "Matrix: inputs")),
		outs(Precondition::validPositiveCount<Sample>(outputs, "Matrix: outputs")),
		factors(Precondition::validPositiveProduct<Sample>(inputs, outputs, "Matrix: total size")),
		unconnected_in(ins, CopyZero::ZERO),
		unconnected_out(outs, CopyZero::ZERO),
		inSamples(&unconnected_in),
		outSamples(&unconnected_out),
		min(minimum < maximum ? minimum : maximum),
		max(minimum < maximum ? maximum : minimum)
	{
		for (size_t factor = 0; factor < factors.length(); factor++) {
			factors[factor] = min;
		}
	}

	Matrix(Array<Sample> &inputs, size_t outputs, Sample minimum, Sample maximum) :
		ins(inputs.length()),
		outs(Precondition::validPositiveCount<Sample>(outputs, "Matrix: outputs")),
		factors(Precondition::validPositiveProduct<Sample>(inputs, outputs, "Matrix: total size")),
		unconnected_in(ins, CopyZero::ZERO),
		unconnected_out(outs, CopyZero::ZERO),
		inSamples(&inputs),
		outSamples(&unconnected_out),
		min(minimum < maximum ? minimum : maximum),
		max(minimum < maximum ? maximum : minimum)
	{
		for (size_t factor = 0; factor < factors.length(); factor++) {
			factors[factor] = min;
		}
	}

	Matrix(size_t inputs, Array<Sample> &outputs, Sample minimum, Sample maximum) :
		ins(inputs),
		outs(outputs.length()),
		factors(Precondition::validPositiveProduct<Sample>(inputs, outputs, "Matrix: total size")),
		unconnected_in(ins, CopyZero::ZERO),
		unconnected_out(outs, CopyZero::ZERO),
		inSamples(&unconnected_in),
		outSamples(&outputs),
		min(minimum < maximum ? minimum : maximum),
		max(minimum < maximum ? maximum : minimum)
	{
		for (size_t factor = 0; factor < factors.length(); factor++) {
			factors[factor] = min;
		}
	}
	size_t inputs() const
	{
		return ins;
	}

	size_t outputs() const
	{
		return outs;
	}
	Array<Sample> &getInput() const
	{
		return *inSamples;
	}
	Array<Sample> &getOutput() const
	{
		return *outSamples;
	}
	void setInput(Array<Sample> &input)
	{
		if (input.length() == ins) {
			inSamples = &input;
			return;
		}
		throw std::invalid_argument("Input array must have same size as number of matrix inputs");
	}

	void setOutput(Array<Sample> &output)
	{
		if (output.length() == outs) {
			outSamples = &output;
			return;
		}
		throw std::invalid_argument("Output array must have same size as number of matrix outputs");
	}

	void resetInput(size_t index)
	{
		inSamples = &unconnected_in;
	}

	void resetOutput(size_t index)
	{
		outSamples = &unconnected_out;
	}

	void setFactor(size_t in, size_t out, Sample factor)
	{
		if (in < ins && out < outs) {
			factors[indexOf(in, out)] = factor <= min ? min : factor >= max ? max : factor;
		}
	}

	const Sample getFactor(size_t in, size_t out)
	{
		if (in < ins && out < outs) {
			return factors[indexOf(in, out)];
		}

		return min;
	}

	void multiply() const
	{
		const Array<Sample> &input = *inSamples;
		const Array<Sample> &output = *outSamples;

		for (size_t out = 0; out < outs; out++) {
			Sample sum = 0.0;
			for (size_t in = 0; in < ins; in++) {
				sum += input[in] * factors[indexOf(in, out)];
			}
			output[out] = sum;
		}
	}
};

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_MATRIX_GUARD_H_ */
