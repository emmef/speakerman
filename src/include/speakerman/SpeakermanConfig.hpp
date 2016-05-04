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

#ifndef SMS_SPEAKERMAN_SPEAKERMANCONFIG_GUARD_H_
#define SMS_SPEAKERMAN_SPEAKERMANCONFIG_GUARD_H_

#include <chrono>
#include <ostream>
#include <tdap/IndexPolicy.hpp>
#include <tdap/Value.hpp>
#include <speakerman/utils/Config.hpp>
#include <cmath>

namespace speakerman {

	struct EqualizerConfig
	{
		static constexpr double MIN_CENTER_FREQ = 40;
		static constexpr double MAX_CENTER_FREQ = 22000;

		static constexpr double MIN_GAIN = 0.10;
		static constexpr double MAX_GAIN = 10.0;

		static constexpr double MIN_BANDWIDTH = 0.25;
		static constexpr double MAX_BANDWIDTH = 8.0;

		static constexpr size_t KEY_CENTER = 0;
		static constexpr size_t KEY_GAIN = 1;
		static constexpr size_t KEY_BANDWIDTH = 2;

		static constexpr const char * KEY_SNIPPET_EQUALIZER = "equalizer";
		static constexpr const char * KEY_SNIPPET_CENTER = "center";
		static constexpr const char * KEY_SNIPPET_GAIN = "gain";
		static constexpr const char * KEY_SNIPPET_BANDWIDTH = "bandwidth";

		double center;
		double gain;
		double bandwidth;
	};

	struct GroupConfig
	{
		static constexpr size_t MIN_EQS = 0;
		static constexpr size_t DEFAULT_EQS = 0;
		static constexpr size_t MAX_EQS = 2;

		static constexpr double MIN_THRESHOLD = 0.010;
		static constexpr double DEFAULT_THRESHOLD = 0.1;
		static constexpr double MAX_THRESHOLD = 0.5;

		static constexpr double MIN_VOLUME = 0;
		static constexpr double DEFAULT_VOLUME = 1.0;
		static constexpr double MAX_VOLUME = 20.0;

		static constexpr double MIN_DELAY = 0;
		static constexpr double DEFAULT_DELAY = 0;
		static constexpr double MAX_DELAY = 0.010;

		static constexpr size_t KEY_EQ_COUNT = 0;
		static constexpr size_t KEY_THRESHOLD = 1;
		static constexpr size_t KEY_VOLUME = 2;
		static constexpr size_t KEY_DELAY = 3;


		static constexpr const char * KEY_SNIPPET_GROUP = "group";
		static constexpr const char * KEY_SNIPPET_EQ_COUNT = "equalizers";
		static constexpr const char * KEY_SNIPPET_THRESHOLD = "threshold";
		static constexpr const char * KEY_SNIPPET_VOLUME = "volume";
		static constexpr const char * KEY_SNIPPET_DELAY = "delay";


		EqualizerConfig eq[MAX_EQS];
		size_t eqs;
		double threshold;
		double volume;
		double delay;
	};

	struct SpeakermanConfig
	{
		static constexpr size_t MIN_GROUPS = 1;
		static constexpr size_t DEFAULT_GROUPS = 1;
		static constexpr size_t MAX_GROUPS = 4;

		static constexpr size_t MIN_GROUP_CHANNELS = 1;
		static constexpr size_t DEFAULT_GROUP_CHANNELS = 2;
		static constexpr size_t MAX_GROUP_CHANNELS = 5;

		static constexpr double MIN_REL_SUB_THRESHOLD = 0.25;
		static constexpr double DEFAULT_REL_SUB_THRESHOLD = M_SQRT2;
		static constexpr double MAX_REL_SUB_THRESHOLD = 2.0;

		static constexpr double MIN_SUB_DELAY = GroupConfig::MIN_DELAY;
		static constexpr double DEFAULT_SUB_DELAY = GroupConfig::DEFAULT_DELAY;
		static constexpr double MAX_SUB_DELAY = GroupConfig::MAX_DELAY;

		static constexpr size_t MIN_SUB_OUTPUT = 0;
		static constexpr size_t DEFAULT_SUB_OUTPUT = 1;
		static constexpr size_t MAX_SUB_OUTPUT = MAX_GROUPS *  MAX_GROUP_CHANNELS + 1;

