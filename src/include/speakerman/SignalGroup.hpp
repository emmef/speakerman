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

namespace speakerman {

using namespace tdap;

template<size_t CHANNELS_PER_GROUP, size_t GROUPS>
struct VolumeControl
{
	static constexpr size_t CHANNELS = GROUPS * CHANNELS_PER_GROUP;

	class Matrix {
		double volume_[CHANNELS][CHANNELS];

	public:
		Matrix(double value) {
			setAll(value);
		}

		Matrix() : Matrix(0) { }

		double validVolume(double volume) {
			if (volume >= -1e-5 && volume <= 1e-5) {
				return 0;
			}
			return Values::force_between(volume, -10.0, 10.0);
		}

		Matrix &set(size_t output, size_t input, double volume) {
			volume_[IndexPolicy::array(output, CHANNELS)][IndexPolicy::array(input, CHANNELS)] =
					validVolume(volume);
			return *this;
		}

		Matrix &setAll(double volume) {
			double v = validVolume(volume);

			for (size_t i = 0; i < CHANNELS; i++) {
				for (size_t j = 0; i < CHANNELS; i++) {
					volume_[i][j] = v;
				}
			}
			return *this;
		}

		Matrix &setGroup(size_t output, size_t input, double volume)
		{
			double v = validVolume(volume);

			size_t in = input * CHANNELS_PER_GROUP;
			size_t out = output * CHANNELS_PER_GROUP;
			for (size_t channel = 0 ; channel < CHANNELS_PER_GROUP; channel++) {
				volume_[out + channel][in + channel] = v;
			}
			return *this;
		}

		void approach(const Matrix &source, const IntegrationCoefficients<double> &coefficients)
		{
			for (size_t i = 0; i < CHANNELS; i++) {
				for (size_t j = 0; i < CHANNELS; i++) {
					coefficients.integrate(source.volume_[i][j], volume_[i][j]);
				}
			}
		}

		template<typename...A>
		void apply(const ArrayTraits<double, A...> &input, ArrayTraits<double, A...> &output, double noise)
		{
			for (size_t o = 0; o < CHANNELS; o++) {
				double sum = noise;
				for (size_t i = 0; i < CHANNELS; i++) {
					sum += volume_[o][i] * input[i];
				}
				output[o] = sum;
			}
		}
	};

	IntegrationCoefficients<double>	integration;
	Matrix userVolume;
	Matrix actualVolume;

	VolumeControl()
	{
		userVolume.setAll(0);
		actualVolume.setAll(0);
		integration.setCharacteristicSamples(96000 * 0.05);
	}

	void configure(double sampleRate, double rc, Matrix initialVolumes)
	{
		integration.setCharacteristicSamples(sampleRate * rc);
		userVolume = initialVolumes;
		actualVolume.setAll(0);
	}

	void setVolume(Matrix newVolumes)
	{
		userVolume = newVolumes;
	}

	template<typename...A>
	void apply(const ArrayTraits<double, A...> &input, ArrayTraits<double, A...> &output, double noise)
	{
		actualVolume.approach(userVolume, integration);
		actualVolume.apply(input, output, noise);
	}
};

template<size_t CHANNELS_PER_GROUP, size_t GROUPS, size_t CROSSOVERS>
class SignalGroup
{
public:
	static constexpr size_t INPUTS = GROUPS * CHANNELS_PER_GROUP;
	// bands are around crossovers
	static constexpr size_t BANDS = CROSSOVERS + 1;
	// multixplex by frequency bands
	static constexpr size_t CROSSOVER_OUPUTS = INPUTS * BANDS;
	// sub-woofer channels summed, so don't process CROSSOVER_OUPUTS channels
	static constexpr size_t PROCESSING_CHANNELS = 1 + CROSSOVERS * INPUTS;
	// RMS detection are per group, not per channel (and only one for sub)
	static constexpr size_t DETECTORS = 1 + CROSSOVERS * GROUPS;
	// Limiters are per group and sub
	static constexpr size_t LIMITERS = 1 + GROUPS;
	// OUTPUTS
	static constexpr size_t OUTPUTS = INPUTS + 1;

	using CrossoverFrequencies = FixedSizeArray<double, CROSSOVERS>;
	using ThresholdValues = FixedSizeArray<double, LIMITERS>;

private:
	PinkNoise::Default noise;
	FixedSizeArray<double, INPUTS> inputWithVolumeAndNoise;
	FixedSizeArray<double, PROCESSING_CHANNELS> processInput;
	FixedSizeArray<double, OUTPUTS> output;

	Crossovers::Filter<double, INPUTS, CROSSOVERS>  crossoverFilter;
	FixedSizeArray<double, DETECTORS> limiterThresholds;
	ACurves::Filter<double, PROCESSING_CHANNELS> aCurve;

