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
#include <array>
#include <jack/jack.h>
#include <simpledsp/Types.hpp>
#include <simpledsp/Values.hpp>
#include <simpledsp/Precondition.hpp>
#include <simpledsp/Array.hpp>
#include <simpledsp/List.hpp>
#include <simpledsp/MultibandSplitter.hpp>
#include <speakerman/VolumeMatrix.hpp>
#include <speakerman/Frame.hpp>

namespace speakerman {

static constexpr size_t MAX_CHANNELS = 16;
static constexpr size_t SUB_FILTER_ORDER = 2;


template<size_t ORDER, size_t CROSSOVERS, size_t INS, size_t CHANNELS, size_t OUTS, size_t SUBS> class SpeakerManager
{
	typedef Multiband::Splitter<accurate_t, ORDER, CROSSOVERS, CHANNELS> Splitter;
	typedef typename Splitter::SplitterPlan Plan;

	Plan plan;
	Splitter splitter;
	VolumeMatrix<accurate_t, INS, CHANNELS> inMatrix;
	VolumeMatrix<accurate_t, CHANNELS, OUTS> outMatrix;
	VolumeMatrix<accurate_t, CHANNELS, SUBS> subMatrix;
	array<accurate_t, INS> input;
	array<accurate_t, OUTS> output;
	array<accurate_t, SUBS> subs;
	array<accurate_t, CHANNELS> outInput;
	array<accurate_t, CHANNELS> subInput;

	array<accurate_t, CHANNELS> afterInMatrix;
	array<freq_t, CROSSOVERS> crossovers;

	template <size_t C, size_t R> void connectDefaults(VolumeMatrix<accurate_t, C, R> &matrix, accurate_t scale)
	{
		size_t max = matrix.columns() < matrix.rows() ? matrix.rows() : matrix.columns();

		for (size_t i = 0; i < max; i++) {
			matrix.set(i % matrix.columns(), i % matrix.rows(), scale);
		}
	}

public:
	// ins, outs and subs should be named groups so that named volume controls can apply
	SpeakerManager() :
		splitter(plan),
		inMatrix(1e-6, 1.0 * CHANNELS / INS),
		outMatrix(1e-6, 1.0 * OUTS / CHANNELS),
		subMatrix(1e-6, 1.0 * SUBS / CHANNELS)
	{
		splitter.reload();

		connectDefaults(inMatrix, inMatrix.getMaximum());
		connectDefaults(outMatrix, outMatrix.getMaximum());
		connectDefaults(subMatrix, subMatrix.getMaximum());
	}
	VolumeMatrix<accurate_t, INS, CHANNELS> &inputMatrix()
	{
		return inMatrix;
	}
	VolumeMatrix<accurate_t, CHANNELS, OUTS> &outputMatrix()
	{
		return outMatrix;
	}
	VolumeMatrix<accurate_t, CHANNELS, SUBS> &subWooferMatrix()
	{
		return subMatrix;
	}
	array<accurate_t, INS> &getInput()
	{
		return input;
	}
	const array<accurate_t, OUTS> &getOutput()
	{
		return output;
	}
	const array<accurate_t, SUBS> &getSubWoofer()
	{
		return subs;
	}

	void process()
	{
		inMatrix.multiply(input, afterInMatrix);

		auto separated = splitter.process(afterInMatrix);

		for (size_t i = 0; i < CHANNELS; i++) {
			accurate_t highs = 0.0;
			for (size_t i = 0; i < CROSSOVERS; i++) {
				highs += separated(1, i);
			}
			outInput[i] = highs;
			subInput[i] = separated(CROSSOVERS, i);
		}

		outMatrix.multiply(outInput, output);
		subMatrix.multiply(subInput, subs);
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
