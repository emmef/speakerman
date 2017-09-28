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
#include <unistd.h>
#include <tdap/Value.hpp>
#include <tdap/IndexPolicy.hpp>
#include <tdap/FixedSizeArray.hpp>
#include <speakerman/SpeakermanConfig.hpp>

namespace speakerman {

    using namespace std;
    using namespace tdap;

    class ReadConfigException : public runtime_error
    {
    public:
        explicit ReadConfigException(const char *message) : runtime_error(message)
        {

        }

        static ReadConfigException noSeek()
        {
            return ReadConfigException("Could not reset file read position");
        }
    };


    static constexpr size_t ID_GLOBAL_CNT = 8;
    static constexpr size_t ID_GROUP_CNT = 6;
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
                (fieldId == SpeakermanConfig::KEY_SUB_THRESHOLD || fieldId == SpeakermanConfig::KEY_SUB_DELAY ||
                 fieldId == SpeakermanConfig::KEY_GENERATE_NOISE);
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

    static const char *getConfigString(size_t idx)
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
        strings[getOffset(-1, -1, SpeakermanConfig::KEY_SUB_OUTPUT)] = SpeakermanConfig::KEY_SNIPPET_SUB_OUTPUT;
        strings[getOffset(-1, -1, SpeakermanConfig::KEY_CROSSOVERS)] = SpeakermanConfig::KEY_SNIPPET_CROSSOVERS;
        strings[getOffset(-1, -1, SpeakermanConfig::KEY_INPUT_OFFSET)] = SpeakermanConfig::KEY_SNIPPET_INPUT_OFFSET;
        strings[getOffset(-1, -1,
                          SpeakermanConfig::KEY_GENERATE_NOISE)] = SpeakermanConfig::KEY_SNIPPET_GENERATE_NOISE;

