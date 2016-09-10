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

// RAII FPU state class, sets FTZ and DAZ and rounding, no exceptions 
// Adapted from code by mystran @ kvraudio
// http://www.kvraudio.com/forum/viewtopic.php?t=312228&postdays=0&postorder=asc&start=0

class ZFPUState
{
private:
  unsigned int sse_control_store;

public:
  enum Rounding
  {
      kRoundNearest = 0,
      kRoundNegative,
      kRoundPositive,
      kRoundToZero,
  };

  ZFPUState(Rounding mode = kRoundToZero)
  {
      sse_control_store = _mm_getcsr();

      // bits: 15 = flush to zero | 6 = denormals are zero 
      // bitwise-OR with exception masks 12:7 (exception flags 5:0) 
      // rounding 14:13, 00 = nearest, 01 = neg, 10 = pos, 11 = to zero 
      // The enum above is defined in the same order so just shift it up 
      _mm_setcsr(0x8040 | 0x1f80 | ((unsigned int)mode << 13));
  }

  ~ZFPUState()
  {
      // clear exception flags, just in case (probably pointless) 
      _mm_setcsr(sse_control_store & (~0x3f));
  }
};


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
	// Limiters are per group and sub
	static constexpr size_t DELAY_CHANNELS = 1 + GROUPS * CHANNELS_PER_GROUP;
	// OUTPUTS
	static constexpr size_t OUTPUTS = INPUTS + 1;

	static constexpr double GROUP_MAX_DELAY = GroupConfig::MAX_DELAY;
	static constexpr double LIMITER_MAX_DELAY = 0.01;
	static constexpr double RMS_MAX_DELAY = 0.01;

	static constexpr size_t GROUP_MAX_DELAY_SAMPLES = 0.5 + 192000 * GroupConfig::MAX_DELAY;
	static constexpr size_t LIMITER_MAX_DELAY_SAMPLES = 0.5 + 192000 * LIMITER_MAX_DELAY;
	static constexpr size_t RMS_MAX_DELAY_SAMPLES = 0.5 + 192000 * RMS_MAX_DELAY;

	using CrossoverFrequencies = FixedSizeArray<T, CROSSOVERS>;
	using ThresholdValues = FixedSizeArray<T, LIMITERS>;
	using Configurable = SpeakermanRuntimeConfigurable<T, GROUPS, BANDS, CHANNELS_PER_GROUP>;
	using ConfigData = SpeakermanRuntimeData<T, GROUPS, BANDS>;

	class GroupDelay : public Delay<T>
	{
	public:
		      GroupDelay() : Delay<T>(GROUP_MAX_DELAY_SAMPLES) {}
	};
	
	class LimiterDelay : public Delay<T>
	{
	public:
		LimiterDelay() : Delay<T>(LIMITER_MAX_DELAY_SAMPLES) {}
	};
	
	class RmsDelay : public Delay<T>
	{
	public:
		RmsDelay() : Delay<T>(RMS_MAX_DELAY_SAMPLES) {}
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
	using Detector = AdvancedRms::Detector<T>;

	Detector *rmsDetector;

	FixedSizeArray<HoldMaxDoubleIntegrated<T>, LIMITERS> limiter;
	RmsDelay rmsDelay[PROCESSING_CHANNELS];
	LimiterDelay limiterDelay[LIMITERS];
	GroupDelay groupDelay[DELAY_CHANNELS];
	EqualizerFilter<double, CHANNELS_PER_GROUP> filters_[GROUPS];

	Configurable runtime;
	FixedSizeArray<IntegratorFilter<T>, LIMITERS> signalIntegrator;

	T sampleRate_;
	bool bypass = true;
	
	static constexpr double PERCEIVED_FAST_BURST_POWER = 0.25;
	static constexpr double PERCEIVED_SLOW_BURST_POWER = 0.15;

	static AdvancedRms::UserConfig rmsUserConfig()
	{
		static constexpr double SLOW = 0.4;
		static constexpr double FAST = 0.0005;
		
		return { FAST, SLOW, 
			pow(FAST / AdvancedRms::PERCEPTIVE_FAST_WINDOWSIZE, PERCEIVED_FAST_BURST_POWER),
			pow(SLOW / AdvancedRms::PERCEPTIVE_FAST_WINDOWSIZE, PERCEIVED_SLOW_BURST_POWER) };
	}

	static AdvancedRms::UserConfig rmsUserSubConfig()
	{
		static constexpr double SLOW = 0.4;
		static constexpr double FAST = 0.0005;
		
		return { FAST, SLOW, 
			pow(FAST / AdvancedRms::PERCEPTIVE_FAST_WINDOWSIZE, PERCEIVED_FAST_BURST_POWER), 
			pow(SLOW / AdvancedRms::PERCEPTIVE_FAST_WINDOWSIZE, PERCEIVED_SLOW_BURST_POWER) };
	}

public:
	DynamicProcessorLevels levels;

	DynamicsProcessor() : sampleRate_(0), levels(GROUPS, CROSSOVERS), rmsDetector(new Detector[DETECTORS])
	{
		levels.reset();
	}

	~DynamicsProcessor() {
		delete [] rmsDetector;
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
		for (size_t channel = 0; channel < PROCESSING_CHANNELS; channel++) {
			rmsDelay[channel].setDelay(rmsDelaySamples);
		}

		// Limiter delay and integration constants
		T limiterIntegrationSamples = 0.001 * sampleRate;
		size_t limiterHoldSamples = 0.5 + 3 * limiterIntegrationSamples;
		for (size_t i = 0; i < LIMITERS; i++) {
			limiter[i].setMetrics(limiterIntegrationSamples, limiterHoldSamples);
		}
		for (size_t channel = 0; channel < LIMITERS; channel++) {
			limiterDelay[channel].setDelay(limiterHoldSamples);
		}

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
		groupDelay[0].setDelay(data.subDelay());
		for (size_t group = 0, i = 1; group < GROUPS; group++) {
			filters_[group].configure(data.groupConfig(group).filterConfig());
			size_t delaySamples = data.groupConfig(group).delay();
			for (size_t channel = 0; channel < CHANNELS_PER_GROUP; channel++, i++) {
				groupDelay[i].setDelay(delaySamples);
			}
		}

	}

	void process(
			const FixedSizeArray<T, INPUTS> &input,
			FixedSizeArray<T, OUTPUTS> &target)
	{
		ZFPUState state;
		runtime.approach();
		applyVolumeAddNoise(input);
		moveToProcessingChannels(crossoverFilter.filter(inputWithVolumeAndNoise));
		processSubRms();
		processChannelsRms();
		mergeFrequencyBands();
		processChannelsFilters();
		processSubLimiter(target);
		processChannelsLimiter(target);
	}

private:
	void applyVolumeAddNoise(const FixedSizeArray<T, INPUTS> &input)
	{
		T ns = noise();
		for (size_t group = 0; group < GROUPS; group++) {
			const GroupRuntimeData<T, BANDS> &conf = runtime.data().groupConfig(group);
			auto volume = conf.volume();
			T signal = 0;
			for (size_t channel = 0; channel < CHANNELS_PER_GROUP; channel++) {
				T x = 0.0;
				for (size_t inGroup = 0; inGroup < GROUPS; inGroup++) {
					x += volume[inGroup] * input[inGroup * CHANNELS_PER_GROUP + channel];
				}
				signal += x * x;
				inputWithVolumeAndNoise[group * CHANNELS_PER_GROUP + channel] = x + ns;
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
		processInput[0] = rmsDelay[0].setAndGet(x);
		x *= runtime.data().subRmsScale();
		T detect = rmsDetector[0].integrate_smooth(x * x, 1.0);
		T gain = 1.0 / detect;
		levels.setSubGain(gain);
		processInput[0] *= gain;
	}

	void processChannelsRms()
	{
		for (size_t band = 0, delay = 1, baseOffset = 1, detector = 1; band < CROSSOVERS; band++) {
			for (size_t group = 0; group < GROUPS; group++, detector++) {
				T squareSum = 0.0;
				T scaleForUnity = runtime.data().groupConfig(group).bandRmsScale(1 + band);
				size_t nextOffset = baseOffset + CHANNELS_PER_GROUP;
				for (size_t offset = baseOffset; offset < nextOffset; offset++, delay++) {
					T x = processInput[offset];
					processInput[offset] = rmsDelay[delay].setAndGet(x);
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
		output[0] = limiterDelay[0].setAndGet(x);
		x *= scale;
		T detect = limiter[0].applyWithMinimum(fabs(x), 1.0);
		T gain = 1.0 / detect;
		T out = Values::clamp(gain * output[0], -threshold, threshold);
		target[0] = groupDelay[0].setAndGet(out);
	}

	void processChannelsLimiter(FixedSizeArray<T, OUTPUTS> &target)
	{
		for (size_t group = 0, groupDelayChannel = 1, limiterDelayChannel = 1; group < GROUPS; group++) {
			const size_t startOffs = 1 + group * CHANNELS_PER_GROUP;
			T peak = 0.0;
			for (size_t channel = 0, offset = startOffs; channel < CHANNELS_PER_GROUP; channel++, offset++, limiterDelayChannel++) {
				T x = output[offset];
				output[offset] = limiterDelay[limiterDelayChannel].setAndGet(x);
				peak = Values::max(peak, fabs(x));
			}
			T scale = runtime.data().groupConfig(group).limiterScale();
			T threshold = runtime.data().groupConfig(group).limiterThreshold();
			peak *= scale;
			T detect = limiter[group + 1].applyWithMinimum(peak, 1.0);
			T gain = 1.0 / detect;
			for (size_t channel = 0, offset = startOffs; channel < CHANNELS_PER_GROUP; channel++, offset++, groupDelayChannel++) {
				T out = Values::clamp(gain * output[offset], -threshold, threshold);
				target[offset] = groupDelay[groupDelayChannel].setAndGet(out);
			}
		}
	}
};


} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_SIGNALGROUP_GUARD_H_ */