	FixedSizeArray<double, BANDS> relativeBandWeights;
	FixedSizeArray<AdvancedRms::Detector<15>, DETECTORS> rmsDetector;
	FixedSizeArray<double, DETECTORS> detectorWeight;

	FixedSizeArray<HoldMaxDoubleIntegrated<double>, LIMITERS> limiter;
	Delay<double> rmsDelay;
	Delay<double> limiterDelay;

	double sampleRate_;
	using Volume = VolumeControl<CHANNELS_PER_GROUP, GROUPS>;
	using Matrix = typename Volume::Matrix;
	Volume volumeControl;

	static AdvancedRms::UserConfig rmsUserConfig()
	{
		return { 0.0005, 0.4, 0.5, 1.2 };
	}

public:
	SignalGroup() : rmsDelay(96000), limiterDelay(96000), sampleRate_(0)
	{

	}

	void setSampleRate(
			double sampleRate,
			const FixedSizeArray<double, CROSSOVERS> &crossovers,
			const FixedSizeArray<double, LIMITERS> &thresholds)
	{
		noise.setScale(1e-5);
		aCurve.setSampleRate(sampleRate);
		crossoverFilter.configure(sampleRate, crossovers);

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

		setThresholds(thresholds);
		Matrix matrix;
		for (size_t group = 0; group < GROUPS; group++) {
			matrix.setGroup(group, group, 1);
		}
		volumeControl.configure(sampleRate, 0.05, matrix);
		sampleRate_ = sampleRate;
	}
	void process(
			const FixedSizeArray<double, INPUTS> &input,
			FixedSizeArray<double, INPUTS + 1> &output)
	{
		volumeControl.apply(input, inputWithVolumeAndNoise, noise());
		moveToProcessingChannels(crossoverFilter.filter(inputWithVolumeAndNoise));
		processSubRms();
		processChannelsRms();
		mergeFrequencyBands();
		processSubLimiter();
		processChannelsLimiter();
	}

	void setThresholds(const FixedSizeArray<double, LIMITERS> &thresholds) {
		detectorWeight[0] = thresholds[0] * relativeBandWeights[0];
		for (size_t group = 0, i = 1; group < GROUPS; group++) {
			for (size_t band = 1; band <= CROSSOVERS; band++, i++) {
				detectorWeight[i] = thresholds[group] * relativeBandWeights[band];
			}
		}
		for (size_t i = 0; i < DETECTORS; i++) {
			rmsDetector[i].setValue(detectorWeight[i]);
		}
		for (size_t i = 0; i < LIMITERS; i++) {
			limiterThresholds[i] = Values::min(thresholds[i] / rmsUserConfig().peakWeight, 0.99);
			limiter[i].setValue(limiterThresholds[i]);
		}
	}

private:

	void moveToProcessingChannels(const FixedSizeArray<double, CROSSOVER_OUPUTS> &multi)
	{
		// Sum all lowest frequency bands
		processInput[0] = 0.0;
		for (size_t channel = 0; channel < INPUTS; channel++) {
			processInput[0] += multi[channel];
		}

		// copy rest of channels
		for (size_t i = 1, channel = INPUTS; i < processInput.size(); i++, channel++) {
			processInput[i] = multi[channel];
		}
	}

	void processSubRms()
	{
		double x = processInput[0];
		processInput[0] = rmsDelay.setAndGet(x);
		double weight = detectorWeight[0];
		double detect = rmsDetector[0].integrate(x * x, weight);
		double gain = weight / detect;
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
				double weight = detectorWeight[detector];
				double detect = rmsDetector[detector].integrate(squareSum, weight);
				double gain = weight / detect;
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
		double threshold = limiterThresholds[0];
		double x = output[0];
		output[0] = limiterDelay.setAndGet(x);
		double detect = limiter[0].applyWithMinimum(x, threshold);

		double gain = threshold / detect;
		output[0] *= Values::clamp(gain * output[0], -threshold, threshold);
	}

	void processChannelsLimiter()
	{
		for (size_t group = 1, channel = 1; group <= GROUPS; group++) {
			double threshold = limiterThresholds[group];
			size_t max = channel + CHANNELS_PER_GROUP;
			double peak = 0.0;
			for (size_t offset = channel; offset < max; offset++) {
				double x = output[offset];
				output[offset] = limiterDelay.setAndGet(x);
				peak = Values::max(peak, fabs(output[offset]));
			}
			double detect = limiter[group].applyWithMinimum(peak, threshold);
			double gain = threshold / detect;
			for (size_t offset = channel; offset < max; offset++) {
				output[offset] *= Values::clamp(gain * output[offset], -threshold, threshold);
			}
		}
	}


};


} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_SIGNALGROUP_GUARD_H_ */