        string key;
        for (size_t group = 0; group < SpeakermanConfig::MAX_GROUPS; group++) {
            string groupKey = GroupConfig::KEY_SNIPPET_GROUP;
            groupKey += "/";
            groupKey += (char)(group + '0');
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
            key = groupKey;
            key += GroupConfig::KEY_SNIPPET_USE_SUB;
            strings[getOffset(group, -1, GroupConfig::KEY_USE_SUB)] = key;
            key = groupKey;
            key += GroupConfig::KEY_SNIPPET_MONO;
            strings[getOffset(group, -1, GroupConfig::KEY_MONO)] = key;

            string eqBase = groupKey;
            eqBase += EqualizerConfig::KEY_SNIPPET_EQUALIZER;
            eqBase += "/";
            for (size_t eq = 0; eq < GroupConfig::MAX_EQS; eq++) {
                string eqKey = eqBase;
                eqKey += (char)('0' + eq);
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

    const char *getConfigKey(ssize_t groupId, ssize_t eqId, size_t fieldId)
    {
        return getConfigString(getOffset(groupId, eqId, fieldId));
    }


    static constexpr const char *KEY_GROUPS = "/groups";
    static constexpr const char *KEY_GROUP_CHANNELS = "/group-channels";
    static constexpr const char *KEY_THRESHOLD_SUB_RELATIVE = "/sub-relative-threshold_";

    static constexpr const char *KEY_GROUP_PREFIX = "group/";
    static constexpr const char *KEY_GROUP_EQS = "/equalizers";
    static constexpr const char *KEY_GROUP_THRESHOLD = "/threshold_";
    static constexpr const char *KEY_GROUP_VOLUME = "/volume";

    static bool fileExists(const char *fileName)
    {
        FILE *f = fopen(fileName, "r");
        if (f != nullptr) {
            fclose(f);
            return true;
        }
        return false;
    }

    static string internalGetInstallBase()
    {
        static constexpr const char *prefix = INSTALL_DIR;
        if (access(prefix, F_OK) != 0) {
            return "";
        }
        string prefixDir = prefix;
        if (prefixDir.at(prefixDir.length() - 1) != '/') {
            prefixDir += '/';
        }
        std::cout << "Install prefix: " << prefixDir << std::endl;
        return prefixDir;
    }

    const char *getInstallBaseDirectory()
    {
        static const string base = internalGetInstallBase();

        return base.length() > 0 ? base.c_str() : nullptr;
    }

    static string internalGetWebSiteDirectory()
    {
        static const char *prefix = getInstallBaseDirectory();
        std::cout << "Test " << prefix << std::endl;
        if (prefix == nullptr) {
            return "";
        }
        string prefixDir = prefix;
        prefixDir += "share/speakerman/web/";
        string indexPath = prefixDir;
        indexPath += "index.html";
        if (access(indexPath.c_str(), F_OK) == 0) {
            std::cout << "Web site directory: " << prefixDir << std::endl;
            return prefixDir;
        }
        std::cout << "Test " << prefixDir << std::endl;

        return "";
    }

    const char *getWebSiteDirectory()
    {
        static string dir = internalGetWebSiteDirectory();

        return dir.length() > 0 ? dir.c_str() : nullptr;
    }

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

    template<typename T>
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

    static bool readBool(int &variable, const char *key, const char *value)
    {
        if (strncasecmp("true", value, 5) == 0 || strncasecmp("yes", value, 5) == 0 || strncmp("1", value, 2) == 0) {
            variable = 1;
            return true;
        }
        if (strncasecmp("false", value, 5) == 0 || strncasecmp("no", value, 5) == 0 || strncmp("0", value, 2) == 0) {
            variable = 0;
            return true;
        }
        if (*value == 0) {
            return true;
        }
        std::cerr << "E: Invalid value for \"" << key << "\": " << value << std::endl;
        return false;
    }

    template<typename T, size_t N>
    static bool
    readNumbers(FixedSizeArray<T, N> &array, const char *key, const char *value, T min, T max, bool forceWithin = false)
    {
        using Type = typename ValueManager<T>::Value;
        const char *parsePos = value;

        for (size_t i = 0; i < array.size(); i++) {
            Type tempValue;
            char *endptr = nullptr;
            if (parsePos && *parsePos) {
                tempValue = ValueManager<T>::parse(parsePos, &endptr);
                if (tempValue == 0 && !(*endptr == 0 || *endptr == ',' || *endptr == ';')) {
                    std::cerr << "E: Invalid value for \"" << key << "\"[" << i << ": " << parsePos << std::endl;
                    return false;
                }
                bool outOfRange = tempValue < min || tempValue > max;
                if (outOfRange) {
                    cerr << "W: value for \"" << key << "\"[" << i << ": value " << tempValue << " out of [" << min
                         << ".." << max << "]: ";
                    if (!forceWithin) {
                        cerr << "not set!" << endl;
                        return false;
                    }
                    tempValue = Value<Type>::force_between(tempValue, min, max);
                    cerr << "set to " << tempValue << endl;
                }
                parsePos = endptr && *endptr ? endptr + 1 : nullptr;
            }
            else {
                tempValue = UnsetValue<T>::value;
            }

            array[i] = static_cast<T>(tempValue);
        }

        return true;
    }

    template<typename T>
    static void setDefault(T &value, T defaultValue)
    {
        if (value != UnsetValue<T>::value) {
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

    static void resetStream(istream &stream)
    {
        stream.clear(istream::eofbit);
        stream.seekg(0, stream.beg);
        if (stream.fail()) {
            throw ReadConfigException::noSeek();
        }
    }

    typedef struct
    {
        SpeakermanConfig &config;
        bool initial;
    } SpeakermanConfigCallbackData;


    typedef struct
    {
        GroupConfig &config;
        size_t groupId;
        bool initial;
    } GroupConfigCallbackData;

    typedef struct
    {
        EqualizerConfig &config;
        size_t groupId;
        size_t eqId;
        bool initial;
    } EqConfigCallbackData;

    static speakerman::config::CallbackResult readGlobalCallback(const char *key, const char *value, void *data)
    {
        auto config = static_cast<SpeakermanConfigCallbackData *>(data);

        if (isKey(key, config->initial, -1, -1, SpeakermanConfig::KEY_GROUP_COUNT)) {
            readNumber(config->config.groups, key, value, SpeakermanConfig::MIN_GROUPS, SpeakermanConfig::MAX_GROUPS);
        }
        else if (isKey(key, config->initial, -1, -1, SpeakermanConfig::KEY_CHANNELS)) {
            readNumber(config->config.groupChannels, key, value, SpeakermanConfig::MIN_GROUP_CHANNELS,
                       SpeakermanConfig::MAX_GROUP_CHANNELS);
        }
        else if (isKey(key, config->initial, -1, -1, SpeakermanConfig::KEY_SUB_THRESHOLD)) {
            readNumber(config->config.relativeSubThreshold, key, value, SpeakermanConfig::MIN_REL_SUB_THRESHOLD,
                       SpeakermanConfig::MAX_REL_SUB_THRESHOLD, true);
        }
        else if (isKey(key, config->initial, -1, -1, SpeakermanConfig::KEY_SUB_DELAY)) {
            readNumber(config->config.subDelay, key, value, SpeakermanConfig::MIN_SUB_DELAY,
                       SpeakermanConfig::MAX_SUB_DELAY, true);
        }
        else if (isKey(key, config->initial, -1, -1, SpeakermanConfig::KEY_SUB_OUTPUT)) {
            readNumber(config->config.subOutput, key, value, SpeakermanConfig::MIN_SUB_OUTPUT,
                       SpeakermanConfig::MAX_SUB_OUTPUT, true);
        }
        else if (isKey(key, config->initial, -1, -1, SpeakermanConfig::KEY_INPUT_OFFSET)) {
            readNumber(config->config.inputOffset, key, value, SpeakermanConfig::MIN_INPUT_OFFSET,
                       SpeakermanConfig::MAX_INPUT_OFFSET, true);
        }
        else if (isKey(key, config->initial, -1, -1, SpeakermanConfig::KEY_CROSSOVERS)) {
            readNumber(config->config.crossovers, key, value, SpeakermanConfig::MIN_CROSSOVERS,
                       SpeakermanConfig::MAX_CROSSOVERS, true);
        }
        else if (isKey(key, config->initial, -1, -1, SpeakermanConfig::KEY_GENERATE_NOISE)) {
            readBool(config->config.generateNoise, key, value);
        }
        return speakerman::config::CallbackResult::CONTINUE;
    }

    static speakerman::config::CallbackResult readGroupCallback(const char *key, const char *value, void *data)
    {
        auto config = static_cast<GroupConfigCallbackData *>(data);
        if (isKey(key, config->initial, config->groupId, -1, GroupConfig::KEY_EQ_COUNT)) {
            readNumber(config->config.eqs, key, value, GroupConfig::MIN_EQS, GroupConfig::MAX_EQS);
        }
        else if (isKey(key, config->initial, config->groupId, -1, GroupConfig::KEY_THRESHOLD)) {
            readNumber(config->config.threshold, key, value, GroupConfig::MIN_THRESHOLD, GroupConfig::MAX_THRESHOLD,
                       true);
        }
        else if (isKey(key, config->initial, config->groupId, -1, GroupConfig::KEY_VOLUME)) {
            FixedSizeArray<double, SpeakermanConfig::MAX_GROUPS> volumes;
            readNumbers(volumes, key, value, GroupConfig::MIN_VOLUME, GroupConfig::MAX_VOLUME, true);
            for (size_t i = 0; i < SpeakermanConfig::MAX_GROUPS; i++) {
                config->config.volume[i] = volumes[i];
            }
        }
        else if (isKey(key, config->initial, config->groupId, -1, GroupConfig::KEY_DELAY)) {
            readNumber(config->config.delay, key, value, GroupConfig::MIN_DELAY, GroupConfig::MAX_DELAY, true);
        }
        else if (isKey(key, config->initial, config->groupId, -1, GroupConfig::KEY_USE_SUB)) {
            readBool(config->config.use_sub, key, value);
        }
        else if (isKey(key, config->initial, config->groupId, -1, GroupConfig::KEY_MONO)) {
            readBool(config->config.mono, key, value);
        }
        return speakerman::config::CallbackResult::CONTINUE;
    }

    static speakerman::config::CallbackResult readEqCallback(const char *key, const char *value, void *data)
    {
        auto config = static_cast<EqConfigCallbackData *>(data);

        if (isKey(key, config->initial, config->groupId, config->eqId, EqualizerConfig::KEY_CENTER)) {
            readNumber(config->config.center, key, value, EqualizerConfig::MIN_CENTER_FREQ,
                       EqualizerConfig::MAX_CENTER_FREQ);
        }
        else if (isKey(key, config->initial, config->groupId, config->eqId, EqualizerConfig::KEY_GAIN)) {
            readNumber(config->config.gain, key, value, EqualizerConfig::MIN_GAIN, EqualizerConfig::MAX_GAIN, true);
        }
        else if (isKey(key, config->initial, config->groupId, config->eqId, EqualizerConfig::KEY_BANDWIDTH)) {
            readNumber(config->config.bandwidth, key, value, EqualizerConfig::MIN_BANDWIDTH,
                       EqualizerConfig::MAX_BANDWIDTH, true);
        }
        return speakerman::config::CallbackResult::CONTINUE;
    }

    static void
    readEq(EqConfigCallbackData &data, istream &stream, config::Reader &reader, const SpeakermanConfig &basedUpon)
    {
        if (data.eqId >= GroupConfig::MAX_EQS) {
            return;
        }
        resetStream(stream);
        EqualizerConfig &config = data.config;

        auto result = reader.read(stream, readEqCallback, &data);
        if (result != config::ReadResult::SUCCESS && result != config::ReadResult::STOPPED) {
            throw runtime_error("Parse error");
        }

        const EqualizerConfig &defaulConfig = basedUpon.group[data.groupId].eq[data.eqId];
        setDefault(config.center, defaulConfig.center);
        setDefault(config.gain, defaulConfig.gain);
        setDefault(config.bandwidth, defaulConfig.bandwidth);
    }

    static void
    readGroup(GroupConfigCallbackData &data, istream &stream, config::Reader &reader, const SpeakermanConfig &basedUpon)
    {
        if (data.groupId >= SpeakermanConfig::MAX_GROUPS) {
            return;
        }
        resetStream(stream);
        GroupConfig &config = data.config;

        auto result = reader.read(stream, readGroupCallback, &data);
        if (result != config::ReadResult::SUCCESS && result != config::ReadResult::STOPPED) {
            throw runtime_error("Parse error");
        }

        const GroupConfig &defaultConfig = basedUpon.group[data.groupId];
        setDefault(config.eqs, defaultConfig.eqs);
        setDefault(config.threshold, defaultConfig.threshold);
        for (size_t i = 0; i < SpeakermanConfig::MAX_GROUPS; i++) {
            setDefault(config.volume[i], defaultConfig.volume[i]);
        }
        setDefault(config.delay, defaultConfig.delay);
        setDefault(config.use_sub, defaultConfig.use_sub);
        setDefault(config.mono, defaultConfig.mono);

        for (size_t eq = 0; eq < config.eqs; eq++) {
            EqConfigCallbackData info{config.eq[eq], data.groupId, eq, data.initial};
            readEq(info, stream, reader, basedUpon);
        }
    }

    static void
    readGlobals(SpeakermanConfig &config, istream &stream, config::Reader &reader, const SpeakermanConfig &basedUpon,
                bool initial)
    {
        resetStream(stream);

        SpeakermanConfigCallbackData data{config, initial};
        auto result = reader.read(stream, readGlobalCallback, &data);
        if (result != config::ReadResult::SUCCESS && result != config::ReadResult::STOPPED) {
            throw runtime_error("Parse error");
        }

        setDefault(config.groups, basedUpon.groups);
        setDefault(config.groupChannels, basedUpon.groupChannels);
        setDefault(config.relativeSubThreshold, basedUpon.relativeSubThreshold);
        setDefault(config.subDelay, basedUpon.subDelay);
        setDefault(config.subOutput, basedUpon.subOutput);
        setDefault(config.crossovers, basedUpon.crossovers);
        setDefault(config.inputOffset, basedUpon.inputOffset);
        setDefault(config.generateNoise, basedUpon.generateNoise);

        for (size_t g = 0; g < config.groups; g++) {
            GroupConfigCallbackData data{config.group[g], g, initial};
            readGroup(data, stream, reader, basedUpon);
        }
    }

    static void
    actualReadConfig(SpeakermanConfig &config, istream &stream, const SpeakermanConfig &basedUpon, bool initial)
    {
        config::Reader reader;
        config = SpeakermanConfig::unsetConfig();
        readGlobals(config, stream, reader, basedUpon, initial);
    }

    class StreamOwner
    {
        ifstream &stream;
        bool owns;

        void operator=(const StreamOwner &source)
        {}
        void operator=(StreamOwner &&source) noexcept
        {}
    public:
        explicit StreamOwner(ifstream &owned) : stream(owned), owns(true)
        {}
        StreamOwner(const StreamOwner &source) : stream(source.stream), owns(false)
        {}
        StreamOwner(StreamOwner &&source) noexcept : stream(source.stream), owns(true)
        {
            source.owns = false;
        }

        ~StreamOwner()
        {
            if (owns && stream.is_open()) {
                stream.close();
            }
        }
    };

    long long getFileTimeStamp(const char *fileName)
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
        long long stamp = getFileTimeStamp(configFileName());
        stream.open(configFileName());

        StreamOwner owner(stream);
        if (!stream.is_open()) {
            cerr << "Stream not open..." << endl;
            return basedUpon;
        }
        try {
            actualReadConfig(result, stream, basedUpon, initial);
        }
        catch (const ReadConfigException &e) {
            cerr << "E: " << e.what() << endl;
            return basedUpon;
        }
        dumpSpeakermanConfig(result, cout);
        result.timeStamp = stamp;
        return result;
    }

    SpeakermanConfig readSpeakermanConfig(bool initial)
    {
        SpeakermanConfig basedUpon = SpeakermanConfig::defaultConfig();
        return readSpeakermanConfig(basedUpon, initial);
    }

    void dumpSpeakermanConfig(const SpeakermanConfig &dump, ostream &output)
    {
        const char *assgn = " = ";
        output << "# Speakerman configuration dump" << endl << endl;
        output << "# Global" << endl;
        output << getConfigKey(-1, -1, SpeakermanConfig::KEY_GROUP_COUNT) << assgn << dump.groups << endl;
        output << getConfigKey(-1, -1, SpeakermanConfig::KEY_CHANNELS) << assgn << dump.groupChannels << endl;
        output << getConfigKey(-1, -1, SpeakermanConfig::KEY_SUB_THRESHOLD) << assgn << dump.relativeSubThreshold
               << endl;
        output << getConfigKey(-1, -1, SpeakermanConfig::KEY_SUB_DELAY) << assgn << dump.subDelay << endl;
        output << getConfigKey(-1, -1, SpeakermanConfig::KEY_SUB_OUTPUT) << assgn << dump.subOutput << endl;
        output << getConfigKey(-1, -1, SpeakermanConfig::KEY_INPUT_OFFSET) << assgn << dump.inputOffset << endl;
        output << getConfigKey(-1, -1, SpeakermanConfig::KEY_CROSSOVERS) << assgn << dump.crossovers << endl;
        output << getConfigKey(-1, -1, SpeakermanConfig::KEY_GENERATE_NOISE) << assgn << dump.generateNoise << endl;

        for (size_t group = 0; group < dump.groups; group++) {
            const GroupConfig &groupConfig = dump.group[group];
            output << getConfigKey(group, -1, GroupConfig::KEY_EQ_COUNT) << assgn << groupConfig.eqs << endl;
            output << getConfigKey(group, -1, GroupConfig::KEY_THRESHOLD) << assgn << groupConfig.threshold << endl;
            output << getConfigKey(group, -1, GroupConfig::KEY_DELAY) << assgn << groupConfig.delay << endl;
            output << getConfigKey(group, -1, GroupConfig::KEY_USE_SUB) << assgn << groupConfig.use_sub << endl;
            output << getConfigKey(group, -1, GroupConfig::KEY_MONO) << assgn << groupConfig.mono << endl;
            output << getConfigKey(group, -1, GroupConfig::KEY_VOLUME) << assgn << "[";
            for (size_t i = 0; i < dump.groups; i++) {
                if (i > 0) {
                    output << ' ';
                }
                output << groupConfig.volume[i];
            }
            output << "]" << endl;

            for (size_t eq = 0; eq < std::min(GroupConfig::MAX_EQS, groupConfig.eqs); eq++) {
                const EqualizerConfig &eqConfig = groupConfig.eq[eq];
                output << getConfigKey(group, eq, EqualizerConfig::KEY_CENTER) << assgn << eqConfig.center << endl;
                output << getConfigKey(group, eq, EqualizerConfig::KEY_GAIN) << assgn << eqConfig.gain << endl;
                output << getConfigKey(group, eq, EqualizerConfig::KEY_BANDWIDTH) << assgn << eqConfig.bandwidth
                       << endl;
            }
        }
        output << "Timestamp: " << dump.timeStamp << endl;
    }

} /* End of namespace speakerman */

