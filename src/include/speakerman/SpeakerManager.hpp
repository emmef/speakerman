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
#include <speakerman/SignalGroup.hpp>
#include <speakerman/jack/JackProcessor.hpp>
#include <speakerman/jack/Names.hpp>

namespace speakerman {

template<size_t CHANNELS>
class SpeakerManager : public JackProcessor
{
	static constexpr size_t GROUPS = CHANNELS / 2;
	AdvancedRms::Detector<15> integrator;
	Delay<double> rmsDelay ;

	HoldMaxDoubleIntegrated<double> limiter;
	Delay<double> peakDelay ;
	PortDefinitions portDefinitions_;
	ACurves::Filter<double, 2 * CHANNELS> weighting;
	PinkNoise::Default pinkNoise;

	double volume_ = 20.0;

	double threshold = 0.2;
	double limiterThreshold = 0.25;
	double minRc = 0.0005;
	double preGain = 1.0;
	bool fewerInputs = false;
	bool fewerOutputs = false;

protected:
	virtual const PortDefinitions &getDefinitions() override { return portDefinitions_; }
	virtual bool onMetricsUpdate(ProcessingMetrics metrics) override
	{
		std::cout << "Updated metrics: {rate:" << metrics.sampleRate << ", bsize:" << metrics.bufferSize << "}" << std::endl;
		AdvancedRms::UserConfig config = { 0.0005, 0.400, 0.5, 1.2 };
		integrator.userConfigure(config, metrics.sampleRate);
//		setTimes(
//				metrics.sampleRate,
//				0.4, minRc, threshold);
		double limiterSamples = metrics.sampleRate * 0.0005;
		size_t delaySamples = 0.5 + 3 * limiterSamples;
		delaySamples *= 4; // (4 channels)
		peakDelay.setDelay(delaySamples);
		delaySamples = 0.5 + 3 * minRc;
		delaySamples *= 4; // (4 channels)
		rmsDelay.setDelay(delaySamples);
		limiterThreshold = Value<double>::min(1.0, threshold * 4);
		limiter = HoldMaxDoubleIntegrated<double>(3 * limiterSamples, limiterSamples, limiterThreshold);
		weighting.setSampleRate(metrics.sampleRate);
		pinkNoise.setScale(1e-5);
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
		RefArray<jack_default_audio_sample_t> inputs[CHANNELS];
		RefArray<jack_default_audio_sample_t> outputs[CHANNELS + 1];

		size_t portNumber;
		size_t index;
		for (index = 0, portNumber = 0; index < CHANNELS; portNumber++, index++) {
			inputs[index] = ports.getBuffer(portNumber);
		}
		for (index = 0; index <= CHANNELS; portNumber++, index++) {
			outputs[index] = ports.getBuffer(portNumber);
		}
		double x[CHANNELS];
		double k[CHANNELS];
		double square[CHANNELS];

		for (size_t i = 0; i < frames; i++) {
			double squareSum = 0.0;
			double noise = pinkNoise();
			for (size_t channel = 0; channel < CHANNELS; channel++) {
				double X = volume_ * (noise + inputs[channel][i]);
				double K = X;//weighting.filter(channel, X);
				squareSum += K * K;
				x[channel] = X;
			}

			double rms = integrator.integrate(squareSum, threshold);
			double gain = threshold / rms;

			double peak = limiterThreshold;
			for (size_t channel = 0; channel < CHANNELS; channel++) {
				double y = gain * rmsDelay.setAndGet(x[channel]);
				peak = Value<double>::max(peak, y);
				x[channel] = y;
			}

			double limiterGain = limiterThreshold / limiter.apply(peak);
			double sub = 0.0;
			for (size_t channel = 0; channel < CHANNELS; channel++) {
				double y = limiterGain * peakDelay.setAndGet(x[channel]);
				sub += y;
				outputs[channel][i] = Value<double>::clamp(y, -limiterThreshold, limiterThreshold);
			}

			outputs[CHANNELS][i] = Value<double>::clamp(0.25 * sub, -limiterThreshold, limiterThreshold);
		}
		return true;
	}

public:
	virtual bool needsBufferSize() const override { return false; }
	virtual bool needsSampleRate() const override { return true; }

	SpeakerManager() :
		portDefinitions_(16, 32), peakDelay(10000), rmsDelay(10000), weighting(48000)
	{
		char name[1 + Names::get_port_size()];
		bool left = true;
		for (size_t channel = 0; channel < CHANNELS; channel++, left = !left) {
			 snprintf(name, 1 + Names::get_port_size(),
					 "in_%zu_%s", 1 + channel/2, left ? "left" : "right");
			 portDefinitions_.addInput(name);
		}
		for (size_t channel = 0; channel < CHANNELS; channel++, left = !left) {
			snprintf(name, 1 + Names::get_port_size(),
					 "out_%zu_%s", 1 + channel/2, left ? "left" : "right");
			portDefinitions_.addOutput(name);
		}
		portDefinitions_.addOutput("out_sub");
	}

	double volume() const { MemoryFence m; return volume_; }
	void setVolume(double newVolume) {MemoryFence m; volume_ = newVolume; }
};

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_SPEAKERMANAGER_GUARD_H_ */
