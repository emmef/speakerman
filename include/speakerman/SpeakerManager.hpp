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
#include <speakerman/jack/JackProcessor.hpp>
#include <tdap/Integration.hpp>
#include <tdap/Followers.hpp>

namespace speakerman {

class IntegratorArray
{
	static ValueRange<size_t> &sizeRange()
	{
		static ValueRange<size_t> range(2, 20);
		return range;
	}
	static ValueRange<double> &minRcRange()
	{
		static ValueRange<double> range(0.001, 0.02);
		return range;
	}
	static ValueRange<double> &maxRcRange()
	{
		static ValueRange<double> range(0.05, 4.0);
		return range;
	}
	static ValueRange<double> &followRcRange()
	{
		static ValueRange<double> range(0.001, 0.006);
		return range;
	}
	struct Filter
	{
		IntegratorFilter<double> integrator;
		double scale;
	};
	double maxRc_ = 0.4;
	double minRc_ = 0.006;
	double minOutput_ = 1.0;
	Array<Filter> filters_;
	SmoothHoldMaxAttackRelease<double> follower_;


public:
	IntegratorArray(size_t size) :
		filters_(sizeRange().getBetween(size)), follower_(1, 1, 1, 1) {}

	void setTimes(double sampleRate, double maxRc, double minRc, double followRc, double minOutput)
	{
		minOutput_ = minOutput;
		double followSamples = sampleRate * followRcRange().getBetween(followRc);
		follower_ = SmoothHoldMaxAttackRelease<double>(1.5*followSamples, followSamples, followSamples, minOutput);
		maxRc_ = maxRcRange().getBetween(maxRc);
		minRc_ = minRcRange().getBetween(minRc);
		double minMaxTimeRatio = minRc_ / maxRc_;
		double ratioIncrement = 1.0 / (filters_.size() - 1);
		for (size_t i = 0; i < filters_.size(); i++) {
			double ratio = pow(minMaxTimeRatio, ratioIncrement * i);
			double scale = pow(minMaxTimeRatio, 0.25 * ratioIncrement * i);
			double rc = maxRc_ * ratio;
			cout << "[" << i << "] " << rc << " scale " << scale << endl;
			filters_[i].integrator.coefficients.setCharacteristicSamples(sampleRate * rc);
			filters_[i].scale = scale;
		}
	}

	double integrate(double squareInput)
	{
		double value = minOutput_;
		for (size_t i = 0; i < filters_.size(); i++) {
			double x = filters_[i].integrator.integrate(squareInput);
			x *= filters_[i].scale;
			value = Value<double>::max(value, x);
		}

		return follower_.apply(sqrt(value));
	}

};

class SpeakerManager : public JackProcessor
{
	PortDefinitions portDefinitions_;
	IntegratorArray integrator;
	double threshold = 0.025;

	static constexpr size_t IN_1_1 = 0;
	static constexpr size_t IN_1_2 = 1;
	static constexpr size_t IN_2_1 = 2;
	static constexpr size_t IN_2_2 = 3;
	static constexpr size_t OUT_1_1 = 4;
	static constexpr size_t OUT_1_2 = 5;
	static constexpr size_t OUT_2_1 = 6;
	static constexpr size_t OUT_2_2 = 7;
	static constexpr size_t OUT_SUB = 8;

protected:
	virtual const PortDefinitions &getDefinitions() override { return portDefinitions_; }
	virtual bool onMetricsUpdate(ProcessingMetrics metrics) override
	{
		std::cout << "Updated metrics: {rate:" << metrics.sampleRate << ", bsize:" << metrics.bufferSize << "}" << std::endl;
		integrator.setTimes(
				metrics.sampleRate,
				1.0, 0.01, 0.001, threshold);
		return true;
	}
	virtual void onPortsRegistered(const Ports &) override
	{
		std::cout << "No action on ports registered" << std::endl;
	}
	virtual void onReset() override
	{
		std::cout << "No action on reset" << std::endl;
	}
	virtual bool process(jack_nframes_t frames, const Ports &ports) override
	{
		for (size_t i = 0; i < frames; i++) {
			jack_default_audio_sample_t x11 = ports.getBuffer(IN_1_1)[i];
			jack_default_audio_sample_t x12 = ports.getBuffer(IN_1_2)[i];
			jack_default_audio_sample_t x21 = ports.getBuffer(IN_2_1)[i];
			jack_default_audio_sample_t x22 = ports.getBuffer(IN_2_2)[i];

			double squareSum =
					x11 * x11 +
					x12 * x12 +
					x21 * x21 +
					x22 * x22;

			double rms = integrator.integrate(squareSum);
			double gain = threshold / rms;

			ports.getBuffer(OUT_1_1)[i] = gain * x11;
			ports.getBuffer(OUT_1_2)[i] = gain * x12;
			ports.getBuffer(OUT_2_1)[i] = gain * x21;
			ports.getBuffer(OUT_2_2)[i] = gain *x22;

			ports.getBuffer(OUT_SUB)[i] = 0.25 * gain * (
					ports.getBuffer(IN_1_1)[i] +
					ports.getBuffer(IN_1_2)[i] +
					ports.getBuffer(IN_2_1)[i] +
					ports.getBuffer(IN_2_2)[i]);
		}
		return true;
	}

public:
	virtual bool needsBufferSize() const override { return false; }
	virtual bool needsSampleRate() const override { return true; }

	SpeakerManager() :
		portDefinitions_(16, 32), integrator(15)
	{
		portDefinitions_.addInput("in_1_channel_1");
		portDefinitions_.addInput("in_1_channel_2");
		portDefinitions_.addInput("in_2_channel_1");
		portDefinitions_.addInput("in_2_channel_2");

		portDefinitions_.addOutput("out_1_channel_1");
		portDefinitions_.addOutput("out_1_channel_2");
		portDefinitions_.addOutput("out_2_channel_1");
		portDefinitions_.addOutput("out_2_channel_2");
		portDefinitions_.addOutput("out_subwoofer");
	}
};

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_SPEAKERMANAGER_GUARD_H_ */
