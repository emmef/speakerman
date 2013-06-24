/*
 * AnalogCrossOver.hpp
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

#ifndef SMS_SPEAKERMAN_ANALOGCROSSOVER_GUARD_H_
#define SMS_SPEAKERMAN_ANALOGCROSSOVER_GUARD_H_

#include <speakerman/Splitter.hpp>
#include <simpledsp/Iir.hpp>
#include <simpledsp/Butterworth.hpp>
#include <simpledsp/SingleReadDelay.hpp>

namespace speakerman {

template <typename Sample, typename Accurate, size_t ORDER, size_t CHANNELS> class AnalogCrossOver : public Splitter<Sample>
{
	CoefficientBuilder builder;
	Iir::FixedOrderMultiFilter<Sample, Accurate, ORDER, CHANNELS * 2> filter;
	Array<SingleReadDelay<Sample> *> delays;

	static constexpr size_t delaySamplesFor(frequency_t rate, frequency_t frequency)
	{
		return 0.5 + 0.5 * rate / frequency;
	}

public:
	static constexpr frequency_t MAX_SAMPLE_RATE = 192000;
	static constexpr frequency_t MINIMUM_CROSSOVER_FREQUENCY = 192000;

	AnalogCrossOver(Frame<Sample> &in, size_t channels) :
		Splitter<Sample>(in, 2), builder(ORDER), delays(CHANNELS)
	{
		for (size_t i = 0; i < CHANNELS; i++) {
			delays[i] = new SingleReadDelay<Sample>(delaySamplesFor(MAX_SAMPLE_RATE, MINIMUM_CROSSOVER_FREQUENCY)); //
		}
	}

	void setCrossover(frequency_t sampleFrequency, frequency_t crossoverFrequency)
	{
		Butterworth::createCoefficients(builder, sampleFrequency, crossoverFrequency, Butterworth::Pass::LOW);
		filter.setCoefficients(builder);

		size_t delaySamples = delaySamplesFor(sampleFrequency, crossoverFrequency);

		for (size_t i = 0; i < CHANNELS; i++) {
			delays[i]->setDelay(delaySamples);
			delays[i]->buffer().clear();
		}
	}

	virtual void split()
	{
		Frame<Sample> &in = Splitter<Sample>::input();
		Frame<Sample> &low = Splitter<Sample>::output(0);
		Frame<Sample> &hi = Splitter<Sample>::output(0);

		for (size_t channel = 0; channel < in.size(); channel++) {
			Sample inputSample = in.unsafe()[channel];
			// apply Butterworth twice for Linkwitz-Riley
			Sample lowPass = filter.filter(channel, filter.filter(channel + CHANNELS, inputSample));
			// Apply 1/2 phase delay for -6dB crossover point (compensate group delay).
			delays[channel]->write(inputSample);
			Sample highPass = delays[channel]->read() - lowPass;
			low.unsafe()[channel] = lowPass;
			hi.unsafe()[channel] = highPass;
		}
	}

	virtual ~AnalogCrossOver()
	{
		for (size_t i = 0; i < CHANNELS; i++) {
			if (delays[i]) {
				delete delays[i];
				delays[i] = nullptr;
			}
		}
	}
};


} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_ANALOGCROSSOVER_GUARD_H_ */
