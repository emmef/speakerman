/*
 * SpeakerMan.hpp
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

#ifndef SMS_SPEAKERMAN_SPEAKERMAN_GUARD_H_
#define SMS_SPEAKERMAN_SPEAKERMAN_GUARD_H_

#include <iostream>
#include <vector>
#include <jack/jack.h>
#include <simpledsp/Types.hpp>
#include <simpledsp/Values.hpp>
#include <simpledsp/Precondition.hpp>
#include <simpledsp/Array.hpp>
#include <simpledsp/List.hpp>
#include <simpledsp/MultibandSplitter.hpp>
#include <speakerman/Frame.hpp>
#include <speakerman/Matrix.hpp>

namespace speakerman {

static constexpr size_t MAX_CHANNELS = 16;
static constexpr size_t SUB_FILTER_ORDER = 2;


template<size_t ORDER, size_t CROSSOVERS, size_t INS, size_t CHANNELS, size_t OUTS, size_t SUBS> class SpeakerManager
{
	typedef Multiband::Splitter<accurate_t, ORDER, CROSSOVERS, CHANNELS> Splitter;
	typedef typename Splitter::SplitterPlan Plan;

	Plan plan;
	Splitter splitter;
	Matrix<sample_t> inMatrix;
	Matrix<sample_t> outMatrix;
	Matrix<sample_t> subMatrix;
	array<accurate_t, CHANNELS> afterInMatrix;
	array<freq_t, CROSSOVERS> crossovers;

	void connectDefaults(Matrix<sample_t> &matrix, sample_t scale)
	{
		size_t max = matrix.inputs() < matrix.outputs() ? matrix.outputs() : matrix.inputs();

		for (size_t i = 0; i < max; i++) {
			matrix.setFactor(i % matrix.inputs(), i % matrix.outputs(), scale);
		}
	}

public:
	// ins, outs and subs should be named groups so that named volume controls can apply
	SpeakerManager() :
		splitter(plan),
		inMatrix(INS, CHANNELS, 1e-6, 1.0 * CHANNELS / INS),
		outMatrix(CHANNELS, OUTS, 1e-6, 1.0 * OUTS / CHANNELS),
		subMatrix(CHANNELS, SUBS, 1e-6, 1.0 * SUBS / CHANNELS)
	{
		splitter.reload();

		connectDefaults(inMatrix, inMatrix.getMaximum());
		connectDefaults(outMatrix, outMatrix.getMaximum());
		connectDefaults(subMatrix, subMatrix.getMaximum());
	}
	Matrix<sample_t> &inputMatrix()
	{
		return inMatrix;
	}
	Matrix<sample_t> &outputMatrix()
	{
		return outMatrix;
	}
	Matrix<sample_t> &subWooferMatrix()
	{
		return subMatrix;
	}
	Array<sample_t> &getInput()
	{
		inMatrix.getInput();
	}
	const Array<sample_t> &getOutput() const
	{
		return outMatrix.getOutput();
	}
	const Array<sample_t> &getSubWoofer() const
	{
		return subMatrix.getOutput();
	}

	void process()
	{
		inMatrix.multiply();

		Array<sample_t> &inOutput = inMatrix.getOutput();
		for (size_t i = 0; i < CHANNELS; i++) {
			afterInMatrix[i] = inOutput(i);
		}

		auto separated = splitter.process(afterInMatrix);

		Array<sample_t> &outInput = outMatrix.getInput();
		Array<sample_t> &subInput = subMatrix.getInput();
		for (size_t i = 0; i < CHANNELS; i++) {
			subInput[i] = separated[0][i];
			outInput[i] = separated[1][i];
		}

		outMatrix.multiply();
		subMatrix.multiply();
	}
	void configure(array<freq_t, CROSSOVERS> frequencies, freq_t sampleRate)
	{
		for (size_t i = 0; i < CROSSOVERS; i++) {
			plan.setCrossover(i, frequencies[i] / sampleRate);
		}
		splitter.reload();
	}
	void configure() {
		for (size_t i = 0; i < splitter.size(); i++) {
			splitter.get(i).configure();
		}
	}
};

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_SPEAKERMAN_GUARD_H_ */
