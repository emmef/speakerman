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




template<typename T, size_t CHANNELS_PER_GROUP, size_t GROUPS, size_t CROSSOVERS>
class DynamicsProcessor
{
	static_assert(is_floating_point<T>::value, "expected floating-point value parameter");
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

	static constexpr size_t CHANNEL_DELAY_SIZE =
			SpeakermanConfig::MAX_GROUP_CHANNELS * (2 + 192000 * GroupConfig::MAX_DELAY);

	using CrossoverFrequencies = FixedSizeArray<T, CROSSOVERS>;
	using ThresholdValues = FixedSizeArray<T, LIMITERS>;
	using Configurable = SpeakermanRuntimeConfigurable<T, GROUPS, BANDS, CHANNELS_PER_GROUP>;
	using ConfigData = SpeakermanRuntimeData<T, GROUPS, BANDS>;

	class ChannelDelay : public Delay<T>
	{
	public:
		ChannelDelay() : Delay<T>(CHANNEL_DELAY_SIZE) {}
	};

private:
	PinkNoise::Default noise;
	PinkNoise::Default subNoise;
	FixedSizeArray<T, INPUTS> inputWithVolumeAndNoise;
	FixedSizeArray<T, PROCESSING_CHANNELS> processInput;
	FixedSizeArray<T, OUTPUTS> output;
	FixedSizeArray<T, BANDS> relativeBandWeights;

	Crossovers::Filter<double, T, INPUTS, CROSSOVERS>  crossoverFilter;
	ACurves::Filter<T, PROCESSING_CHANNELS> aCurve;

	FixedSizeArray<AdvancedRms::Detector<T, 18>, DETECTORS> rmsDetector;

	FixedSizeArray<HoldMaxDoubleIntegrated<T>, LIMITERS> limiter;
	Delay<T> rmsDelay;
	Delay<T> limiterDelay;
	ChannelDelay channelDelay[LIMITERS];
	EqualizerFilter<double, CHANNELS_PER_GROUP> filters_[GROUPS];

	Configurable runtime;
	FixedSizeArray<IntegratorFilter<T>, LIMITERS> signalIntegrator;

	T sampleRate_;
	bool bypass = true;

	static AdvancedRms::UserConfig rmsUserConfig()
	{
		return { 0.0005, 0.5, 0.5, 1.0 };
	}

	static AdvancedRms::UserConfig rmsUserSubConfig()
	{
		return { 0.010, 0.5, 0.75, 1.0 };
	}

public:
	DynamicProcessorLevels levels;

	DynamicsProcessor() : rmsDelay(96000), limiterDelay(96000), sampleRate_(0), levels(GROUPS, CROSSOVERS)
	{
		levels.reset();
	}

	void setSampleRate(
			T sampleRate,
			const FixedSizeArray<T, CROSSOVERS> &crossovers,
			const SpeakermanConfig &config)
	{
		noise.setScale(1e-5);
		subNoise.setScale(1e-5);
		aCurve.setSampleRate(sampleRate);
		crossoverFilter.configure(sampleRate, crossovers);
		for (size_t i = 0; i < GROUPS; i++) {
			signalIntegrator[i].coefficients.setCharacteristicSamples(AdvancedRms::PERCEPTIVE_FAST_WINDOWSIZE * sampleRate);
		}

		// Rms detector confiuration
		AdvancedRms::UserConfig rmsConfig = rmsUserConfig();
		size_t rmsDelaySamples = 0.5 + rmsConfig.minRc * 3 * sampleRate;
		rmsDelay.setDelay(PROCESSING_CHANNELS * rmsDelaySamples);

		// Limiter delay and integration constants
		T limiterIntegrationSamples = 0.001 * sampleRate;
		size_t limiterHoldSamples = 0.5 + 3 * limiterIntegrationSamples;
		for (size_t i = 0; i < LIMITERS; i++) {
			limiter[i].setMetrics(limiterIntegrationSamples, limiterHoldSamples);
		}
		limiterDelay.setDelay(LIMITERS * limiterHoldSamples);

		rmsDetector[0].userConfigure(rmsUserSubConfig(), sampleRate);
		for (size_t i = 1; i < DETECTORS; i++) {
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
		return runtime.userSet();
	}

	ConfigData createConfigData(const SpeakermanConfig &config)
	{
		ConfigData data;
		data.configure(config, sampleRate_, relativeBandWeights, rmsUserConfig().peakWeight / 1.5);
		data.dump();
		return data;
	}

	void updateConfig(const ConfigData &data)
	{
		runtime.modify(data);
		channelDelay[0].setDelay(data.subDelay());
		for (size_t group = 0; group < GROUPS; group++) {
			filters_[group].configure(data.groupConfig(group).filterConfig());
			channelDelay[1 + group].setDelay(CHANNELS_PER_GROUP * data.groupConfig(group).delay());
		}

	}

	void process(
			const FixedSizeArray<T, INPUTS> &input,
			FixedSizeArray<T, OUTPUTS> &target)
	{
		runtime.approach();
		applyVolumeAddNoise(input);
		moveToProcessingChannels(crossoverFilter.filter(inputWithVolumeAndNoise));
		processSubRms();
		processChannelsRms();
		mergeFrequencyBands();
		processChannelsFilters();
//		for (size_t i = 0; i < OUTPUTS; i++) {
//			target[i] = output[i];
//		}
		processSubLimiter(target);
		processChannelsLimiter(target);
	}

private:
	void applyVolumeAddNoise(const FixedSizeArray<T, INPUTS> &input)
	{
		T ns = noise();
		for (size_t group = 0, offs = 0; group < GROUPS; group++) {
			T signal = 0;
			const GroupRuntimeData<T, BANDS> &conf = runtime.data().groupConfig(group);
			T volume = conf.volume();
			for (size_t channel = 0; channel < CHANNELS_PER_GROUP; channel++, offs++) {
				T x = input[offs];
				signal += x * x;
				inputWithVolumeAndNoise[offs] = x * volume + ns;
			}
			levels.setSignal(group, sqrt(signalIntegrator[group].integrate(signal)) * conf.signalMeasureFactor());
		}
	}


	void moveToProcessingChannels(const FixedSizeArray<T, CROSSOVER_OUPUTS> &multi)
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
		T x = processInput[0] + subNoise();
		processInput[0] = rmsDelay.setAndGet(x);
		x *= runtime.data().subRmsScale();
		T detect = rmsDetector[0].integrate_smooth(x * x, 1.0);
		T gain = 1.0 / detect;
		levels.setSubGain(gain);
		processInput[0] *= gain;
	}

