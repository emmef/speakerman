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

#include <ostream>
#include <speakerman/utils/Config.hpp>

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

		static constexpr double MIN_THRESHOLD = 0.030;
		static constexpr double DEFAULT_THRESHOLD = 0.2;
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
		static constexpr double DEFAULT_REL_SUB_THRESHOLD = 1.0;
		static constexpr double MAX_REL_SUB_THRESHOLD = 2.0;

		static constexpr double MIN_SUB_DELAY = GroupConfig::MIN_DELAY;
		static constexpr double DEFAULT_SUB_DELAY = GroupConfig::DEFAULT_DELAY;
		static constexpr double MAX_SUB_DELAY = GroupConfig::MAX_DELAY;

		static constexpr size_t KEY_GROUP_COUNT = 0;
		static constexpr size_t KEY_CHANNELS = 1;
		static constexpr size_t KEY_SUB_THRESHOLD = 2;
		static constexpr size_t KEY_SUB_DELAY = 3;

		static constexpr const char * KEY_SNIPPET_GROUP_COUNT = "groups";
		static constexpr const char * KEY_SNIPPET_CHANNELS = "group-channels";
		static constexpr const char * KEY_SNIPPET_SUB_THRESHOLD = "sub-relative-threshold";
		static constexpr const char * KEY_SNIPPET_SUB_DELAY = "sub-delay";

		GroupConfig group[MAX_GROUPS];
		size_t groups;
		size_t groupChannels;
		double relativeSubThreshold;
		double subDelay;
		long long timeStamp;
	};

	const char * configFileName();
	SpeakermanConfig readSpeakermanConfig(bool initial);
	SpeakermanConfig readSpeakermanConfig(const SpeakermanConfig &basedUpon, bool initial);
	SpeakermanConfig getDefaultConfig();
	void dumpSpeakermanConfig(const SpeakermanConfig& dump, std::ostream &output);
	long long getFileTimeStamp(const char * fileName);
	long long getConfigFileTimeStamp();

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_SPEAKERMANCONFIG_GUARD_H_ */