		static constexpr size_t KEY_GROUP_COUNT = 0;
		static constexpr size_t KEY_CHANNELS = 1;
		static constexpr size_t KEY_SUB_THRESHOLD = 2;
		static constexpr size_t KEY_SUB_DELAY = 3;
		static constexpr size_t KEY_SUB_OUTPUT = 4;

		static constexpr const char * KEY_SNIPPET_GROUP_COUNT = "groups";
		static constexpr const char * KEY_SNIPPET_CHANNELS = "group-channels";
		static constexpr const char * KEY_SNIPPET_SUB_THRESHOLD = "sub-relative-threshold";
		static constexpr const char * KEY_SNIPPET_SUB_DELAY = "sub-delay";
		static constexpr const char * KEY_SNIPPET_SUB_OUTPUT = "sub-output";

		GroupConfig group[MAX_GROUPS];
		size_t groups;
		size_t groupChannels;
		size_t subOutput;
		double relativeSubThreshold;
		double subDelay;
		long long timeStamp;
	};


	class DynamicProcessorLevels
	{
		double gains_[SpeakermanConfig::MAX_GROUPS + 1];
		double avg_gains_[SpeakermanConfig::MAX_GROUPS + 1];
		double signal_[SpeakermanConfig::MAX_GROUPS];
		size_t groups_;
		size_t crossovers_;
		size_t count_;

	public:
		DynamicProcessorLevels() : groups_(0), count_(0) {};
		DynamicProcessorLevels(size_t groups, size_t crossovers) : groups_(groups), crossovers_(crossovers) {}

		size_t groups() const { return groups_; }
		size_t signals() const { return groups_ + 1; }
		size_t count() const { return count_; }
		size_t crossovers() const { return crossovers_; }

		void operator += (const DynamicProcessorLevels &levels)
		{
			size_t count = Values::min(groups_, levels.groups_);
			for (size_t i= 0; i <= count; i++) {
				gains_[i] = Values::min(gains_[i], levels.gains_[i]);
				avg_gains_[i] += levels.avg_gains_[i];
			}
			for (size_t i= 0; i < count; i++) {
				signal_[i] = Values::max(signal_[i], levels.signal_[i]);
			}
			count_ += levels.count_;
		}

		void reset()
		{
			for (size_t limiter = 0; limiter <= groups_; limiter++) {
				gains_[limiter] = 1.0;
				avg_gains_[limiter] = 0;
			}
			for (size_t group = 0; group < groups_; group++) {
				signal_[group] = 0.0;
			}
			count_ = 0;
		}

		void setGroupGain(size_t group, double gain)
		{
			double &g = gains_[1 + IndexPolicy::array(group, groups_)];
			g = Values::min(g, gain);
			avg_gains_[1 + group] += g;
		}

		double getGroupGain(size_t group) const
		{
			return gains_[1 + IndexPolicy::array(group, groups_)];
		}

		double getAverageGroupGain(size_t group) const
		{
			size_t den = count_ * crossovers_;
			return den == 0 ? 1.0 : avg_gains_[1 + IndexPolicy::array(group, groups_)] / den;
		}

		void setSubGain(double gain)
		{
			double &g = gains_[0];
			g = Values::min(g, gain);
			avg_gains_[0] += g;
			count_++;
		}

		double getSubGain() const
		{
			return gains_[0];
		}

		double getAverageSubGain() const
		{
			return count_ == 0 ? 1.0 : avg_gains_[0] / count_;
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

	class SpeakerManagerControl
	{
	public:

		virtual const SpeakermanConfig &getConfig() const = 0;
		virtual bool applyConfigAndGetLevels(const SpeakermanConfig &config, DynamicProcessorLevels *levels, std::chrono::milliseconds timeoutMillis) = 0;
		virtual bool getLevels(DynamicProcessorLevels *levels, std::chrono::milliseconds timeoutMillis) = 0;
	};

	const char *getInstallBaseDirectory();
	const char * getWebSiteDirectory();
	const char * configFileName();
	const char * webDirectory();
	SpeakermanConfig readSpeakermanConfig(bool initial);
	SpeakermanConfig readSpeakermanConfig(const SpeakermanConfig &basedUpon, bool initial);
	SpeakermanConfig getDefaultConfig();
	void dumpSpeakermanConfig(const SpeakermanConfig& dump, std::ostream &output);
	long long getFileTimeStamp(const char * fileName);
	long long getConfigFileTimeStamp();



} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_SPEAKERMANCONFIG_GUARD_H_ */
