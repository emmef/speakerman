/*
 * SpeakerManager.hpp
 *
 * Part of 'Speaker management system'
 *
 * Copyright (C) 2013-2014 Michel Fleur.
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

#ifndef SMS_SPEAKERMAN_SPEAKERMANAGER_GUARD_H_
#define SMS_SPEAKERMAN_SPEAKERMANAGER_GUARD_H_

#include <iostream>
#include <tdap/Delay.hpp>
#include <tdap/FixedSizeArray.hpp>
#include <tdap/AdvancedRmsDetector.hpp>
#include <tdap/MemoryFence.hpp>
#include <tdap/Noise.hpp>
#include <tdap/Weighting.hpp>
#include <tdap/IirButterworth.hpp>
#include <speakerman/jack/JackProcessor.hpp>
#include <speakerman/jack/Names.hpp>
#include "DynamicsProcessor.hpp"

namespace speakerman {


class SpeakerManagerControl : public JackProcessor
{
public:

	virtual const SpeakermanConfig &getConfig() const = 0;
	virtual bool applyConfigAndGetLevels(const SpeakermanConfig &config, DynamicProcessorLevels *levels, std::chrono::milliseconds duration) = 0;
	virtual bool getLevels(DynamicProcessorLevels *levels, std::chrono::milliseconds duration) = 0;
};

template<typename T, size_t CHANNELS_PER_GROUP, size_t GROUPS>
class SpeakerManager : public SpeakerManagerControl
{
	static_assert(is_floating_point<T>::value, "expected floating-point value parameter");
	static constexpr size_t CROSSOVERS = 3;

	using Processor = DynamicsProcessor<T, CHANNELS_PER_GROUP, GROUPS, CROSSOVERS>;
	using CrossoverFrequencies = typename Processor::CrossoverFrequencies;
	using ThresholdValues = typename Processor::ThresholdValues;
	using Levels = DynamicProcessorLevels;
	using ConfigData = typename Processor::ConfigData;

	static constexpr size_t INPUTS = Processor::INPUTS;
	static constexpr size_t OUTPUTS = Processor::OUTPUTS;

	static CrossoverFrequencies crossovers()
	{
		CrossoverFrequencies cr;
		cr[0] = 80;
		cr[1] = 160;
		cr[2] = 4500;
		return cr;
	};

	static ThresholdValues thresholds(double value = 0.2)
	{
		ThresholdValues thres;
		double t = Values::min(value, 1.0 / sqrt(INPUTS));
		thres[0] = Values::min(0.9, t * INPUTS);
		for (size_t i = 1; i < thres.size(); i++) {
			thres[i] = t;
		}
		return thres;
	}

	PortDefinitions portDefinitions_;
	SpeakermanConfig config_;
	Processor processor;
	std::mutex mutex_;

	bool fewerInputs = false;
	bool fewerOutputs = false;

	struct TransportData
	{
		ConfigData configData;
		Levels levels;
		bool configChanged;

		TransportData() : levels(GROUPS), configChanged(false) {}
	};

	Transport<TransportData> transport;
	TransportData preparedConfigData;

protected:
	virtual const PortDefinitions &getDefinitions() override { return portDefinitions_; }
	virtual bool onMetricsUpdate(ProcessingMetrics metrics) override
	{
		std::cout << "Updated metrics: {rate:" << metrics.sampleRate << ", bsize:" << metrics.bufferSize << "}" << std::endl;
		AdvancedRms::UserConfig config = { 0.0005, 0.400, 0.5, 1.2 };
		processor.setSampleRate(metrics.sampleRate, crossovers(), config_);
		Levels levels(GROUPS);
		levels.reset();
		preparedConfigData.configData = processor.getConfigData();
		preparedConfigData.configChanged = true; // force to reload equalizer filters
		transport.init(preparedConfigData, true);
		preparedConfigData.configChanged = false;
		return true;
	}

	virtual void onPortsRegistered(jack_client_t *client, const Ports &) override
	{
		std::cout << "No action on ports registered" << std::endl;
		const char * unspecified = ".*";
		PortNames inputs(client, unspecified, unspecified, JackPortIsPhysical|JackPortIsOutput);
		PortNames outputs(client, unspecified, unspecified, JackPortIsPhysical|JackPortIsOutput);
	}

	virtual void onReset() override
	{
		std::cout << "No action on reset" << std::endl;
	}

	virtual bool process(jack_nframes_t frames, const Ports &ports) override
	{
		RefArray<jack_default_audio_sample_t> inputs[INPUTS];
		RefArray<jack_default_audio_sample_t> outputs[OUTPUTS];

		auto lockFreeData = transport.getLockFree();
		bool modifiedTransport = lockFreeData.modified();

		if (modifiedTransport) {
			processor.levels.reset();
			if (lockFreeData.data().configChanged) {
				processor.updateConfig(lockFreeData.data().configData);
			}
		}
		size_t portNumber;
		size_t index;
		for (index = 0, portNumber = 0; index < INPUTS; portNumber++, index++) {
			inputs[index] = ports.getBuffer(portNumber);
		}
		for (index = 0; index <= INPUTS; portNumber++, index++) {
			outputs[index] = ports.getBuffer(portNumber);
		}

		FixedSizeArray<T, INPUTS> inFrame;
		FixedSizeArray<T, OUTPUTS> outFrame;

		for (size_t i = 0; i < frames; i++) {

			for (size_t channel = 0; channel < INPUTS; channel++) {
				inFrame[channel] = inputs[channel][i];
			}

			processor.process(inFrame, outFrame);

			size_t channel;
			for (channel = 0; channel < OUTPUTS - 1; channel++) {
				outputs[channel][i] = outFrame[channel + 1];
			}
			outputs[channel][i] = outFrame[0];
		}

		lockFreeData.data().levels = processor.levels;

		return true;
	}

public:
	virtual bool needsBufferSize() const override { return false; }
	virtual bool needsSampleRate() const override { return true; }

	SpeakerManager(const SpeakermanConfig &config) :
		portDefinitions_(1 + 2 * SpeakermanConfig::MAX_GROUPS * SpeakermanConfig::MAX_GROUP_CHANNELS),
		config_(config)
	{
		char name[1 + Names::get_port_size()];
		for (size_t channel = 0; channel < Processor::INPUTS; channel++) {
			 snprintf(name, 1 + Names::get_port_size(),
					 "in_%zu_%zu", 1 + channel/CHANNELS_PER_GROUP, 1 + channel % CHANNELS_PER_GROUP);
			 portDefinitions_.addInput(name);
		}
		for (size_t channel = 0; channel < Processor::INPUTS; channel++) {
			snprintf(name, 1 + Names::get_port_size(),
					 "out_%zu_%zu", 1 + channel/CHANNELS_PER_GROUP, 1 + channel % CHANNELS_PER_GROUP);
			portDefinitions_.addOutput(name);
		}
		portDefinitions_.addOutput("out_sub");
	}

	virtual const SpeakermanConfig &getConfig() const override
	{
		return config_;
	}

	virtual bool getLevels(DynamicProcessorLevels *levels, std::chrono::milliseconds duration) override
	{
		std::unique_lock<std::mutex> lock(mutex_);
		TransportData result;
		preparedConfigData.levels.reset();
		if (transport.getAndSet(preparedConfigData, result, duration)) {
			if (levels) {
				*levels = result.levels;
			}
			return true;
		}
		return false;
	}

	virtual bool applyConfigAndGetLevels(const SpeakermanConfig &config, DynamicProcessorLevels *levels, std::chrono::milliseconds duration) override
	{
		std::unique_lock<std::mutex> lock(mutex_);
		TransportData result;
		config_ = config;
		preparedConfigData.configData = processor.createConfigData(config);
		preparedConfigData.levels.reset();
		preparedConfigData.configChanged = true;
		if (transport.getAndSet(preparedConfigData, result, duration)) {
			if (levels) {
				*levels = result.levels;
			}
			return true;
		}
		return false;
	}

};

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_SPEAKERMANAGER_GUARD_H_ */
