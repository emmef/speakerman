/*
 * BandSplitter.hpp
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

#ifndef SMS_SPEAKERMAN_BANDSPLITTER_GUARD_H_
#define SMS_SPEAKERMAN_BANDSPLITTER_GUARD_H_

#include <stdlib.h>
#include <simpledsp/Values.hpp>
#include <simpledsp/Precondition.hpp>
#include <simpledsp/Array.hpp>
#include <simpledsp/Butterfly.hpp>
#include <simpledsp/Butterworth.hpp>
#include <simpledsp/Iir.hpp>
#include <simpledsp/List.hpp>
#include <simpledsp/Multiplexer.hpp>
#include <speakerman/Limiter.hpp>
#include <simpledsp/Noise.hpp>

namespace speakerman {

using namespace simpledsp;

class BandSplitter
{
public:
	static constexpr size_t MAX_INPUTS = 16;
	static constexpr size_t MAX_CROSSOVERS = 12;
private:
	typedef Iir::FixedOrderMultiFilter<sample_t, accurate_t, 2, 2 * MAX_INPUTS> Filter;

	Noise<MAX_INPUTS> noise;
	Array<sample_t> input;
	Array<sample_t> output;
	Array<frequency_t> crossover;
	ButterflyPlan plan;
	Array<sample_t> filterOutput;
	List<Filter> lowPass;
	List<Filter> highPass;
	List<LimiterSettingsWithThreshold> limiterSetting;
	List<Limiter> limiter;
	frequency_t sampleFrequency;

	static size_t validInputCount(size_t ins) {
		if (ins > 1 && ins <= MAX_INPUTS) {
			return ins;
		}
		throw std::invalid_argument("Number of input must be between 1 and 8");
	}

	size_t indexOf(size_t channel, size_t band) const
	{
		return input.length() * band + channel;
	}

	void configureLimiters()
	{
		for (size_t i = 0; i < plan.outputs(); i++) {
			limiter.get(i).reconfigure(sampleFrequency);
		}
	}

	void configureFilters()
	{
		CoefficientBuilder builder(2);
		for (size_t i = 0, j = 0; i < crossover.length(); i++) {
			Butterworth::createCoefficients(builder, sampleFrequency, crossover[i], Butterworth::Pass::LOW, true);
			lowPass.get(j++).setCoefficients(builder);
			lowPass.get(j++).setCoefficients(builder);
			Butterworth::createCoefficients(builder, sampleFrequency, crossover[i], Butterworth::Pass::HIGH, true);
			highPass.get(j++).setCoefficients(builder);
			highPass.get(j++).setCoefficients(builder);
		}
		noise.setCutoff(sampleFrequency, 1000);
	}

public:

	BandSplitter(size_t channels, const Array<frequency_t> &crossovers, const LimiterSettings &limiterSettings) :
		noise(pow(2, -23), 96000, 1000),
		input(channels),
		output(channels),
		crossover(crossovers),
		plan(crossover.length()),
		filterOutput(channels * plan.outputs()),
		lowPass(plan.size()),
		highPass(plan.size()),
		limiterSetting(plan.outputs()),
		limiter(plan.outputs())
	{
		CoefficientBuilder builder(2);
		for (size_t i =0; i < crossover.length(); i++) {
			Butterworth::createCoefficients(builder, 96000, crossover[i], Butterworth::Pass::HIGH);
			highPass.add(builder);
			highPass.add(builder); // Linkwitz Riley applies 2 Butterworth filters
			Butterworth::createCoefficients(builder, 96000, crossover[i], Butterworth::Pass::LOW);
			highPass.add(builder);
			highPass.add(builder); // Linkwitz Riley applies 2 Butterworth filters
		}
		for (size_t i = 0; i < plan.outputs(); i++) {
			limiterSetting.add(LimiterSettingsWithThreshold(limiterSettings));
			limiter.add(limiterSetting.get(i));
		}
	}
	size_t channels() const
	{
		return input.length();
	}
	Array<sample_t> &in()
	{
		return input;
	}
	Array<sample_t> &out()
			{
		return output;
	}
	const Array<frequency_t> &crossovers() const
	{
		return crossover;
	}
	const size_t limiters() const
	{
		return limiter.size();
	}
	void setSoftThreshold(size_t limiterIndex, accurate_t threshold)
	{
		limiterSetting.get(limiterIndex).setSoftThreshold(threshold);
		configureLimiters();
	}
	void process() {
		// Frequency-splitting
		for (size_t channel = 0; channel < input.length(); channel++) {
			ButterflyEntry entry = plan[0];
			filterOutput[indexOf(channel, entry.input())] = input[channel] + noise.get(channel);

			for (size_t band = 0, filterOffs = 0; band < plan.size(); band++) {
				entry = plan[band];
				size_t inPosition = 2 * entry.input();
				accurate_t y = filterOutput[indexOf(channel, entry.input())];

				accurate_t butterLo = lowPass.get(filterOffs).fixed(channel, y);
				accurate_t butterHi = highPass.get(filterOffs).fixed(channel, y);
				filterOffs++;
				accurate_t linkwitzLo = lowPass.get(filterOffs).fixed(channel, butterLo);
				accurate_t linkwitzHi = highPass.get(filterOffs).fixed(channel, butterHi);
				filterOffs++;

				filterOutput[indexOf(channel, entry.output1())] = linkwitzLo;
				filterOutput[indexOf(channel, entry.output2())] = linkwitzHi;
			}
		}
		// Detect and limit
		for (size_t band = 0; band < plan.outputs(); band++) {
			accurate_t detection = 0.0;
			for (size_t channel = 0; channel < input.length(); channel++) {
				accurate_t x = filterOutput[indexOf(channel, band)];
				detection += x * x;
			}
			Limiter &lim = limiter.get(band);
			lim.detect(sqrt(detection));
			for (size_t channel = 0; channel < input.length(); channel++) {
				accurate_t &x = filterOutput[indexOf(channel, band)];
				x = lim.getLimited(x);
			}
		}
		// Write to output
		for (size_t channel = 0; channel < input.length(); channel++) {
			sample_t y = 0.0;
			for (size_t band = 0, filterOffs = 0; band < plan.size(); band++) {
				y += filterOutput[indexOf(channel, band)];
			}
			output[channel] = y;
		}
	}

	void configure() {
		configureFilters();
		configureLimiters();
	}

	void configure(frequency_t sampleRate) {
		sampleFrequency = Frequency::validRate(sampleRate);
	}
};

}/* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_BANDSPLITTER_GUARD_H_ */
