/*
 * Dynamics.hpp
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

#ifndef SMS_SPEAKERMAN_DYNAMICS_GUARD_H_
#define SMS_SPEAKERMAN_DYNAMICS_GUARD_H_

#include <type_traits>
#include <simpledsp/Butterfly.hpp>
#include <simpledsp/CharacteristicSamples.hpp>
#include <simpledsp/IirFixed.hpp>
#include <simpledsp/Size.hpp>
#include <simpledsp/Types.hpp>
#include <simpledsp/TimeMeasurement.hpp>
#include <simpledsp/Values.hpp>
#include <simpledsp/Vector.hpp>

namespace speakerman {

using namespace simpledsp;


template <typename Sample, size_t CROSSOVERS, size_t ORDER, size_t ALLPASS_RC_TIMES, size_t BAND_RC_TIMES>
struct Dynamics
{
	static_assert(is_floating_point<Sample>::value, "Sample should be a floating-point type");

	static constexpr size_t BANDS = CROSSOVERS + 1;
	static constexpr Sample MINIMUM_THRESHOLD = 0.01;
	static constexpr Sample MAXIMUM_THRESHOLD = 1.0;
	static const Sample clampedThreshold(const Sample threshold) {
		return max(MINIMUM_THRESHOLD, min(MAXIMUM_THRESHOLD, threshold));
	}


	/**
	 * Describes the configuration that the user wants to have.
	 * Times are given in seconds and frequencies in Hertz.
	 * After a call to makeConfig with the proper sample
	 * frequency, the Conifg object is written to.
	 */
	struct UserConfig
	{
		Sample threshold;

		// Amount of energy allowed per frequency band, relative to threshold
		ArrayVector<Sample, BANDS> bandThreshold;

		// Crossover frequencies
		ArrayVector<double, CROSSOVERS> frequencies;
		// Characteristic times for all-pass slow detection
		ArrayVector<double, ALLPASS_RC_TIMES> allPassRcs;
		// Characteristic times for fast and per band detection
		ArrayVector<double, ALLPASS_RC_TIMES> bandRcs;

		bool seperateSubChannel = true;
	};

	struct Coeff {
		double frequency = 0;
		Iir::Fixed::Coefficients<Sample, ORDER> highPass;
		Iir::Fixed::Coefficients<Sample, ORDER> lowPass;
	};

	/**
	 * Configuration used by the actual processing step. The
	 * Dynamics contains two configurations: one to
	 * write and one to read and use. A LockFreeCOnsumer
	 * is used to make sure memory visibility and the fact that
	 * the consumer, that actually processes using the configuration,
	 * never sees a half-written configuration.
	 */
	struct Config
	{
		// Last know sample rate, used for reconfigure.
		double sampleRate = 0;
		// Filter coefficients for each crossover.
		ArrayVector<array<Sample, ORDER>, 3> a;
		ArrayVector<Coeff, CROSSOVERS> coeffs;
		bool updatedFiltersCoefficients = true;

		// multiplier for band detection value, so it can be compared to a threshold of 1
		// all values are detected as squared (to postpone square root processing to minimum)
		ArrayVector<Sample, BANDS> bandMultiplier;

		// Characteristic times for all-pass slow  detection
		ArrayVector<CharacteristicSamples, ALLPASS_RC_TIMES> allPassRcs;
		// Characteristic times for fast and per band detection
		ArrayVector<CharacteristicSamples, BAND_RC_TIMES> bandRcs;

		Sample thresholdMultiplier = 1.0;
		// Keying filters, that are applied to the "all-pass" detection
		// This simulates a kind-of ear-curve with some practical twists

		struct Key {
			Iir::Fixed::Coefficients<Sample, 1> loCut;
			Iir::Fixed::Coefficients<Sample, 2> midBoost;
			Iir::Fixed::Coefficients<Sample, 1> hiCut;
		} keying;

		// Determines the speed at which changes in, say, threshold or other stuff are followed;
		CharacteristicSamples valueRc;

		bool seperateSubChannel = true;

		void configure(const UserConfig &userConfig, double currentSampleRate)
		{
			Iir::CoefficientsBuilder builder(1, false);

			cout << "Applying user configuration" << endl;

			updatedFiltersCoefficients = false;
			cout << "- Crossovers:" << endl;
			for (size_t crossover = 0; crossover < CROSSOVERS; crossover++) {
				Coeff &coeff = coeffs[crossover];
				builder.setOrder(coeff.highPass.order());


				double f = Values<double>::clamp(userConfig.frequencies(crossover), 40, 16000);

				cout << "  - Configure crossover " << crossover << " @ " << f << "Hz." << endl;

				updatedFiltersCoefficients |= f != coeff.frequency;
				coeff.frequency = f;

				Butterworth::createCoefficients(builder, currentSampleRate, f, Butterworth::Pass::HIGH, true);
				coeff.highPass.assign(builder);

				Butterworth::createCoefficients(builder, currentSampleRate, f, Butterworth::Pass::LOW, true);
				coeff.lowPass.assign(builder);
			}

			cout << "  Frequencies changed: " << updatedFiltersCoefficients  << endl;

			Sample threshold = clampedThreshold(userConfig.threshold);
			thresholdMultiplier = 1.0 / (threshold * threshold);

			cout << "- Threshold:" << endl << "  - RMS-value: " << threshold << endl << "  - Multiplier: " << thresholdMultiplier << endl;

			cout << "- RCs for keyed full-bandwidth follower:" << endl;
			for (size_t rc = 0; rc < ALLPASS_RC_TIMES; rc++) {
				allPassRcs[rc].setCharacteristicSamples(currentSampleRate * userConfig.allPassRcs(rc));

				cout << "  - rc[" << rc << "] t: " << userConfig.allPassRcs(rc) << "; i: " <<
						allPassRcs[rc].inputMultiplier() << "; h: " << allPassRcs[rc].historyMultiplier() << endl;
			}

			cout << "- Keying filters: " << endl
					<< "  - 180 Hz. 1st order high pass" << endl
					<< "  - 2500 Hz. 2nd order high pass (add-up)" << endl
					<< "  - 5000 Hz. 1st order low pass" << endl;

			builder.setOrder(keying.loCut.order());
			Butterworth::createCoefficients(builder, currentSampleRate, 180, Butterworth::Pass::HIGH, true);
			keying.loCut.assign(builder);

			builder.setOrder(keying.midBoost.order());
			Butterworth::createCoefficients(builder, currentSampleRate, 2500, Butterworth::Pass::HIGH, true);
			keying.midBoost.assign(builder);

			builder.setOrder(keying.hiCut.order());
			Butterworth::createCoefficients(builder, currentSampleRate, 5000, Butterworth::Pass::LOW, true);
			keying.hiCut.assign(builder);

			cout << "- Per-band energy levels:" << endl;
			for (size_t band = 0; band < BANDS; band++) {
				Sample bandEnergy = clampedThreshold(userConfig.bandThreshold(band));
				bandMultiplier[band] = thresholdMultiplier / (bandEnergy * bandEnergy);
				cout << "  - band[" << band << "] level: fraction of threshold: " << bandEnergy << "; multiplier: " << bandMultiplier[band] << endl;
			}

			cout << "- RCs for each frequency band:" << endl;
			for (size_t rc = 0; rc < BAND_RC_TIMES; rc++) {
				bandRcs[rc].setCharacteristicSamples(currentSampleRate * userConfig.bandRcs(rc));

				cout << "  - rc[" << rc << "] t: " << userConfig.bandRcs(rc) << "; i: " <<
						bandRcs[rc].inputMultiplier() << "; h: " << bandRcs[rc].historyMultiplier() << endl;
			}

			valueRc.setCharacteristicSamples(0.1 * currentSampleRate);

			seperateSubChannel = userConfig.seperateSubChannel;
			if (seperateSubChannel) {
				cout << "- Separating sub woofer channel" << endl;
			}
			sampleRate = currentSampleRate;
		}

		void reconfigure(const UserConfig &userConfig)
		{
			configure(sampleRate);
		}

	};

	template<size_t CHANNELS>
	struct Processor
	{
		Config &conf;
		// used for butterfly crossover filtering
		const ArrayVector<size_t, 3 * CROSSOVERS> ioPlan;

		ArrayVector<Iir::Fixed::History<Sample, ORDER>, 4 * CHANNELS * CROSSOVERS> bandPassHistory;
		struct Keying {
			Iir::Fixed::MultiFixedChannelFilter<Sample, Sample, 1, CHANNELS> loCut;
			Iir::Fixed::MultiFixedChannelFilter<Sample, Sample, 2, CHANNELS> mdBoost;
			Iir::Fixed::MultiFixedChannelFilter<Sample, Sample, 1, CHANNELS> hiCut;
		}
		keying;

		ArrayVector<Sample, ALLPASS_RC_TIMES> allPassIntegrated;
		ArrayVector<ArrayVector<Sample, BAND_RC_TIMES>, BANDS> bandIntegrated;

		Sample thresholdMultiplier = 1.0;
		ArrayVector<Sample, BANDS> bandMultiplier;
		ArrayVector<Sample, CHANNELS> input;
		ArrayVector<Sample, CHANNELS> output;
		ArrayVector<Sample, CHANNELS> subout;
		ArrayVector<ArrayVector<Sample, CHANNELS>, BANDS> bands;

		void clearHistory()
		{
			for (size_t i = 0; i < bandPassHistory.length(); i++) {
				bandPassHistory[i].clear();
			}
		}

		void init()
		{
			clearHistory();
			thresholdMultiplier = 1.0;
			bandMultiplier.fill(1.0);
			input.zero();
			output.zero();
			bands.zero();
		}

		// Execute for each block of samples
		void checkFilterChanges()
		{
			if (conf.updatedFiltersCoefficients) {
				keying.loCut.setCoefficients(conf.keying.loCut);
				keying.mdBoost.setCoefficients(conf.keying.midBoost);
				keying.hiCut.setCoefficients(conf.keying.hiCut);
				conf.updatedFiltersCoefficients = false;
			}
		}

		// Execute for each frame
		void applyValueIntegration() {
			thresholdMultiplier = conf.valueRc.integrate(conf.thresholdMultiplier, thresholdMultiplier);
			for (size_t band = 0; band < BANDS; band++) {
				bandMultiplier[band] = conf.valueRc.integrate(conf.bandMultiplier[band], bandMultiplier[band]);
			}
		}

		Sample getAllPassDetection()
		{
			Sample keyedSquareSum = 0.0;
			Sample squareSum = 0.0;
			for (size_t channel = 0; channel < CHANNELS; channel++)	{
				Sample x = input[channel];
				squareSum += x * x;
				Sample lowCut = keying.hiCut.filter(channel, x) ;
				Sample boostMid = lowCut + 1.4 * keying.mdBoost.filter(channel, lowCut);
				Sample keyed = keying.hiCut.filter(channel, boostMid);
				keyedSquareSum += keyed * keyed;
			}

			keyedSquareSum *= thresholdMultiplier;
			Sample maxIntegratedValue = 0.0;

			// Integrate with all characteristic times, except the fastest one, which
			// is used for smoothing.
			for (size_t i = 1; i < ALLPASS_RC_TIMES; i++) {
				Sample integrated = conf.allPassRcs[i].integrate(keyedSquareSum, allPassIntegrated[i]);
				allPassIntegrated[i] = integrated;
				if (integrated > maxIntegratedValue) {
					maxIntegratedValue = integrated;
				}
			}
			// Don't take any action when all-pass is below threshold
			maxIntegratedValue = max(1.0, maxIntegratedValue);

			// Integrate for smooth operation
			Sample smoothed = conf.allPassRcs[0].integrate(maxIntegratedValue, allPassIntegrated[0]);
			allPassIntegrated[0] = smoothed;

			return smoothed;
		}

		void splitFrequencyBands()
		{
			size_t inputIdx = ioPlan(0);

			for (size_t channel = 0; channel < CHANNELS; channel++) {
				bands[inputIdx][channel] = input(channel);
			}

			for (size_t crossover = 0, ioIndex = 0, historyIndex = 0; crossover < CROSSOVERS; crossover++) {
				inputIdx = ioPlan(ioIndex++);
				size_t output1Idx = ioPlan(ioIndex++);
				size_t output2Idx = ioPlan(ioIndex++);

				const Coeff &coeff = conf.coeffs(inputIdx);

				for (size_t channel = 0; channel < CHANNELS; channel++) {
					Sample x = bands[inputIdx][channel];

					Sample hi = Iir::Fixed::filter(coeff.highPass, bandPassHistory[historyIndex++], x);
					hi = Iir::Fixed::filter(coeff.highPass, bandPassHistory[historyIndex++], hi);

					Sample lo = Iir::Fixed::filter(coeff.lowPass, bandPassHistory[historyIndex++], x);
					lo = Iir::Fixed::filter(coeff.lowPass, bandPassHistory[historyIndex++], lo);

					bands[output1Idx][channel] = hi;
					bands[output2Idx][channel] = lo;
				}
			}
		}

		void processFrequencyBands(Sample allPassDetection)
		{
			for (size_t band = 0; band < BANDS; band++) {
				Sample squareSum = 0.0;
				for (size_t channel = 0; channel < CHANNELS; channel++) {
					Sample x = bands[band][channel];
					squareSum += x * x;
				}

				Sample maxIntegrated = 0.0;
				for (size_t rc = 1; rc < BAND_RC_TIMES; rc++) {
					Sample &history = bandIntegrated[band][rc];
					Sample integrated = conf.bandRcs[rc].integrate(squareSum, history);
					history = integrated;
					if (integrated > maxIntegrated) {
						maxIntegrated = integrated;
					}
				}
				// multiply for threshold = 1 and compare to the allPassdetection level
				Sample detection = max(allPassDetection, conf.bandMultiplier[band] * maxIntegrated);
				Sample &history = bandIntegrated[band][0];
				Sample smoothed = conf.bandRcs[0].integrate(detection, history);
				history = smoothed;

				Sample multiplication = smoothed > 1.0 ? 1.0 / sqrt(smoothed) : 1.0;

				for (size_t channel = 0; channel < CHANNELS; channel++) {
					bands[band][channel] *= multiplication;
				}
			}
		}

		void sumFrequencyBands()
		{
			size_t start;
			if (conf.seperateSubChannel) {
				start = 1;
				for (size_t channel = 0; channel < CHANNELS; channel++) {
					subout[channel] = bands[0][channel];
				}
			}
			else {
				start = 0;
				subout.zero();
			}
			for (size_t channel = 0; channel < CHANNELS; channel++) {
				Sample sumOfFrequencyBands = 0.0;
				for (size_t band = start; band < BANDS; band++) {
					sumOfFrequencyBands += bands[band][channel];
				}
				output[channel] = sumOfFrequencyBands;
			}
		}

		void process()
		{
			applyValueIntegration();
			splitFrequencyBands();
			processFrequencyBands(getAllPassDetection());
			sumFrequencyBands();
		}

		Processor(Config &config) :
			conf(config),
			ioPlan(createFilterPlan())
		{

		}
	};

	// Utility methods
	static const ArrayVector<size_t, 3 * CROSSOVERS> createFilterPlan()
	{
		ButterflyPlan plan(CROSSOVERS);
		ArrayVector<size_t, 3 * CROSSOVERS> result;

		for (size_t crossover = 0, node = 0; crossover < CROSSOVERS; crossover++) {
			result[node++] = plan.input(crossover);
			result[node++] = plan.output1(crossover);
			result[node++] = plan.output2(crossover);
		}

		return result;
	}
};


} /* End of namespace speakerman */


