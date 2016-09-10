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

	template<typename T>
	struct RuntimeConfig
	{
		static_assert(is_floating_point<T>::value, "Need floating point type");
		static constexpr size_t RC_TIMES = 10;
		size_t smallWindowSamples;
		FixedSizeArray<T, RC_TIMES> scale;
		size_t followCharacteristicSamples;
		size_t followHoldSamples;
		size_t trueRmsLevels;

		void calculate(UserConfig userConfig, double sampleRate)
		{
			UserConfig config = userConfig.validate();
			followCharacteristicSamples = 0.5 + sampleRate * followRcRange().getBetween(config.minRc / 2);
			followHoldSamples = 0.5 + sampleRate * PERCEPTIVE_FAST_WINDOWSIZE;//().getBetween(config.minRc * 2);
			double largeRc = 2 * PERCEPTIVE_SLOW_WINDOWSIZE;
			double smallRc = largeRc / pow(2, RC_TIMES - 1);
			smallWindowSamples = smallRc *  sampleRate;
			size_t i = 0;
			double rc = smallRc;
			while (Values::relative_distance(rc, PERCEPTIVE_FAST_WINDOWSIZE) > 0.5) {
				scale[i++] = pow(rc / PERCEPTIVE_FAST_WINDOWSIZE, 0.25);
				rc *= 2.0;
			}
			scale[i++] = 1.0;
			trueRmsLevels = i;
			rc *= 2.0;
			while (Values::relative_distance(rc, largeRc) > 0.5) {
				scale[i++] = pow(rc / PERCEPTIVE_FAST_WINDOWSIZE, 0.15);
				rc *= 2.0;
			}
			while (i < RC_TIMES) {
				scale[i++] = pow(PERCEPTIVE_FAST_WINDOWSIZE / rc, 0.25);
				rc *= 2.0;
			}
//			rc = smallRc;
//			for (size_t i = 0; i < RC_TIMES; i++, rc *= 2.0) {
//				std::cout << "RC " << (1000 * rc) << " ms. level=" << scale[i] << std::endl;
//			}
		}
	};

	template<typename T>
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
		MultiRcRms<T, (size_t)16, RuntimeConfig<T>::RC_TIMES> filter_;
		SmoothHoldMaxAttackRelease<T> follower_;

	public:
		Detector() : follower_(1, 1, 1, 1) {
		}

		void userConfigure(UserConfig userConfig, double sampleRate)
		{
			RuntimeConfig<T> runtimeConfig;
			runtimeConfig.calculate(userConfig, sampleRate);
			configure(runtimeConfig);
		}

		void configure(RuntimeConfig<T> config)
		{
			follower_ = SmoothHoldMaxAttackRelease<T>(
					config.followHoldSamples,
					config.followCharacteristicSamples,
					config.followHoldSamples,
					0);
			filter_.configure_true_levels(config.trueRmsLevels);
			filter_.setSmallWindow(config.smallWindowSamples);
			filter_.setIntegrators(0.01);
			for (size_t i = 0; i < RuntimeConfig<T>::RC_TIMES; i++) {
				filter_.set_scale(i, config.scale[i]);
			}
			filter_.setValue(10);
			follower_.setValue(10);
		}

		void setValue(T x)
		{
			follower_.setValue(x);
		}

		T integrate_smooth(T squareInput, T minOutput)
		{
			T value = filter_.addSquareGetValue(squareInput, minOutput);
			return follower_.apply(value);
		}
	};

};


} /* End of name space tdap */

#endif /* TDAP_ADVANCED_RMS_DETECTOR_HEADER_GUARD */
