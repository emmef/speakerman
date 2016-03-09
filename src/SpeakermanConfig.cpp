/*
 * SpeakermanConfig.cpp
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

#include <string>
#include <cstring>
#include <cmath>
#include <mutex>
#include <iostream>
#include <sys/stat.h>
#include <tdap/Value.hpp>
#include <tdap/IndexPolicy.hpp>
#include <tdap/Count.hpp>
#include <speakerman/SpeakermanConfig.hpp>
#include <speakerman/utils/Config.hpp>

namespace speakerman {

	using namespace std;
	using namespace tdap;

	class ReadConfigException : public runtime_error
	{
	public:
		ReadConfigException(const char * message) : runtime_error(message)
		{

		}
		static ReadConfigException noSeek()
		{
			return ReadConfigException("Could not reset file read position");
		}
	};


	static constexpr size_t ID_GLOBAL_CNT = 4;
	static constexpr size_t ID_GROUP_CNT = 4;
	static constexpr size_t ID_EQ_CNT = 3;

	static constexpr bool isUserAllowed(ssize_t groupId, ssize_t eqId, size_t fieldId)
	{
		return
				eqId >= 0 ? false : // eq happens after limiter do not allowed
				groupId >= 0 ? fieldId == GroupConfig::KEY_VOLUME : // volume happens before limiter so is allowed
				false;
	}

	static constexpr bool isRuntimeAllowed(ssize_t groupId, ssize_t eqId, size_t fieldId)
	{
		return
			eqId >= 0 ? true :
			groupId >= 0 ? true :
			fieldId == SpeakermanConfig::KEY_SUB_THRESHOLD;
	}

	static constexpr size_t GROUP_CONFIG_SIZE =
			GroupConfig::MAX_EQS * ID_EQ_CNT + ID_GROUP_CNT;
	static constexpr size_t GLOBAL_SIZE =
			ID_GLOBAL_CNT + SpeakermanConfig::MAX_GROUPS * GROUP_CONFIG_SIZE;
	static constexpr ssize_t INDEX_NEGATIVE = -2 * static_cast<ssize_t>(GLOBAL_SIZE);

	static ssize_t getGroupOffsetUnsafe(ssize_t eqId, size_t fieldId)
	{
		return
				eqId < 0 ? (fieldId < ID_GROUP_CNT ? fieldId : INDEX_NEGATIVE) :
				eqId >= GroupConfig::MAX_EQS ? INDEX_NEGATIVE :
				ID_GROUP_CNT + eqId * ID_EQ_CNT + (fieldId < ID_EQ_CNT ? fieldId : INDEX_NEGATIVE);
	}

	static ssize_t getOffsetUnsafe(ssize_t groupId, ssize_t eqId, size_t fieldId)
	{
		return
			groupId < 0 ? (fieldId < ID_GLOBAL_CNT ? fieldId : INDEX_NEGATIVE) :
			groupId >= SpeakermanConfig::MAX_GROUPS ? INDEX_NEGATIVE :
			ID_GLOBAL_CNT + groupId * GROUP_CONFIG_SIZE + getGroupOffsetUnsafe(eqId, fieldId);
	}

	static ssize_t getOffset(ssize_t groupId, ssize_t eqId, size_t fieldId)
	{
		ssize_t result = getOffsetUnsafe(groupId, eqId, fieldId);
		if (result >= 0 && result < GLOBAL_SIZE) {
			return result;
		}
		cerr << "E: getOffset(" << groupId << "," << eqId << "," << fieldId << "): " << result;
		if (result < 0) {
			cerr << ": Invalid combination of arguments" << endl;
			throw std::invalid_argument("Invalid combination of arguments");
		}
		if (result >= GLOBAL_SIZE) {
			cerr << ": Internal error with result " << endl;
			throw std::logic_error("Wrongly calculated index");
		}
	}

	static const char * getConfigString(size_t idx)
	{
		static bool flag = false;
		static mutex m;
		static string strings[GLOBAL_SIZE];
		IndexPolicy::force(idx, GLOBAL_SIZE);

		if (flag) {
			return strings[idx].c_str();
		}
		unique_lock<mutex> lock(m);
		if (flag) {
			return strings[idx].c_str();
		}

		strings[getOffset(-1, -1, SpeakermanConfig::KEY_GROUP_COUNT)] = SpeakermanConfig::KEY_SNIPPET_GROUP_COUNT;
		strings[getOffset(-1, -1, SpeakermanConfig::KEY_CHANNELS)] = SpeakermanConfig::KEY_SNIPPET_CHANNELS;
		strings[getOffset(-1, -1, SpeakermanConfig::KEY_SUB_THRESHOLD)] = SpeakermanConfig::KEY_SNIPPET_SUB_THRESHOLD;
		strings[getOffset(-1, -1, SpeakermanConfig::KEY_SUB_DELAY)] = SpeakermanConfig::KEY_SNIPPET_SUB_DELAY;

		string key;
		for (size_t group = 0; group < SpeakermanConfig::MAX_GROUPS; group++) {
			string groupKey = GroupConfig::KEY_SNIPPET_GROUP;
			groupKey += "/";
			groupKey += ('0' + group);
			groupKey += "/";

			key = groupKey;
			key += GroupConfig::KEY_SNIPPET_EQ_COUNT;
			strings[getOffset(group, -1, GroupConfig::KEY_EQ_COUNT)] = key;
			key = groupKey;
			key += GroupConfig::KEY_SNIPPET_THRESHOLD;
			strings[getOffset(group, -1, GroupConfig::KEY_THRESHOLD)] = key;
			key = groupKey;
			key += GroupConfig::KEY_SNIPPET_VOLUME;
			strings[getOffset(group, -1, GroupConfig::KEY_VOLUME)] = key;
			key = groupKey;
			key += GroupConfig::KEY_SNIPPET_DELAY;
			strings[getOffset(group, -1, GroupConfig::KEY_DELAY)] = key;

			string eqBase = groupKey;
			eqBase += EqualizerConfig::KEY_SNIPPET_EQUALIZER;
			eqBase += "/";
			for (size_t eq = 0; eq < GroupConfig::MAX_EQS; eq++) {
				string eqKey = eqBase;
				eqKey += ('0' + eq);
				eqKey += "/";

				key = eqKey;
				key += EqualizerConfig::KEY_SNIPPET_CENTER;
				strings[getOffset(group, eq, EqualizerConfig::KEY_CENTER)] = key;
				key = eqKey;
				key += EqualizerConfig::KEY_SNIPPET_GAIN;
				strings[getOffset(group, eq, EqualizerConfig::KEY_GAIN)] = key;
				key = eqKey;
				key += EqualizerConfig::KEY_SNIPPET_BANDWIDTH;
				strings[getOffset(group, eq, EqualizerConfig::KEY_BANDWIDTH)] = key;
			}
		}

		flag = true;

		return strings[idx].c_str();
	};

	const char * getConfigKey(ssize_t groupId, ssize_t eqId, size_t fieldId)
	{
		return getConfigString(getOffset(groupId, eqId, fieldId));
	}

	static constexpr size_t UNSET_SIZE = -1;
	static constexpr double UNSET_FLOAT =  numeric_limits<double>::infinity();

	static constexpr const char * KEY_GROUPS = "/groups";
	static constexpr const char * KEY_GROUP_CHANNELS = "/group-channels";
	static constexpr const char * KEY_THRESHOLD_SUB_RELATIVE = "/sub-relative-threshold";

	static constexpr const char * KEY_GROUP_PREFIX = "group/";
	static constexpr const char * KEY_GROUP_EQS = "/equalizers";
	static constexpr const char * KEY_GROUP_THRESHOLD = "/threshold";
	static constexpr const char * KEY_GROUP_VOLUME = "/volume";

	static string getConfigFileName()
	{
		string configFileName = std::getenv("HOME");
		configFileName += "/.config/speakerman/speakerman.conf";

		return configFileName;
	}

	template<typename T, int type>
	struct ValueManager__
	{

	};

	template<typename T>
	struct ValueManager__<T, 1>
	{
		static_assert(is_integral<T>::value, "Expected integral type parameter");
		using Value = long long int;

		static Value parse(const char *value, char **end)
		{
			return strtoll(value, end, 10);
		}
	};

	template<typename T>
	struct ValueManager__<T, 2>
	{
		static_assert(is_floating_point<T>::value, "Expected floating point type parameter");
		using Value = long double;

		static Value parse(const char *value, char **end)
		{
			return strtold(value, end);
		}
	};

	template <typename T>
	struct ValueManager : public ValueManager__<T, is_integral<T>::value ? 1 : 2>
	{
		using __Value = ValueManager__<T, is_integral<T>::value ? 1 : 2>;
		using Value = typename __Value::Value;
		using __Value::parse;
	};

	const char *configFileName()
	{
		static string name = getConfigFileName();

		return name.c_str();
	}

	template<typename T>
	static bool readNumber(T &variable, const char *key, const char *value, T min, T max, bool forceWithin = false)
	{
		char *endptr;
		using Type = typename ValueManager<T>::Value;
		Type tempValue = ValueManager<T>::parse(value, &endptr);

		if (*endptr != 0 && !config::isWhiteSpace(*endptr)) {
			std::cerr << "E: Invalid value for \"" << key << "\": " << value << std::endl;
			return false;
		}

		bool outOfRange = tempValue < min || tempValue > max;
		if (outOfRange) {
			cerr << "W: value for \"" << key << "\": value " << tempValue << " out of [" << min << ".." << max << "]: ";
			if (!forceWithin) {
				cerr << "not set!" << endl;
				return false;
			}
			tempValue = Value<Type>::force_between(tempValue, min, max);
			cerr << "set to " << tempValue << endl;
		}
		variable = static_cast<T>(tempValue);
		return true;
	}

	template<typename T>
	static void setDefault(T &value, T unsetValue, T defaultValue, const char * key)
	{
		if (value != unsetValue) {
			return;
		}
		value = defaultValue;
	}

	static bool isKey(const char *key, bool initial, ssize_t groupId, ssize_t eqId, size_t fieldId)
	{
		if (initial || isRuntimeAllowed(groupId, eqId, fieldId)) {
			return strncmp(key, getConfigKey(groupId, eqId, fieldId), config::Reader::MAX_KEY_LENGTH) == 0;
		}
		return false;
	}

	static SpeakermanConfig generateDefaultConfig()
	{
		SpeakermanConfig config;

		config.groups = SpeakermanConfig::DEFAULT_GROUPS;
		config.groupChannels = SpeakermanConfig::DEFAULT_GROUP_CHANNELS;
		config.relativeSubThreshold = SpeakermanConfig::DEFAULT_REL_SUB_THRESHOLD;

		for (size_t group = 0; group < SpeakermanConfig::MAX_GROUPS; group++) {
			GroupConfig &groupConfig = config.group[group];
			groupConfig.eqs = GroupConfig::DEFAULT_EQS;
			groupConfig.threshold = GroupConfig::DEFAULT_THRESHOLD;
			groupConfig.volume = GroupConfig::DEFAULT_VOLUME;
			for (size_t eq = 0; eq < GroupConfig::MAX_EQS; eq++) {
				EqualizerConfig &eqConfig = groupConfig.eq[eq];
				eqConfig.center = 20000;
				eqConfig.gain = 10;
				eqConfig.bandwidth = 2;
			}
		}

		return config;
	}

	static const SpeakermanConfig &getDefaultConfigRef()
	{
		static SpeakermanConfig config = generateDefaultConfig();

		return config;
	}

	SpeakermanConfig getDefaultConfig()
	{
		return getDefaultConfigRef();
	}

	static void resetStream(istream &stream)
	{
		stream.clear(istream::eofbit);
		stream.seekg(0, stream.beg);
		if (stream.fail()) {
			throw ReadConfigException::noSeek();
		}
	}

	typedef struct {
		SpeakermanConfig &config;
		bool initial;
	} SpeakermanConfigCallbackData;


	typedef struct {
		GroupConfig &config;
		size_t groupId;
		bool initial;
	} GroupConfigCallbackData;

	typedef struct {
		EqualizerConfig &config;
		size_t groupId;
		size_t eqId;
		bool initial;
	} EqConfigCallbackData;

	static speakerman::config::CallbackResult readGlobalCallback(const char *key, const char *value, void *data)
	{
		SpeakermanConfigCallbackData *config = static_cast<SpeakermanConfigCallbackData *>(data);

		if (isKey(key, config->initial, -1, -1, SpeakermanConfig::KEY_GROUP_COUNT)) {
			readNumber(config->config.groups, key, value, SpeakermanConfig::MIN_GROUPS, SpeakermanConfig::MAX_GROUPS);
		}
		else if (isKey(key, config->initial, -1, -1, SpeakermanConfig::KEY_CHANNELS)) {
			readNumber(config->config.groupChannels, key, value, SpeakermanConfig::MIN_GROUP_CHANNELS, SpeakermanConfig::MAX_GROUP_CHANNELS);
		}
		else if (isKey(key, config->initial, -1, -1, SpeakermanConfig::KEY_SUB_THRESHOLD)) {
			readNumber(config->config.relativeSubThreshold, key, value, SpeakermanConfig::MIN_REL_SUB_THRESHOLD, SpeakermanConfig::MAX_REL_SUB_THRESHOLD, true);
		}
		else if (isKey(key, config->initial, -1, -1, SpeakermanConfig::KEY_SUB_DELAY)) {
			readNumber(config->config.subDelay, key, value, SpeakermanConfig::MIN_SUB_DELAY, SpeakermanConfig::MAX_SUB_DELAY, true);
		}
		return speakerman::config::CallbackResult::CONTINUE;
	}

	static speakerman::config::CallbackResult readGroupCallback(const char *key, const char *value, void *data)
	{
		GroupConfigCallbackData *config = static_cast<GroupConfigCallbackData *>(data);

		if (isKey(key, config->initial, config->groupId, -1, GroupConfig::KEY_EQ_COUNT)) {
			readNumber(config->config.eqs, key, value, GroupConfig::MIN_EQS, GroupConfig::MAX_EQS);
		}
		else if (isKey(key, config->initial, config->groupId, -1, GroupConfig::KEY_THRESHOLD)) {
			readNumber(config->config.threshold, key, value, GroupConfig::MIN_THRESHOLD, GroupConfig::MAX_THRESHOLD, true);
		}
		else if (isKey(key, config->initial, config->groupId, -1, GroupConfig::KEY_VOLUME)) {
			readNumber(config->config.volume, key, value, GroupConfig::MIN_VOLUME, GroupConfig::MAX_VOLUME, true);
		}
		else if (isKey(key, config->initial, config->groupId, -1, GroupConfig::KEY_DELAY)) {
			readNumber(config->config.delay, key, value, GroupConfig::MIN_DELAY, GroupConfig::MAX_DELAY, true);
		}
		return speakerman::config::CallbackResult::CONTINUE;
	}

	static speakerman::config::CallbackResult readEqCallback(const char *key, const char *value, void *data)
	{
		EqConfigCallbackData *config = static_cast<EqConfigCallbackData *>(data);

		if (isKey(key, config->initial, config->groupId, config->eqId, EqualizerConfig::KEY_CENTER)) {
			readNumber(config->config.center, key, value, EqualizerConfig::MIN_CENTER_FREQ, EqualizerConfig::MAX_CENTER_FREQ);
		}
		else if (isKey(key, config->initial, config->groupId, config->eqId, EqualizerConfig::KEY_GAIN)) {
			readNumber(config->config.gain, key, value, EqualizerConfig::MIN_GAIN, EqualizerConfig::MAX_GAIN, true);
		}
		else if (isKey(key, config->initial, config->groupId, config->eqId, EqualizerConfig::KEY_BANDWIDTH)) {
			readNumber(config->config.bandwidth, key, value, EqualizerConfig::MIN_BANDWIDTH, EqualizerConfig::MAX_BANDWIDTH, true);
		}
		return speakerman::config::CallbackResult::CONTINUE;
	}

	static void readEq(EqConfigCallbackData &data, istream &stream, config::Reader &reader, const SpeakermanConfig &basedUpon)
	{
		if (data.eqId >= GroupConfig::MAX_EQS) {
			return;
		}
		resetStream(stream);
		EqualizerConfig &config = data.config;
		config.center = UNSET_FLOAT;
		config.gain = UNSET_FLOAT;
		config.bandwidth = UNSET_FLOAT;
		
		auto result = reader.read(stream, readEqCallback, &data);
		if (result != config::ReadResult::SUCCESS && result != config::ReadResult::STOPPED) {
			throw runtime_error("Parse error");
		}

		const EqualizerConfig &defaulConfig = basedUpon.group[data.groupId].eq[data.eqId];
		setDefault(config.center, UNSET_FLOAT, defaulConfig.center, getConfigKey(data.groupId, data.eqId, EqualizerConfig::KEY_CENTER));
		setDefault(config.gain, UNSET_FLOAT, defaulConfig.gain, getConfigKey(data.groupId, data.eqId, EqualizerConfig::KEY_GAIN));
		setDefault(config.bandwidth, UNSET_FLOAT, defaulConfig.bandwidth, getConfigKey(data.groupId, data.eqId, EqualizerConfig::KEY_BANDWIDTH));
	}

	static void readGroup(GroupConfigCallbackData &data, istream &stream, config::Reader &reader, const SpeakermanConfig &basedUpon)
	{
		if (data.groupId >= SpeakermanConfig::MAX_GROUPS) {
			return;
		}
		resetStream(stream);
		GroupConfig &config = data.config;
		config.eqs = UNSET_SIZE;
		config.threshold = UNSET_FLOAT;
		config.volume = UNSET_FLOAT;
		config.delay = UNSET_FLOAT;

		auto result = reader.read(stream, readGroupCallback, &data);
		if (result != config::ReadResult::SUCCESS && result != config::ReadResult::STOPPED) {
			throw runtime_error("Parse error");
		}

		const GroupConfig &defaultConfig = basedUpon.group[data.groupId];
		setDefault(config.eqs, UNSET_SIZE, defaultConfig.eqs, getConfigKey(data.groupId, -1, GroupConfig::KEY_EQ_COUNT));
		setDefault(config.threshold, UNSET_FLOAT, defaultConfig.threshold, getConfigKey(data.groupId, -1, GroupConfig::KEY_THRESHOLD));
		setDefault(config.volume, UNSET_FLOAT, defaultConfig.volume, getConfigKey(data.groupId, -1, GroupConfig::KEY_VOLUME));
		setDefault(config.delay, UNSET_FLOAT, defaultConfig.delay, getConfigKey(data.groupId, -1, GroupConfig::KEY_DELAY));

		for (size_t eq = 0; eq < GroupConfig::MAX_EQS; eq++) {
			EqConfigCallbackData info { config.eq[eq], data.groupId, eq, data.initial };
			readEq(info, stream, reader, basedUpon);
		}
	}

	static void readGlobals(SpeakermanConfig &config, istream &stream, config::Reader &reader, const SpeakermanConfig &basedUpon, bool initial)
	{
		resetStream(stream);
		config.groups = UNSET_SIZE;
		config.groupChannels = UNSET_SIZE;
		config.relativeSubThreshold = UNSET_FLOAT;
		config.subDelay = UNSET_FLOAT;

		SpeakermanConfigCallbackData data { config, initial };
		auto result = reader.read(stream, readGlobalCallback, &data);
		if (result != config::ReadResult::SUCCESS && result != config::ReadResult::STOPPED) {
			throw runtime_error("Parse error");
		}

		setDefault(config.groups, UNSET_SIZE, basedUpon.groups, getConfigKey(-1, -1, SpeakermanConfig::KEY_GROUP_COUNT));
		setDefault(config.groupChannels, UNSET_SIZE, basedUpon.groupChannels, getConfigKey(-1, -1, SpeakermanConfig::KEY_CHANNELS));
		setDefault(config.relativeSubThreshold, UNSET_FLOAT, basedUpon.relativeSubThreshold, getConfigKey(-1, -1, SpeakermanConfig::KEY_SUB_THRESHOLD));
		setDefault(config.subDelay, UNSET_FLOAT, basedUpon.subDelay, getConfigKey(-1, -1, SpeakermanConfig::KEY_SUB_DELAY));

		for (size_t g = 0; g < config.groups; g++) {
			GroupConfigCallbackData data { config.group[g], g, initial };
			readGroup(data, stream, reader, basedUpon);
		}
	}

	static void actualReadConfig(SpeakermanConfig &config, istream &stream, const SpeakermanConfig &basedUpon, bool initial)
	{
		config::Reader reader;

		readGlobals(config, stream, reader, basedUpon, initial);
	}

	class StreamOwner
	{
		ifstream &stream;
	public:
		StreamOwner(ifstream &owned) : stream(owned) { }

		~StreamOwner()
		{
			if (stream.is_open()) {
				stream.close();
			}
		}
	};

	long long getFileTimeStamp(const char * fileName)
	{
		struct stat stats;
		long long stamp = 0;
		if (stat(fileName, &stats) == 0) {
			return stats.st_mtim.tv_sec;
		}
		return -1;
	}

	long long getConfigFileTimeStamp()
	{
		return getFileTimeStamp(configFileName());
	}

	SpeakermanConfig readSpeakermanConfig(const SpeakermanConfig &basedUpon, bool initial)
	{
		SpeakermanConfig result;
		ifstream stream;
		stream.open(configFileName());
		long long stamp = getFileTimeStamp(configFileName());

		StreamOwner owner(stream);
		if (!stream.is_open()) {
			return basedUpon;
		}
		try {
			actualReadConfig(result, stream, basedUpon, initial);
		}
		catch (const ReadConfigException &e) {
			cerr << "E: " << e.what() << endl;
			return basedUpon;
		}
		result.timeStamp = stamp;
		return result;
	}

	SpeakermanConfig readSpeakermanConfig(bool initial)
	{
		SpeakermanConfig basedUpon = getDefaultConfigRef();
		return readSpeakermanConfig(basedUpon, initial);
	}

	void dumpSpeakermanConfig(const SpeakermanConfig& dump, ostream &output)
	{
		const char *assgn = " = ";
		output << "# Speakerman configuration dump" << endl << endl;
		output << "# Global" << endl;
		output << getConfigKey(-1, -1, SpeakermanConfig::KEY_GROUP_COUNT) << assgn << dump.groups << endl;
		output << getConfigKey(-1, -1, SpeakermanConfig::KEY_CHANNELS) << assgn << dump.groupChannels << endl;
		output << getConfigKey(-1, -1, SpeakermanConfig::KEY_SUB_THRESHOLD) << assgn << dump.relativeSubThreshold << endl;
		output << getConfigKey(-1, -1, SpeakermanConfig::KEY_SUB_DELAY) << assgn << dump.subDelay << endl;

		for (size_t group = 0; group < dump.groups; group++) {
			const GroupConfig &groupConfig = dump.group[group];
			output << getConfigKey(group, -1, GroupConfig::KEY_EQ_COUNT) << assgn << groupConfig.eqs << endl;
			output << getConfigKey(group, -1, GroupConfig::KEY_THRESHOLD) << assgn << groupConfig.threshold << endl;
			output << getConfigKey(group, -1, GroupConfig::KEY_VOLUME) << assgn << groupConfig.volume << endl;
			output << getConfigKey(group, -1, GroupConfig::KEY_DELAY) << assgn << groupConfig.delay << endl;

			for (size_t eq = 0; eq < GroupConfig::MAX_EQS; eq++) {
				const EqualizerConfig &eqConfig = groupConfig.eq[eq];
				output << getConfigKey(group, eq, EqualizerConfig::KEY_CENTER) << assgn << eqConfig.center << endl;
				output << getConfigKey(group, eq, EqualizerConfig::KEY_GAIN) << assgn << eqConfig.gain << endl;
				output << getConfigKey(group, eq, EqualizerConfig::KEY_BANDWIDTH) << assgn << eqConfig.bandwidth << endl;
			}
		}
		output << "Timestamp: " << dump.timeStamp << endl;
	}

} /* End of namespace speakerman */