/**
 		ArrayVector<accurate_t, 4> lowSub;
		ArrayVector<accurate_t, 4> low;
		ArrayVector<accurate_t, 4> midLow;
		ArrayVector<accurate_t, 4> midHigh;
		ArrayVector<accurate_t, 4> high;

		if (consumer.consume(true)) {
			hf0.setCoefficients(rConf.high0);
			lf0.setCoefficients(rConf.low0);
			hf1.setCoefficients(rConf.high1);
			lf1.setCoefficients(rConf.low1);
			hf2.setCoefficients(rConf.high2);
			lf2.setCoefficients(rConf.low2);
			hf3.setCoefficients(rConf.high3);
			lf3.setCoefficients(rConf.low3);
			cout << "RC samples " << rConf.rmsTimeConfig1.rct.characteristicSamples() << endl;
		}

		for (size_t frame = 0; frame < frameCount; frame++) {
			samples[0] = *inputLeft1++;
			samples[1] = *inputRight1++;
			samples[2] = *inputLeft2++;
			samples[3] = *inputRight2++;

			for (size_t i = 0, j = 0; i < 4; i++, j += 2) {
				simpledsp::accurate_t x = samples[i];

				accurate_t l2 = lf2.filter(j + 1, lf2.filter(j, x)); //NO
				accurate_t h2 = hf2.filter(j + 1, hf2.filter(j, x)); // NO

				accurate_t l3 = lf3.filter(j + 1, lf3.filter(j, h2));
				accurate_t h3 = hf3.filter(j + 1, hf3.filter(j, h2));

				accurate_t l1 = lf1.filter(j + 1, lf1.filter(j, l2));
				accurate_t h1 = hf1.filter(j + 1, hf1.filter(j, l2));

				low[i] = l1;
				midLow[i] = h1;
				midHigh[i] = l3;
				high[i] = h3;
			}

			// process
			for (size_t i = 0; i < 4; i++) {
				// square detection
				accurate_t l = low.squaredMagnitude();
				accurate_t ml = midLow.squaredMagnitude();
				accurate_t mh = midHigh.squaredMagnitude();
				accurate_t h = high.squaredMagnitude();

				// integration
				historyValue2[1] = rConf.rmsTimeConfig2.rct.integrate(rConf.rmsTimeConfig2.scale * l, historyValue2[1]);
				historyValue2[2] = rConf.rmsTimeConfig2.rct.integrate(rConf.rmsTimeConfig2.scale * ml, historyValue2[2]);
				historyValue2[3] = rConf.rmsTimeConfig2.rct.integrate(rConf.rmsTimeConfig2.scale * mh, historyValue2[3]);
				historyValue2[4] = rConf.rmsTimeConfig2.rct.integrate(rConf.rmsTimeConfig2.scale * h, historyValue2[4]);

				l = historyValue3[1] = sqrt(historyValue2[1]);
				ml = historyValue3[2] = sqrt(historyValue2[2]);
				mh = historyValue3[3] = sqrt(historyValue2[3]);
				h = historyValue3[4] = sqrt(historyValue2[4]);

				// Limit level
				l = historyValueN[1] = rConf.rmsTimeConfig1.rct.integrate(l <= rConf.lowLevel ? 1 : rConf.lowLevel / l, historyValueN[1]);
				ml = historyValueN[2] = rConf.rmsTimeConfig1.rct.integrate(ml <= rConf.midLowLevel ? 1 : rConf.midLowLevel / ml, historyValueN[2]);
				mh = historyValueN[3] = rConf.rmsTimeConfig1.rct.integrate(mh <= rConf.midHighLevel ? 1 : rConf.midHighLevel / mh, historyValueN[3]);
				h = historyValueN[4] = rConf.rmsTimeConfig1.rct.integrate(h <= rConf.highLevel ? 1 : rConf.highLevel / h, historyValueN[4]);

				low.multiply(l);
				midLow.multiply(ml);
				midHigh.multiply(mh);
				high.multiply(h);
			}
			// Sum up
			accurate_t sub = 0.0;
			for (size_t i = 0, j = 0; i < 4; i++, j += 2) {
				accurate_t limited = low[i] + midLow[i] + midHigh[i] + high[i];
				samples[i] =  hf0.filter(j + 1, hf0.filter(j, limited));
				sub += lf0.filter(j + 1, lf0.filter(j, limited));
			}
			accurate_t subSquare = sub * sub;
			historyValue2[0] = rConf.rmsTimeConfig2.rct.integrate(subSquare, historyValue2[0]);
			accurate_t ls = historyValue3[0] = sqrt(historyValue2[0]);
			ls = historyValueN[0] = rConf.rmsTimeConfig1.rct.integrate(ls <= rConf.lowSubLevel ? 1 : rConf.lowSubLevel / ls, historyValueN[0]);

			sub *= ls;

 */

#endif /* SMS_SPEAKERMAN_DYNAMICS_GUARD_H_ */
