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
		static ValueRange<double> range(0.0005, 0.005);
		return range;
	}
	static ValueRange<double> &followHoldTimeRange()
	{
		static ValueRange<double> range(0.001, 0.015);
		return range;
	}

	struct UserConfig
	{
		double minRc;
		double maxRc;
		double peakWeight;
		double slowWeight;
//		double minRc = 0.001;
//		double maxRc = 0.400;
//		double peakWeight = 0.5;
//		double slowWeight = 1.5;

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

	template<size_t RC_TIMES>
	struct RuntimeConfig
	{
		FixedSizeArray<size_t, RC_TIMES> characteristicSamples;
		FixedSizeArray<double, RC_TIMES> scale;
		size_t followCharacteristicSamples;
		size_t followHoldSamples;

		void calculate(UserConfig userConfig, double sampleRate)
		{
			UserConfig config = userConfig.validate();
			followCharacteristicSamples = 0.5 + sampleRate * followRcRange().getBetween(config.minRc / 2);
			followHoldSamples = 0.5 + sampleRate * followHoldTimeRange().getBetween(config.minRc * 2);

			double fastScalePower = log(config.peakWeight) / log(config.minRc / PERCEPTIVE_FAST_WINDOWSIZE);
			double slowScalePower = log(config.slowWeight) / log(config.maxRc / PERCEPTIVE_FAST_WINDOWSIZE);

			double ratioIncrement = 1.0 / (RC_TIMES - 1);
			for (size_t i = 0; i < RC_TIMES; i++) {
				double rc = config.maxRc * pow(config.minRc / config.maxRc, ratioIncrement * i);
				characteristicSamples[i] = 0.5 + sampleRate * rc;
				double rcForScale = Value<double>::min(PERCEPTIVE_SLOW_WINDOWSIZE, rc);
				double rcPowerForScale = rc > PERCEPTIVE_FAST_WINDOWSIZE ? slowScalePower : fastScalePower;
				scale[i] = pow(rcForScale / PERCEPTIVE_FAST_WINDOWSIZE, rcPowerForScale);
				std::cout << "RC " << (1000 * characteristicSamples[i] / sampleRate) << " ms. level=" << scale[i] << std::endl;
			}
		}
	};

	template<size_t RC_TIMES>
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
			DefaultRms<double> integrator;
			double scale;
		};
		FixedSizeArray<Filter, RC_TIMES> filters_;
		SmoothHoldMaxAttackRelease<double> follower_;

	public:
		Detector() : follower_(1, 1, 1, 1) {}

		void userConfigure(UserConfig userConfig, double sampleRate)
		{
			RuntimeConfig<RC_TIMES> runtimeConfig;
			runtimeConfig.calculate(userConfig, sampleRate);
			configure(runtimeConfig);
		}

		void configure(RuntimeConfig<RC_TIMES> config)
		{
			follower_ = SmoothHoldMaxAttackRelease<double>(
					config.followHoldSamples,
					config.followCharacteristicSamples,
					config.followCharacteristicSamples,
					0);
			for (size_t i = 0; i < RC_TIMES; i++)
			{
				filters_[i].integrator.setWindowSize(config.characteristicSamples[i]);
				filters_[i].scale = config.scale[i];
			}
		}

		void setValue(double x)
		{
			follower_.setValue(x);
		}

		double integrate(double squareInput, double minOutput)
		{
			double value = minOutput;
			for (size_t i = 0; i < filters_.size(); i++) {
				double x = filters_[i].integrator.addSquareCompareAndGet(
						squareInput, value);
				x *= filters_[i].scale;
				value = Value<double>::max(value, x);
			}

			return follower_.apply(value);
		}
	};

};


} /* End of name space tdap */

#endif /* TDAP_ADVANCED_RMS_DETECTOR_HEADER_GUARD */
