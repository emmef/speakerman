/*
 * SignalGroup.hpp
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

#ifndef SMS_SPEAKERMAN_SIGNALGROUP_GUARD_H_
#define SMS_SPEAKERMAN_SIGNALGROUP_GUARD_H_

#include <cmath>
#include <tdap/Delay.hpp>
#include <tdap/AdvancedRmsDetector.hpp>
#include <tdap/MemoryFence.hpp>
#include <tdap/Weighting.hpp>
#include <tdap/Noise.hpp>
#include <tdap/Crossovers.hpp>
#include <tdap/Transport.hpp>
#include <speakerman/SpeakermanRuntimeData.hpp>

namespace speakerman {

using namespace tdap;


class DynamicProcessorLevels
{
	double gains_[SpeakermanConfig::MAX_GROUPS + 1];
	double signal_[SpeakermanConfig::MAX_GROUPS];
	size_t groups_;

public:
	DynamicProcessorLevels(size_t groups) : groups_(groups) {}

	size_t groups() const { return groups_; }
	size_t signals() const { return groups_ + 1; }

	void reset()
	{
		for (size_t limiter = 0; limiter <= groups_; limiter++) {
			gains_[limiter] = 0.0;
		}
		for (size_t group = 0; group < groups_; group++) {
			signal_[group] = 0.0;
		}
	}

	void setGroupGain(size_t group, double gain)
	{
		double &g = gains_[1 + IndexPolicy::array(group, groups_)];
		g = Values::min(g, gain);
	}

	double getGroupGain(size_t group) const
	{
		return gains_[1 + IndexPolicy::array(group, groups_)];
	}

	void setSubGain(double gain)
	{
		double &g = gains_[0];
		g = Values::min(g, gain);
	}

	double getGroupGain() const
	{
		return gains_[0];
	}

	void setSignal(size_t group, double signal)
	{
		double &s = signal_[IndexPolicy::array(group, groups_)];
		s = Values::max(s, signal);
	}

	double getSignal(size_t group) const
	{
		return signal_[IndexPolicy::array(group, groups_)];
	}
};


template<size_t CHANNELS_PER_GROUP, size_t GROUPS, size_t CROSSOVERS>
class DynamicsProcessor
{
public:
	static constexpr size_t INPUTS = GROUPS * CHANNELS_PER_GROUP;
	// bands are around crossovers
	static constexpr size_t BANDS = CROSSOVERS + 1;
	// multiplex by frequency bands
	static constexpr size_t CROSSOVER_OUPUTS = INPUTS * BANDS;
	// sub-woofer groupChannels summed, so don't process CROSSOVER_OUPUTS groupChannels
	static constexpr size_t PROCESSING_CHANNELS = 1 + CROSSOVERS * INPUTS;
	// RMS detection are per group, not per channel (and only one for sub)
	static constexpr size_t DETECTORS = 1 + CROSSOVERS * GROUPS;
	// Limiters are per group and sub
	static constexpr size_t LIMITERS = 1 + GROUPS;
	// OUTPUTS
	static constexpr size_t OUTPUTS = INPUTS + 1;

	using CrossoverFrequencies = FixedSizeArray<double, CROSSOVERS>;
	using ThresholdValues = FixedSizeArray<double, LIMITERS>;
	using Configurable = SpeakermanRuntimeConfigurable<double, GROUPS, BANDS, CHANNELS_PER_GROUP>;
	using ConfigData = SpeakermanRuntimeData<double, GROUPS, BANDS>;


private:
	PinkNoise::Default noise;
	PinkNoise::Default subNoise;
	FixedSizeArray<double, INPUTS> inputWithVolumeAndNoise;
	FixedSizeArray<double, PROCESSING_CHANNELS> processInput;
	FixedSizeArray<double, OUTPUTS> output;
	FixedSizeArray<double, BANDS> relativeBandWeights;

	Crossovers::Filter<double, INPUTS, CROSSOVERS>  crossoverFilter;
	ACurves::Filter<double, PROCESSING_CHANNELS> aCurve;

	FixedSizeArray<AdvancedRms::Detector<15>, DETECTORS> rmsDetector;

	FixedSizeArray<HoldMaxDoubleIntegrated<double>, LIMITERS> limiter;
	Delay<double> rmsDelay;
	Delay<double> limiterDelay;
	Configurable runtime;
	IntegrationCoefficients<double> signalIntegrator;

	double sampleRate_;
	bool bypass = true;

	static AdvancedRms::UserConfig rmsUserConfig()
	{
		return { 0.0005, 0.35, 0.5, 1.2 };
	}

public:
	DynamicProcessorLevels levels;

	DynamicsProcessor() : rmsDelay(96000), limiterDelay(96000), sampleRate_(0), levels(GROUPS)
	{
		levels.reset();
	}

	void setSampleRate(
			double sampleRate,
			const FixedSizeArray<double, CROSSOVERS> &crossovers,
			const SpeakermanConfig &config)
	{
		noise.setScale(1e-5);
		subNoise.setScale(1e-5);
		aCurve.setSampleRate(sampleRate);
		crossoverFilter.configure(sampleRate, crossovers);
		signalIntegrator.setCharacteristicSamples(0.1 * sampleRate);

		// Rms detector confiuration
		AdvancedRms::UserConfig rmsConfig = rmsUserConfig();
		size_t rmsDelaySamples = 0.5 + rmsConfig.minRc * 3 * sampleRate;
		rmsDelay.setDelay(PROCESSING_CHANNELS * rmsDelaySamples);

		// Limiter delay and integration constants
		double limiterIntegrationSamples = 0.0005 * sampleRate;
		size_t limiterHoldSamples = 0.5 + 4 * limiterIntegrationSamples;
		for (size_t i = 0; i < LIMITERS; i++) {
			limiter[i].setMetrics(limiterIntegrationSamples, limiterHoldSamples);
		}
		limiterDelay.setDelay(LIMITERS * limiterHoldSamples);

		for (size_t i = 0; i < DETECTORS; i++) {
			rmsDetector[i].userConfigure(rmsConfig, sampleRate);
		}
		auto weights = Crossovers::weights(crossovers, sampleRate);
		relativeBandWeights[0] = weights[0];
		for (size_t band = 1; band <= CROSSOVERS; band++) {
			relativeBandWeights[band] = weights[2 * band + 1];
		}

		sampleRate_ = sampleRate;
		runtime.init(createConfigData(config));
	}

	const ConfigData &getConfigData() const
	{
		return runtime.data();
	}

	ConfigData createConfigData(const SpeakermanConfig &config)
	{
		ConfigData data;
		data.configure(config, sampleRate_, relativeBandWeights, rmsUserConfig().peakWeight / 1.5);
		return data;
	}

	void updateConfig(const ConfigData &data)
	{
		runtime.modify(data);
	}

	void process(
			const FixedSizeArray<double, INPUTS> &input,
			FixedSizeArray<double, OUTPUTS> &target)
	{
		runtime.approach();
		applyVolumeAddNoise(input);
		moveToProcessingChannels(crossoverFilter.filter(inputWithVolumeAndNoise));
		processSubRms();
		processChannelsRms();
		mergeFrequencyBands();
		processSubLimiter();
		processChannelsLimiter();
	}

private:
	void applyVolumeAddNoise(const FixedSizeArray<double, INPUTS> &input)
	{
		double ns = noise();
		for (size_t group = 0, offs = 0; group < GROUPS; group++) {
			double signal = 0;
			double volume = runtime.data().groupConfig(group).volume();
			for (size_t channel = 0; channel < CHANNELS_PER_GROUP; channel++, offs++) {
				double x = input[offs];
				signal += x * x;
				inputWithVolumeAndNoise[offs] = x * volume + ns;
			}
			levels.setSignal(group, sqrt(signal));
		}
	}


	void moveToProcessingChannels(const FixedSizeArray<double, CROSSOVER_OUPUTS> &multi)
	{
		// Sum all lowest frequency bands
		processInput[0] = 0.0;
		for (size_t channel = 0; channel < INPUTS; channel++) {
			processInput[0] += multi[channel];
		}

		// copy rest of groupChannels
		for (size_t i = 1, channel = INPUTS; i < processInput.size(); i++, channel++) {
			processInput[i] = multi[channel];
		}
	}

	void processSubRms()
	{
		double x = processInput[0] + subNoise();
		processInput[0] = rmsDelay.setAndGet(x);
		double scaleForUnity = runtime.data().subRmsScale();
		double detectionWeight = runtime.data().subRmsThreshold();
		double detect = scaleForUnity * rmsDetector[0].integrate(x * x, detectionWeight);
		double gain = 1.0 / detect;
		levels.setSubGain(gain);
		processInput[0] *= gain;
	}

	void processChannelsRms()
	{
		for (size_t band = 0, baseOffset = 1, detector = 1; band < CROSSOVERS; band++) {
			for (size_t group = 0; group < GROUPS; group++, detector++) {
				double squareSum = 0.0;
				size_t nextOffset = baseOffset + CHANNELS_PER_GROUP;
				for (size_t offset = baseOffset; offset < nextOffset; offset++) {
					double x = processInput[offset];
					processInput[offset] = rmsDelay.setAndGet(x);
					double y = aCurve.filter(offset, x);
					squareSum += y * y;
				}
				double detectionWeight = runtime.data().groupConfig(group).bandRmsThreshold(band);
				double scaleForUnity = runtime.data().groupConfig(group).bandRmsScale(band);
				double detect = scaleForUnity * rmsDetector[detector].integrate(squareSum, detectionWeight);
				double gain = 1.0 / detect;
				levels.setGroupGain(group, gain);
				for (size_t offset = baseOffset; offset < nextOffset; offset++) {
					processInput[offset] *= gain;
				}
				baseOffset = nextOffset;
			}
		}
	}

	void mergeFrequencyBands()
	{
		output[0] = processInput[0];
		for (size_t channel = 1; channel <= INPUTS; channel++) {
			double sum = 0.0;
			size_t max = channel + INPUTS * CROSSOVERS;
			for (size_t offset = channel; offset < max; offset += INPUTS) {
				sum += processInput[offset];
			}
			output[channel] = sum;
		}
	}

	void processSubLimiter()
	{
		double scale = runtime.data().subLimiterScale();
		double threshold = runtime.data().subLimiterThreshold();
		double x = output[0];
		output[0] = limiterDelay.setAndGet(x);
		double detect = limiter[0].applyWithMinimum(fabs(x), threshold);
		double gain = 1.0 / detect;
		levels.setSubGain(gain);
		output[0] = Values::clamp(gain * output[0], -threshold, threshold);
	}

	void processChannelsLimiter() {
		for (size_t group = 1, channel = 1; group <= GROUPS; group++) {
			size_t max = channel + CHANNELS_PER_GROUP;
			double peak = 0.0;
			for (size_t offset = channel; offset < max; offset++) {
				double x = output[offset];
				output[offset] = limiterDelay.setAndGet(x);
				peak = Values::max(peak, fabs(output[offset]));
			}
			double threshold = runtime.data().groupConfig(group).limiterThreshold();
			double scale = runtime.data().groupConfig(group).limiterScale();
			double detect = scale * limiter[group].applyWithMinimum(peak, threshold);
			double gain = 1.0 / detect;
			levels.setGroupGain(group, gain);
			for (size_t offset = channel; offset < max; offset++) {
				output[offset] = Values::clamp(gain * output[offset], -threshold, threshold);
			}
		}
	}

};


} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_SIGNALGROUP_GUARD_H_ */