	void processChannelsRms()
	{
		for (size_t band = 0, baseOffset = 1, detector = 1; band < CROSSOVERS; band++) {
			for (size_t group = 0; group < GROUPS; group++, detector++) {
				T squareSum = 0.0;
				T scaleForUnity = runtime.data().groupConfig(group).bandRmsScale(1 + band);
				size_t nextOffset = baseOffset + CHANNELS_PER_GROUP;
				for (size_t offset = baseOffset; offset < nextOffset; offset++) {
					T x = processInput[offset];
					processInput[offset] = rmsDelay.setAndGet(x);
					T y = aCurve.filter(offset, x);
					y *= scaleForUnity;
					squareSum += y * y;
				}
				T detect = rmsDetector[detector].integrate_smooth(squareSum, 1.0);
				T gain = 1.0 / detect;
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
			T sum = 0.0;
			size_t max = channel + INPUTS * CROSSOVERS;
			for (size_t offset = channel; offset < max; offset += INPUTS) {
				sum += processInput[offset];
			}
			output[channel] = sum;
		}
	}

	void processChannelsFilters()
	{
		for (size_t group = 0, offs = 1; group < GROUPS; group++) {
			auto filter = filters_[group].filter();
			for (size_t channel = 0; channel < CHANNELS_PER_GROUP; channel++, offs++) {
				output[offs] = filter->filter(channel, output[offs]);
			}
		}
	}

	void processSubLimiter(FixedSizeArray<T, OUTPUTS> &target)
	{
		T scale = runtime.data().subLimiterScale();
		T threshold = runtime.data().subLimiterThreshold();
		T x = output[0];
		output[0] = limiterDelay.setAndGet(x);
		x *= scale;
		T detect = limiter[0].applyWithMinimum(fabs(x), 1.0);
		T gain = 1.0 / detect;
		T out = Values::clamp(gain * output[0], -threshold, threshold);
		target[0] = channelDelay[0].setAndGet(out);
	}

	void processChannelsLimiter(FixedSizeArray<T, OUTPUTS> &target)
	{
		for (size_t group = 0; group < GROUPS; group++) {
			const size_t startOffs = 1 + group * CHANNELS_PER_GROUP;
			T peak = 0.0;
			for (size_t channel = 0, offset = startOffs; channel < CHANNELS_PER_GROUP; channel++, offset++) {
				T x = output[offset];
				output[offset] = limiterDelay.setAndGet(x);
				peak = Values::max(peak, fabs(x));
			}
			T scale = runtime.data().groupConfig(group).limiterScale();
			T threshold = runtime.data().groupConfig(group).limiterThreshold();
			peak *= scale;
			T detect = limiter[group + 1].applyWithMinimum(peak, 1.0);
			T gain = 1.0 / detect;
			for (size_t channel = 0, offset = startOffs; channel < CHANNELS_PER_GROUP; channel++, offset++) {
				T out = Values::clamp(gain * output[offset], -threshold, threshold);
				target[offset] = out;//channelDelay[group + 1].setAndGet(out);
			}
		}
	}
};


} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_SIGNALGROUP_GUARD_H_ */
