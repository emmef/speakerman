/*
 * SpeakermanConfig.hpp
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

#ifndef SMS_SPEAKERMAN_SPEAKERMAN_RUNTIME_CONFIG_GUARD_H_
#define SMS_SPEAKERMAN_SPEAKERMAN_RUNTIME_CONFIG_GUARD_H_

#include <tdap/IirBiquad.hpp>
#include <speakerman/SpeakermanConfig.hpp>

namespace speakerman {
	struct SpeakerManLevels
	{
		static constexpr double getThreshold(double threshold)
		{
			return Values::force_between(threshold, 0.001, 0.999);
		}

		static constexpr double getLimiterThreshold(double threshold, double sloppyFactor)
		{
			return getThreshold(threshold) / Values::force_between(sloppyFactor, 0.1, 1.0);
		}

		static constexpr double getRmsThreshold(double threshold, double relativeBandWeight)
		{
			return getThreshold(threshold) * Values::force_between(relativeBandWeight, 0.001, 0.999);
		}
	};

	template<typename T>
	class EqualizerFilterData
	{
		using Coefficients = FixedSizeIirCoefficients<T,2>;
		Coefficients biquad1_;
		Coefficients biquad2_;
		size_t count_;

	public:
		size_t count() const { return count_; }
		const Coefficients &biquad1() const { return biquad1_; }
		const Coefficients &biquad2() const { return biquad2_; }

		static EqualizerFilterData<T> createConfigured(const GroupConfig &config, double sampleRate)
		{
			EqualizerFilterData<T> result;
			result.configure(config, sampleRate);
			return result;
		}

		void configure(const GroupConfig &config, double sampleRate)
		{
			count_ = config.eqs;
			if (config.eqs > 0) {
				auto w1 = biquad1_.wrap();
				BiQuad::setParametric(w1, sampleRate, config.eq[0].center, config.eq[0].gain, config.eq[0].bandwidth);
				if (config.eqs > 1) {
					auto w2 = biquad2_.wrap();
					BiQuad::setParametric(w1, sampleRate, config.eq[1].center, config.eq[1].gain, config.eq[1].bandwidth);
				}
			}
		}

		void reset()
		{
			biquad1_.setTransparent();
			biquad2_.setTransparent();
			count_ = 0;
		}
	};

	template <typename T, size_t BANDS>
	class GroupRuntimeData
	{
		T volume_;
		T bandRmsScale_[BANDS];
		T bandRmsThreshold_[BANDS];
		T limiterScale_;
		T limiterThreshold_;
		EqualizerFilterData<T> filterConfig_;

	public:
		T volume() const { return volume_; }
		T bandRmsScale(size_t i) const { return bandRmsScale_[IndexPolicy::array(i, BANDS)]; }
		T bandRmsThreshold(size_t i) const { return bandRmsThreshold_[IndexPolicy::array(i, BANDS)]; }
		T limiterScale() const { return limiterScale_; }
		T limiterThreshold() const { return limiterThreshold_; }
		const EqualizerFilterData<T> &filterConfig() const { return filterConfig_; }

		void reset()
		{
			volume_ = 0;
			limiterScale_ = 1;
			limiterThreshold_ = 1;
			for (size_t band = 0; band < BANDS; band++) {
				bandRmsThreshold_[band] = 1.0 / BANDS;
				bandRmsScale_[band] = BANDS;
			}
			filterConfig_.reset();
		}

		void setFilterConfig(const EqualizerFilterData<T> &source) { filterConfig_ = source; }
		template<typename...A>
		void setLevels(double volume, double threshold, double sloppyFactor, const ArrayTraits<A...> &relativeBandWeights)
		{
			volume_ = Values::force_between(volume, 0.0, 10.0);
			if (volume < 1e-6) {
				volume_ = 0;
			}
			limiterThreshold_ = SpeakerManLevels::getLimiterThreshold(threshold, sloppyFactor);
			limiterScale_ = 1.0 / limiterThreshold_;
			for (size_t band = 0; band < BANDS; band++) {
				bandRmsThreshold_[band] = SpeakerManLevels::getRmsThreshold(threshold, relativeBandWeights[band]);
				bandRmsScale_[band] = 1.0 / bandRmsThreshold_[band];
			}
		}

		void init(const GroupRuntimeData<T, BANDS> &source)
		{
			*this = source;
			volume_ = 0;
		}

		void approach(const GroupRuntimeData<T, BANDS> &target, const IntegrationCoefficients<T> &integrator)
		{
			integrator.integrate(target.volume_, volume_);
			integrator.integrate(target.limiterThreshold_, limiterThreshold_);
			integrator.integrate(target.limiterScale_, limiterScale_);
			for (size_t band = 0; band < BANDS; band++) {
				integrator.integrate(target.bandRmsThreshold_[band], bandRmsThreshold_[band]);
				integrator.integrate(target.bandRmsScale_[band], bandRmsScale_[band]);
			}
		}
	};


	template<typename T, size_t GROUPS, size_t BANDS>
	class SpeakermanRuntimeData
	{
		FixedSizeArray<GroupRuntimeData<T, BANDS>, GROUPS> groupConfig_;
		T subLimiterScale_;
		T subLimiterThreshold_;
		T subRmsThreshold_;
		T subRmsScale_;
		IntegrationCoefficients<T> controlSpeed_;


	public:
		GroupRuntimeData<T, BANDS> &groupConfig(size_t i) { return groupConfig_[i]; }
		const GroupRuntimeData<T, BANDS> &groupConfig(size_t i) const { return groupConfig_[i]; }
		T subLimiterScale() const { return subLimiterScale_; }
		T subLimiterThreshold() const { return subLimiterThreshold_; }
		T subRmsThreshold() const { return subRmsThreshold_; }
		T subRmsScale() const { return subRmsScale_; }

		void reset()
		{
			subLimiterThreshold_ = 1;
			subLimiterScale_ = 1;
			subRmsThreshold_ = 1;
			subRmsScale_ = 1;
			for (size_t group = 0; group < GROUPS; group++) {
				groupConfig_[group].reset();
			}
			controlSpeed_.setCharacteristicSamples(5000);
		}

		void init(const SpeakermanRuntimeData<T, GROUPS, BANDS> &source)
		{
			*this = source;
			for (size_t group = 0; group < GROUPS; group++) {
				groupConfig_[group].init(source.groupConfig(group));
			}
		}

		void approach(const SpeakermanRuntimeData<T, GROUPS, BANDS> &target)
		{
			controlSpeed_.integrate(target.subLimiterThreshold_, subLimiterThreshold_);
			controlSpeed_.integrate(target.subLimiterScale_, subLimiterScale_);
			controlSpeed_.integrate(target.subRmsThreshold_, subRmsThreshold_);
			controlSpeed_.integrate(target.subRmsScale_, subRmsScale_);

			for (size_t group = 0; group < GROUPS; group++) {
				groupConfig_[group].approach(target.groupConfig_[group], controlSpeed_);
			}
		}

		template<typename... A>
		void configure(
				const SpeakermanConfig &config, double sampleRate,
				const ArrayTraits<A...> &bandWeights,
				double fastestPeakWeight)
		{
			if (config.groups != GROUPS) {
				throw std::invalid_argument("Cannot change number of groups run-time");
			}
			double sumOfGroupThresholds = 0.0;
			double peakWeight = Values::force_between(fastestPeakWeight, 0.1, 1.0);

			for (size_t group = 0; group < config.groups; group++) {
				const speakerman::GroupConfig &sourceConf = config.group[group];
				GroupRuntimeData<T, BANDS> &targetConf = groupConfig_[group];
				targetConf.setFilterConfig(EqualizerFilterData<T>::createConfigured(sourceConf, sampleRate));

				double groupThreshold = sourceConf.threshold;
				targetConf.setLevels(sourceConf.volume, groupThreshold, fastestPeakWeight, bandWeights);

				sumOfGroupThresholds += groupThreshold;
			}
			double threshold = config.relativeSubThreshold * sumOfGroupThresholds;
			subLimiterScale_ = SpeakerManLevels::getLimiterThreshold(threshold, peakWeight);
			subLimiterThreshold_ = 1.0 / subLimiterScale_;
			subRmsThreshold_ = SpeakerManLevels::getRmsThreshold(threshold, bandWeights[0]);
			subRmsScale_ = 1.0 / subRmsThreshold_;
			controlSpeed_.setCharacteristicSamples(0.05 * sampleRate);
		}
	};

	template<typename T, size_t CHANNELS_PER_GROUP>
	class EqualizerFilter
	{
		using BqFilter = BiquadFilter<T, CHANNELS_PER_GROUP>;
		BqFilter filter1;
		BqFilter filter2;
		MultiFilter<T> * filter_;

		struct SingleBiQuad : public MultiFilter<T>
		{
			BqFilter &f;
			SingleBiQuad(BqFilter &ref) : f(ref) {}
			virtual size_t channels() const { return CHANNELS_PER_GROUP; }
			virtual T filter(size_t channel, T input) { return f.filter(channel, input); }
			virtual void reset() { f.reset(); }
			virtual ~SingleBiQuad() = default;
		}
				singleBiQuad;

		struct DoubleBiQuad : public MultiFilter<T>
		{
			BqFilter &f1;
			BqFilter &f2;
		public:
			DoubleBiQuad(BqFilter &ref1, BqFilter &ref2) : f1(ref1), f2(ref2) {}
			virtual size_t channels() const { return CHANNELS_PER_GROUP; }
			virtual T filter(size_t channel, T input) { return f2.filter(channel, f1.filter(channel, input)); }
			virtual void reset() { f1.reset(); f2.reset(); }
			virtual ~DoubleBiQuad() = default;
		}
				doubleBiQuad;

		static IdentityMultiFilter<T> *noFilter()
		{
			static IdentityMultiFilter<T> filter;
			return &filter;
		}

		MultiFilter<T> * configuredFilter(EqualizerFilterData<T> config)
		{
			if (config.count() == 0) {
				return noFilter();
			}
			filter1.coefficients_ = config.biquad1();
			if (config.count() == 1) {
				return &singleBiQuad;
			}
			filter2.coefficients_ = config.biquad2();
			return &doubleBiQuad;
		}

	public:
		EqualizerFilter() :
			filter_(noFilter()),
			singleBiQuad(filter1),
			doubleBiQuad(filter1, filter2) {}

		void configure(EqualizerFilterData<T> config)
		{
			filter_ = configuredFilter(config);
		}

		MultiFilter<T> * filter() { return filter_; }
	};

	template<typename T, size_t GROUPS, size_t BANDS, size_t CHANNELS_PER_GROUP>
	class SpeakermanRuntimeConfigurable
	{
		using Data = SpeakermanRuntimeData<T, GROUPS, BANDS>;

		Data active;
		Data middle;
		Data userSet;

		EqualizerFilter<T, CHANNELS_PER_GROUP> filters_[GROUPS];

	public:
		EqualizerFilter<T, CHANNELS_PER_GROUP> &filter(size_t index)
		{
			return filters_[IndexPolicy::array(index, GROUPS)];
		}

		const Data &data() const { return active; }

		size_t groups() const { return GROUPS; }
		size_t channelsPerGroup() const { return CHANNELS_PER_GROUP; }

		SpeakermanRuntimeConfigurable()
		{
			active.reset();
			middle.reset();
			userSet.reset();
		}

		void modify(const Data &source)
		{
			userSet = source;
			for (size_t group = 0; group < GROUPS; group++) {
				active.groupConfig(group).setFilterConfig(source.groupConfig(group).filterConfig());
				filters_[group].configure(source.groupConfig(group).filterConfig());
			}
		}

		void init(const Data &source)
		{
			userSet = source;
			middle.init(userSet);
			active.init(middle);
		}

		void approach()
		{
			middle.approach(userSet);
			active.approach(middle);
		}
	};

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_SPEAKERMAN_RUNTIME_CONFIG_GUARD_H_ */
