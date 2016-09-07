/*
 * tdap/AdvancedRmsDetector.hpp
 *
 * Part of TdAP
 * Time-domain Audio Processing
 * Copyright (C) 2015-2016 Michel Fleur.
 * Source https://bitbucket.org/emmef/tdap
 * Email  tdap@emmef.org
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

#ifndef TDAP_ADVANCED_RMS_DETECTOR_HEADER_GUARD
#define TDAP_ADVANCED_RMS_DETECTOR_HEADER_GUARD

#include <type_traits>
#include <cmath>
#include <limits>

#include <tdap/Integration.hpp>
#include <tdap/Followers.hpp>
#include <tdap/Rms.hpp>

namespace tdap {

struct AdvancedRms
{
	static constexpr double PERCEPTIVE_FAST_WINDOWSIZE = 0.050;
	static constexpr double PERCEPTIVE_SLOW_WINDOWSIZE = 0.400;
	static constexpr double MAX_BUCKET_INTEGRATION_TIME = 0.025;

	static ValueRange<double> &peakWeightRange()
	{
		static ValueRange<double> range(0.25, 1.0);
		return range;
	}
	static ValueRange<double> &slowWeightRange()
	{
		static ValueRange<double> range(0.5, 2.0);
		return range;
	}
	static ValueRange<double> &minRcRange()
	{
		static ValueRange<double> range(0.0002, 0.02);
		return range;
	}
	static ValueRange<double> &maxRcRange()
	{
		static ValueRange<double> range(0.100, 4.0);
		return range;
	}
	static ValueRange<double> &followRcRange()
	{
		static ValueRange<double> range(0.0005, 0.025);
		return range;
	}
	static ValueRange<double> &followHoldTimeRange()
	{
		static ValueRange<double> range(0.001, 0.050);
		return range;
	}

	struct UserConfig
	{
		double minRc;
		double maxRc;
		double peakWeight;
		double slowWeight;

		UserConfig validate()
		{
			return {
						minRcRange().getBetween(Value<double>::min(minRc, maxRc / 2)),
						maxRcRange().getBetween(Value<double>::max(maxRc, minRc * 2)),
						peakWeightRange().getBetween(peakWeight),
						slowWeightRange().getBetween(slowWeight)
			};
		}

		UserConfig standard()
		{
			return { 0.0005, 0.4, 0.5, 1.5 };
		}
	};

	template<typename T, size_t RC_TIMES>
	struct RuntimeConfig
	{
		static_assert(is_floating_point<T>::value, "Need floating point type");
		static_assert(RC_TIMES >= 4, "Need at least four characteristic times");
		FixedSizeArray<size_t, RC_TIMES> characteristicSamples;
		FixedSizeArray<T, RC_TIMES> scale;
		FixedSizeArray<T, RC_TIMES> minimumScale;
		size_t maxBucketIntegrationWindow;
		size_t followCharacteristicSamples;
		size_t followHoldSamples;
		size_t fastPerceptiveIdx_;

		void calculate(UserConfig userConfig, double sampleRate)
		{
			UserConfig config = userConfig.validate();
			followCharacteristicSamples = 0.5 + sampleRate * followRcRange().getBetween(config.minRc / 2);
			followHoldSamples = 0.5 + sampleRate * PERCEPTIVE_FAST_WINDOWSIZE;//().getBetween(config.minRc * 2);
			maxBucketIntegrationWindow = 0.5 + sampleRate * MAX_BUCKET_INTEGRATION_TIME;

			double perceptiveSlowToFast = log(PERCEPTIVE_SLOW_WINDOWSIZE / PERCEPTIVE_FAST_WINDOWSIZE);
			double perceptiveFastToPeak = log(PERCEPTIVE_FAST_WINDOWSIZE / config.minRc);
			size_t x, y, z;
			
			if (config.maxRc <= 0.45) {
				double logSum = perceptiveSlowToFast + perceptiveFastToPeak;
				x = Value<size_t>::max(1, perceptiveFastToPeak * RC_TIMES / logSum);
				y = Value<size_t>::max(2, perceptiveSlowToFast * RC_TIMES / logSum);
				x = RC_TIMES - y;
				z = 0;
			}
			else {
				double maxToPerceptiveSlow = log(config.maxRc / PERCEPTIVE_SLOW_WINDOWSIZE);
				double logSum = maxToPerceptiveSlow + perceptiveSlowToFast + perceptiveFastToPeak;
				x = Value<size_t>::max(1, perceptiveFastToPeak * RC_TIMES / logSum);
				y = Value<size_t>::max(2, perceptiveSlowToFast * RC_TIMES / logSum);
				z = Value<size_t>::max(1, maxToPerceptiveSlow * RC_TIMES / logSum);
				if (x + y + z < RC_TIMES) {
					x = RC_TIMES - y - z;
				}
				else {
					while (x + y + z > RC_TIMES) {
						if (z > 1) {
							z--;
						}
						else if (y > 2) {
							y--;
						}
						else if (x > 1) {
							x--;
						}
						else {
							throw std::runtime_error("Invalid number of RC-S");
						}
					}
				}
			}
			for (size_t i = 0; i < z; i++) {
				double t = 1.0 * (z - i) / z;
				double rc = PERCEPTIVE_SLOW_WINDOWSIZE * exp(log(config.maxRc / PERCEPTIVE_SLOW_WINDOWSIZE) * t);
				double sc = exp(log(PERCEPTIVE_FAST_WINDOWSIZE / rc) * 0.25);
				characteristicSamples[i] = 0.5 + rc * sampleRate;
				scale[i] = sc;
				minimumScale[i] = 1.0 / (sc * sc);
			}
			for (size_t i = 0; i < y; i++) {
				double t = 1.0 * (y - 1 - i) / (y - 1);
				double rc = PERCEPTIVE_FAST_WINDOWSIZE * exp(log(PERCEPTIVE_SLOW_WINDOWSIZE / PERCEPTIVE_FAST_WINDOWSIZE) * t);
				double sc = exp(log(rc / PERCEPTIVE_FAST_WINDOWSIZE) * 0.15);
				characteristicSamples[z + i] = 0.5 + rc * sampleRate;
				scale[z + i] = sc;
				minimumScale[z + i] = 1.0 / (sc * sc);
			}
			for (size_t i = 0; i < x; i++) {
				double t = 1.0 * (x - 1 - i) / x;
				double rc = config.minRc * exp(log(PERCEPTIVE_FAST_WINDOWSIZE / config.minRc) * t);
				double sc = exp(log(rc / PERCEPTIVE_FAST_WINDOWSIZE) * 0.25);
				characteristicSamples[z + y + i] = 0.5 + rc * sampleRate;
				scale[z + y  + i] = sc;
				minimumScale[z + y + i] = 1.0 / sc;
			}
			fastPerceptiveIdx_ = z + y;
			for (size_t i = 0; i < RC_TIMES; i++) {
				std::cout << "RC " << (1000 * characteristicSamples[i] / sampleRate) << " ms. level=" << scale[i] << "; min-lvl=" << minimumScale[i] << std::endl;
			}
		}
	};

	template<typename T, size_t RC_TIMES>
	class Detector
	{
		static ValueRange<double> &peakWeightRange()
		{
			static ValueRange<double> range(0.25, 1.0);
			return range;
		}
		static ValueRange<double> &minRcRange()
		{
			static ValueRange<double> range(0.0002, 0.02);
			return range;
		}
		static ValueRange<double> &maxRcRange()
		{
			static ValueRange<double> range(0.05, 4.0);
			return range;
		}
		static ValueRange<double> &followRcRange()
		{
			static ValueRange<double> range(0.001, 0.010);
			return range;
		}
		struct Filter
		{
			BucketIntegratedRms<double, 16> integrator;
			double scale;
		};
		FixedSizeArray<Filter, RC_TIMES> filters_;
		SmoothHoldMaxAttackRelease<T> follower_;
		FixedSizeArray<double, RC_TIMES> minimumScale;
		size_t fastPerceptiveIdx_;

	public:
		Detector() : follower_(1, 1, 1, 1) {
			for (size_t i = 0; i <  RC_TIMES; i++) {
				minimumScale[i] = 1;
			}
		}

		void userConfigure(UserConfig userConfig, double sampleRate)
		{
			RuntimeConfig<T, RC_TIMES> runtimeConfig;
			runtimeConfig.calculate(userConfig, sampleRate);
			configure(runtimeConfig);
		}

		void configure(RuntimeConfig<T, RC_TIMES> config)
		{
			follower_ = SmoothHoldMaxAttackRelease<T>(
					config.followHoldSamples,
					config.followCharacteristicSamples,
					config.followHoldSamples,
					0);
			fastPerceptiveIdx_ = config.fastPerceptiveIdx_;
			for (size_t i = 0; i < RC_TIMES; i++)
			{
				size_t windowSize = config.characteristicSamples[i];
				filters_[i].integrator.setWindowSizeAndRc(windowSize, Values::min(windowSize / 4, config.maxBucketIntegrationWindow));
				T scale = config.scale[i];
				filters_[i].scale = scale;
				minimumScale[i] = config.minimumScale[i];
			}
		}

		void setValue(T x)
		{
			follower_.setValue(x);
		}

//		T integrate(T squareInput, T minOutput)
//		{
//			T value = minOutput * minOutputScale_;
//			for (size_t i = 0; i < filters_.size(); i++) {
//				T x = filters_[i].integrator.addSquareCompareAndGet(
//						squareInput, value);
//				x *= filters_[i].scale;
//				value = Value<T>::max(value, x);
//			}
//
//			return follower_.apply(value);
//		}

		T integrate_smooth(T squareInput, T minOutput)
		{
			T value = 0;
			size_t i;
			for (i = 0; i < fastPerceptiveIdx_; i++) {
				T min = minOutput * minimumScale[i];
				T x = filters_[i].integrator.addSquareAndGetFastAttackWithMinimum(squareInput, min);
				x *= filters_[i].scale;
				value = Value<T>::max(value, x);
			}
			for (; i < filters_.size(); i++) {
				T min = minOutput * minimumScale[i];
				T x = filters_[i].integrator.addSquareCompareAndGet(squareInput, min);
				x *= filters_[i].scale;
				value = Value<T>::max(value, x);
			}

			return follower_.apply(value);
		}
	};

};


} /* End of name space tdap */

#endif /* TDAP_ADVANCED_RMS_DETECTOR_HEADER_GUARD */
