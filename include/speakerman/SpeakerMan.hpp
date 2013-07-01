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

#include <jack/jack.h>
#include <simpledsp/Values.hpp>
#include <simpledsp/Precondition.hpp>
#include <simpledsp/Array.hpp>
#include <simpledsp/List.hpp>
#include <speakerman/Frame.hpp>
#include <speakerman/Matrix.hpp>
#include <speakerman/BandSplitter.hpp>
#include <speakerman/Limiter.hpp>

namespace speakerman {

static constexpr size_t MAX_CHANNELS = 16;
static constexpr size_t SUB_FILTER_ORDER = 2;

class MultibandLimiterConfig
{
	const size_t channels_;
	const Array<frequency_t> crossovers_;
	const LimiterSettings &settings_;
public:
	MultibandLimiterConfig(const Array<frequency_t> &crossovers, const size_t channels, const LimiterSettings &settings) :
		channels_(Precondition::validPositiveProduct<sample_t>(channels, crossovers.length()) / crossovers.length()),
		crossovers_(crossovers),
		settings_(settings_)
	{
	}
	size_t channels() const
	{
		return channels_;
	}
	const Array<frequency_t> &crossovers() const
	{
		return crossovers_;
	}
	const LimiterSettings &settings() const
	{
		return settings_;
	}
};

class MultibandLimiterConfigList
{
	const LimiterSettings *settings;
	Array<frequency_t> crossovers;
	size_t crossoverIndex = 0;
	size_t ch = 0;
	List<MultibandLimiterConfig> list;
	size_t totalInputs_ = 0;

public:
	MultibandLimiterConfigList() : crossovers(12), list(2) {};

	MultibandLimiterConfigList &addCrossover(frequency_t frequency)
	{
		if (crossoverIndex == BandSplitter::MAX_CROSSOVERS) {
			throw std::runtime_error("Cannot add more crossovers");
		}
		for (size_t i = 0; i < crossoverIndex; i++) {
			if (crossovers[i] > frequency) {
				throw std::invalid_argument("Each crossovers must have higher frequency than previous one");
			}
		}
		crossovers[crossoverIndex++] = frequency;

		return *this;
	}

	MultibandLimiterConfigList &setChannels(size_t channels)
	{
		ch = Precondition::validPositiveCount<sample_t>(channels);

		return *this;
	}

	MultibandLimiterConfigList &setLimiterConfig(const LimiterSettings *config) {
		if (config == nullptr) {
			throw nullptr_error("MultibandLimiterConfigList::setLimiterConfig");
		}
		settings = config;

		return *this;
	}

	MultibandLimiterConfigList &add()
	{
		if (crossoverIndex == 0) {
			throw runtime_error("No crossovers configured");
		}
		if (ch == 0) {
			throw runtime_error("No number of channels configured");
		}
		if (!settings) {
			throw runtime_error("No limiter settings specified");
		}
		Array<frequency_t> cr(crossoverIndex, crossovers);
		list.add(cr, ch, *settings);

		totalInputs_ += ch;

		crossoverIndex = 0;
		ch = 0;
		settings = nullptr;

		return *this;
	}

	size_t totalInputs() const {
		if (totalInputs_ == 0) {
			throw runtime_error("MultibandLimiterConfigList: no configurations yet");
		}
		return totalInputs_;
	}

	const List<MultibandLimiterConfig> &build() const
	{
		return list;
	}

};

class SpeakerManager
{
	typedef Iir::FixedOrderMultiFilter<sample_t, accurate_t, 2, 2 * BandSplitter::MAX_INPUTS> Filter;

	Matrix<sample_t> inMatrix;
	Matrix<sample_t> outMatrix;
	Matrix<sample_t> subsMatrix;
	List<BandSplitter> splitter;
	Array<sample_t> lowOutput;
	Array<sample_t> highOutput;
	frequency_t crossover;
	Filter lowPass;
	Filter highPass;

	void connectDefaults(Matrix<sample_t> &matrix, sample_t scale)
	{
		size_t max = matrix.inputs() < matrix.outputs() ? matrix.outputs() : matrix.inputs();

		for (size_t i = 0; i < max; i++) {
			matrix.setFactor(i % matrix.inputs(), i % matrix.outputs(), scale);
		}
	}

public:
	// ins, outs and subs should be named groups so that named volume controls can apply
	SpeakerManager(size_t ins, const MultibandLimiterConfigList &list, size_t outs, size_t subs, frequency_t crossoverFrequency) :
		inMatrix(ins, list.totalInputs(), 1e-6, 1.0),
		outMatrix(list.totalInputs(), outs, 1e-6, 1.0),
		subsMatrix(outs, subs, 1e-6, 1.0),
		lowOutput(outs),
		highOutput(outs),
		splitter(2),
		crossover(crossoverFrequency)
	{
		const List<MultibandLimiterConfig> &configs = list.build();
		size_t ioIndex = 0;
		for (size_t i = 0; i < splitter.size(); i++) {
			const MultibandLimiterConfig &config = configs.get(i);
			BandSplitter &s = splitter.add(config.channels(), config.crossovers(), config.settings());
			for (size_t channel = 0; channel < s.channels(); channel++, ioIndex++) {
				inMatrix.setOutput(ioIndex, s.in()[channel]);
				outMatrix.setInput(ioIndex, s.in()[channel]);
			}
		}
		for (size_t channel = 0; channel < outMatrix.outputs(); channel++) {
			subsMatrix.setInput(channel, lowOutput[channel]);
		}
		connectDefaults(inMatrix, 1.0);
		connectDefaults(outMatrix, 1.0);
		connectDefaults(subsMatrix, 0.5);
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
		return subsMatrix;
	}
	void setInputValue(size_t index, sample_t value)
	{
		inMatrix.setInputValue(index, value);
	}
	sample_t getOutputValue(size_t index) const
	{
		return highOutput[index];
	}
	sample_t getSubWooferValue(size_t index) const
	{
		return lowOutput[index];
	}

	void process()
	{
		inMatrix.multiply();
		for (size_t i = 0; i < splitter.size(); i++) {
			splitter.get(i).process();
		}
		outMatrix.multiply();
		for (size_t channel = 0, filterOffs = 0; channel < outMatrix.outputs(); channel++, filterOffs += 2) {
			sample_t output = outMatrix.output(channel);
			lowOutput[channel] = lowPass.fixed(filterOffs, lowPass.fixed(filterOffs + 1, output));
			highOutput[channel] = highPass.fixed(filterOffs, highPass.fixed(filterOffs + 1, output));
		}
		subsMatrix.multiply();
	}
	void configure(frequency_t sampleRate)
	{
		for (size_t i = 0; i < splitter.size(); i++) {
			splitter.get(i).configure(sampleRate);
		}
		CoefficientBuilder builder(2);
		Butterworth::createCoefficients(builder, sampleRate, crossover, Butterworth::Pass::LOW, true);
		lowPass.setCoefficients(builder);
		Butterworth::createCoefficients(builder, sampleRate, crossover, Butterworth::Pass::HIGH, true);
		highPass.setCoefficients(builder);
	}
	void configure() {
		for (size_t i = 0; i < splitter.size(); i++) {
			splitter.get(i).configure();
		}
	}
};

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_SPEAKERMAN_GUARD_H_ */
