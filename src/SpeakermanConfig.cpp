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

#ifndef SPEAKERMAN_INSTALL_PREFIX
#error "Need to define install prefix with -DSPEAKERMAN_INSTALL_PREFIX=<>"
#else
#define TO_STR2(x) #x
#define TO_STR(x) TO_STR2(x)
static constexpr const char * INSTALLATION_PREFIX = TO_STR(SPEAKERMAN_INSTALL_PREFIX);
#endif


#include <string>
#include <cstring>
#include <cmath>
#include <mutex>
#include <sys/stat.h>
#include <unistd.h>
#include <tdap/Value.hpp>
#include <tdap/FixedSizeArray.hpp>
#include <speakerman/SpeakermanConfig.hpp>
#include <speakerman/utils/Config.hpp>

namespace speakerman {

    using namespace std;
    using namespace tdap;

    class ReadConfigException : public runtime_error
    {
    public:
        explicit ReadConfigException(const char *message) : runtime_error(
                message)
        {

        }

        static ReadConfigException noSeek()
        {
            return ReadConfigException("Could not reset file read position");
        }
    };
    template<typename T>
    struct UnsetValue
    {

    };

    template<>
    struct UnsetValue<size_t>
    {
        static constexpr size_t value = static_cast<size_t>(-1);

        static bool is(size_t test)
        {
            return test == value;
        }
    };


    template<>
    struct UnsetValue<double>
    {
        static constexpr double value =
                std::numeric_limits<double>::quiet_NaN();

        union Tester {
            long long l;
            double f;

            Tester(double v) : l(0) { f = v; }

            bool operator == (const Tester &other) const
            { return l == other.l; }
        };

        static bool is(double test)
        {
            static const Tester sNan = { std::numeric_limits<double>::signaling_NaN() };
            static const Tester qNan = { value };
            Tester t {test};
            return t == sNan || t == qNan;
        }

    };

    template<>
    struct UnsetValue<int>
    {
        static constexpr int value = -1;

        static bool is(size_t test)
        {
            return test == value;
        }
    };

    template<typename T>
    static void unset_config_value(T &value)
    {
        value = UnsetValue<T>::value;
    }

    template<typename T>
    static bool set_if_unset_config_value(T &value, T value_if_unset)
    {
        if (UnsetValue<T>::is(value)) {
            value = value_if_unset;
            return true;
        }
        return false;
    }

    template<typename T>
    static bool
    set_if_unset_or_invalid_config_value(T &value, T value_if_unset, T minimum,
                                         T maximum)
    {
        if (UnsetValue<T>::is(value) || value < minimum ||
            value > maximum) {
            value = value_if_unset;
            return true;
        }
        return false;
    }

    template<typename T>
    static bool unset_if_invalid(T &value, T minimum, T maximum)
    {
        if (value < minimum || value > maximum) {
            value = UnsetValue<T>::value;
            return true;
        }
        return false;
    }

    template<typename T>
    static void box_if_out_of_range(T &value, T minimum, T maximum)
    {
        if (UnsetValue<T>::is(value)) {
            return;
        }
        if (value < minimum) {
            value = minimum;
        }
        else if (value > maximum) {
            value = maximum;
        }
    }

    template<typename T>
    static void
    box_if_out_of_range(T &value, T value_if_unset, T minimum, T maximum)
    {
        if (UnsetValue<T>::is(value)) {
            value = value_if_unset;
        }
        else if (value < minimum) {
            value = minimum;
        }
        else if (value > maximum) {
            value = maximum;
        }
    }



    template<typename T, int type>
    struct ValueParser_
    {

    };

    template<typename T>
    struct ValueParser_<T, 1>
    {
        static_assert(is_integral<T>::value,
                      "Expected integral type parameter");
        using V = long long int;

        static bool parse(T &field, const char *value, char *&end)
        {
            V parsed = strtoll(value, &end, 10);
            if (*end == '\0' || config::isWhiteSpace(*end) || config::isCommentStart(*end)) {
                field = tdap::Value<T>::force_between(
                        parsed,
                        std::numeric_limits<T>::lowest(),
                        std::numeric_limits<T>::max());
                return true;
            }
            cerr << "Error parsing integer" << endl;
            return false;
        }
    };

    template<typename T>
    struct ValueParser_<T, 2>
    {
        static_assert(is_integral<T>::value,
                      "Expected integral type parameter");
        using V = int;

        static bool
        matches(const char *keyword, const char *value, const char *end)
        {
            size_t scan_length = end - value;
            size_t key_length = strnlen(keyword, 128);
            if (scan_length < key_length) {
                return false;
            }

            return strncasecmp(keyword, value, key_length) == 0 &&
                   value[key_length] == '\0' ||
                   config::isWhiteSpace(value[key_length]);
        }

        static bool parse(T &field, const char *value, char *&end)
        {
            for (end = const_cast<char*>(value); config::isAlphaNum(*end); end++) {}

            if (matches("true", value, end) || matches("1", value, end) ||
                matches("yes", value, end)) {
                field = 1;
                return true;
            }
            if (matches("false", value, end) || matches("0", value, end) ||
                matches("no", value, end)) {
                field = 0;
                return true;
            }
            cerr << "Error parsing boolean" << endl;
            return false;
        }
    };

    template<typename T>
    struct ValueParser_<T, 3>
    {
        static_assert(is_floating_point<T>::value,
                      "Expected floating point type parameter");
        using V = long double;

        static bool parse(T &field, const char *value, char *&end)
        {
            V parsed = strtold(value, &end);
            if (*end == '\0' || config::isWhiteSpace(*end) || config::isCommentStart(*end)) {
                field = tdap::Value<V>::force_between(
                        parsed,
                        std::numeric_limits<T>::lowest(),
                        std::numeric_limits<T>::max());
                return true;
            }
            cerr << "Error parsing float" << endl;
            return false;
        }
    };

    template<typename T>
    static constexpr int get_value_parser_type()
    {
        return
                std::is_floating_point<T>::value ? 3 :
                std::is_same<int, T>::value ? 2 :
                std::is_integral<T>::value ? 1 : 4;

    }

    template<typename T>
    struct ValueParser
            : public ValueParser_<T, get_value_parser_type<T>()>
    {

    };

    template<typename T, size_t N>
    static int read_value_array(FixedSizeArray<T, N> &values, const char *value,
                                char *&end)
    {
        using Parser = ValueParser<T>;
        const char *read_from = value;
        size_t i;
        for (i = 0; i < N; i++) {
            T parsed;
            if (!Parser::parse(parsed, read_from, end)) {
                return -1;
            }
            values[i] = parsed;
            bool hadDelimiter = false;
            while (config::isWhiteSpace(*end) || *end == ';' || *end == ',') {
                if (*end == ';' || *end == ',') {
                    if (hadDelimiter) {
                        return -1;
                    }
                    hadDelimiter = true;
                }
                end++;
            }
            if (*end == '\0' || config::isCommentStart(*end)) {
                break;
            }
            read_from = end;
        }
        return i + 1;
    };

    using CountValueParser = ValueParser<size_t>;
    using BooleanValueParser = ValueParser<bool>;
    using FloatValueParser = ValueParser<double>;

    class VariableReader
    {
        bool runtime_changeable_ = false;
    protected:
        virtual void read(SpeakermanConfig &config, const char *key,
                          const char *value) const = 0;

        virtual void copy(SpeakermanConfig &config, const SpeakermanConfig &basedUpon) const = 0;

    public:
        bool runtime_changeable() const
        { return runtime_changeable_; }

        virtual void write(const SpeakermanConfig &config, const char *key, ostream &stream) const = 0;

        VariableReader(bool runtime_changeable) : runtime_changeable_(
                runtime_changeable)
        {}

        bool read(SpeakermanConfig &config, const char *key,
                          const char *value, const SpeakermanConfig &basedUpon) const
        {
            if (&config == &basedUpon || runtime_changeable_) {
                read(config, key, value);
                return true;
            }
            else {
                copy(config, basedUpon);
            }
            cout << "Ignoring (not runtime changeable): " << key << endl;
            return false;
        }

        virtual ~VariableReader() = default;
    };

    template<size_t SIZE>
    class PositionedVariableReader : public VariableReader
    {
        size_t offset_;

        static size_t long
        valid_offset(const SpeakermanConfig &config, const void *field)
        {
            const char *config_address = static_cast<const char *>(static_cast<const void *>(&config));
            const char *field_address = static_cast<const char *>(field);
            if (field_address >= config_address) {
                size_t offset = field_address - config_address;
                if (offset <= sizeof(SpeakermanConfig) - SIZE) {
                    return offset;
                }
            }
            throw std::invalid_argument("Invalid variable offset");
        }

    protected:
        size_t offset() const
        {
            return offset_;
        }

        void * variable(SpeakermanConfig &config) const
        {
            char *config_address = static_cast<char *>(static_cast<void *>(&config));
            return static_cast<void*>(config_address + offset_);
        }

        const void * variable(const SpeakermanConfig &config) const
        {
            const char *config_address = static_cast<const char *>(static_cast<const void *>(&config));
            return static_cast<const void*>(config_address + offset_);
        }

        void copy(SpeakermanConfig &config, const SpeakermanConfig &basedUpon) const
        {
            memcpy(variable(config), variable(basedUpon), SIZE);
        }


    public:
        PositionedVariableReader(bool runtime_changeable, const SpeakermanConfig &config, const void *field) :
                VariableReader(runtime_changeable),
                offset_(valid_offset(config, field))
        {

        }
    };

    template<typename T>
    class TypedVariableReader : public PositionedVariableReader<sizeof(T)>
    {
        using PositionedVariableReader<sizeof(T)>::variable;
    protected:
        void read(SpeakermanConfig &config, const char *key,
                  const char *value) const override
        {

            T &field = *static_cast<T *>(variable(config));
            char *end = const_cast<char *>(value);
            if (!ValueParser<T>::parse(field, value, end)) {
                cerr << "Error parsing \"" << key << "\" @" << (end - value)
                     << ": " << value << endl;
            }
        }
    public:
        TypedVariableReader(bool runtime_changeable, const SpeakermanConfig &config, const T &field)
                : PositionedVariableReader<sizeof(T)>(runtime_changeable, config, &field)
        {};

        void write(const SpeakermanConfig &config, const char *key, ostream &stream) const
        {
            const T &field = *static_cast<const T *>(variable(config));
            if (!UnsetValue<T>::is(field)) {
                stream << key << " = " << field << endl;
            }
        }
    };

    template<typename T, size_t N>
    class TypedArrayVariableReader : public PositionedVariableReader<N * sizeof(T)>
    {
        using PositionedVariableReader<N * sizeof(T)>::variable;
    protected:

        void read(SpeakermanConfig &config, const char *key,
                  const char *value) const override
        {
            FixedSizeArray<T, N> fieldValues;
            T *field = static_cast<T *>(variable(config));
            char *end = const_cast<char*>(value);
            int count = read_value_array(fieldValues, value, end);
            if (count < 0) {
                cerr << "Error parsing \"" << key << "\" @" << (end - value)
                     << ": " << value << endl;
                return;
            }

            size_t i;
            for (i = 0; i < count; i++) {
                field[i] = fieldValues[i];
            }
            for (; i < N; i++) {
                field[i] = UnsetValue<T>::value;
            }
        }

    public:
        TypedArrayVariableReader(bool runtime_changeable, const SpeakermanConfig &config,
                                 const T &field)
                : PositionedVariableReader<N * sizeof(T)>(runtime_changeable, config, &field)
        {};

        void write(const SpeakermanConfig &config, const char * key, ostream &stream) const
        {
            const T *field = static_cast<const T *>(variable(config));
            bool print = false;

            for (size_t i = 0; i < N; i++) {
                if (UnsetValue<T>::is(field[i])) {
                    break;
                }
                else {
                    print = true;
                    break;
                }
            }
            if (print) {
                stream << key << " =";
                for (size_t i = 0; i < N; i++) {
                    if (UnsetValue<T>::is(field[i])) {
                        break;
                    }
                    else {
                        stream << " " << field[i];
                    }
                }
                stream << endl;
            }
        }
    };

    enum class ReaderStatus
    {
        SKIP,
        SUCCESS,
        FAULT
    };

    class KeyVariableReader
    {
        const string key_;
        const VariableReader *reader_;
    public:
        KeyVariableReader(const string &key, VariableReader *reader) :
            key_(key), reader_(reader) {}

        ~KeyVariableReader()
        {
            delete reader_;
        }

        const string &get_key() const
        { return key_; }

        static ReaderStatus
        skip_to_key_start(const char *&result, const char *line)
        {
            if (line == nullptr || *line == '\0') {
                return ReaderStatus::SKIP;
            }
            const char *start;
            for (start = line; *start != '\0' && start - line <= 1024; start++) {
                char c = *start;
                if (config::isCommentStart(c)) {
                    return ReaderStatus::SKIP;
                }
                if (config::isAlpha(c)) {
                    result = start;
                    return ReaderStatus::SUCCESS;
                }
                if (!config::isWhiteSpace(c)) {
                    cerr << "Unexpected characters @ " << (start - line) << ": "
                         << line << endl;
                    break;
                }
            }
            return ReaderStatus::FAULT;
        }

        ReaderStatus read_key(const char *&after_key, const char *start)
        {
            if (strncasecmp(key_.c_str(), start, key_.length()) != 0) {
                return ReaderStatus::SKIP;
            }
            char end = start[key_.length()];
            if (config::isWhiteSpace(end) || end == '=') {
                after_key = start + key_.length();
                return ReaderStatus::SUCCESS;
            }
            return ReaderStatus::SKIP;
        }

        static ReaderStatus
        skip_assignment(const char *&value, const char *start, const char *line)
        {
            const char *rd = start;
            while (config::isWhiteSpace(*rd)) {
                rd++;
            }
            if (*rd != '=') {
                cerr << "Unexpected character @ " << (rd - line) << ": " << line
                     << endl;
                cout << "1line :" << line << endl;
                cout << "1start:" << start << endl;
                return ReaderStatus::FAULT;
            }
            rd++;
            while (config::isWhiteSpace(*rd)) {
                rd++;
            }
            if (*rd == '\0') {
                cerr << "Unexpected end of line @ " << (rd - line) << ": "
                     << line << endl;
                cout << "2line :" << line << endl;
                cout << "2start:" << start << endl;
                return ReaderStatus::FAULT;
            }
            value = rd;
            return ReaderStatus::SUCCESS;
        }


        bool read(SpeakermanConfig &manager, const string &key, const char *value,
              const SpeakermanConfig &basedUpon)
        {
            return reader_->read(manager, key.c_str(), value, basedUpon);
        }

        void write(const SpeakermanConfig & config, ostream &output) const {
            reader_->write(config, key_.c_str(), output);
        }
    };

    class ConfigManager : protected SpeakermanConfig
    {
        KeyVariableReader **readers_ = nullptr;
        size_t capacity_ = 0;
        size_t size_ = 0;

        void ensure_capacity(size_t new_capacity)
        {
            if (new_capacity == 0) {
                for (size_t i = 0; i < size_; i++) {
                    if (readers_[i] != nullptr) {
                        delete readers_[i];
                        readers_[i] = nullptr;
                    }
                }
                size_ = capacity_ = 0;
                return;
            }
            if (new_capacity <= capacity_) {
                return;
            }
            size_t actual_capacity = max(new_capacity, 3 * max(capacity_, (size_t)7) / 2);
            size_t moves = min(actual_capacity, size_);
            KeyVariableReader **new_readers = new KeyVariableReader*[actual_capacity];
            size_t reader;
            for (reader = 0; reader < moves; reader++) {
                new_readers[reader] = readers_[reader];
            }
            for (; reader < actual_capacity; reader++) {
                new_readers[reader] = nullptr;
            }
            readers_ = new_readers;
            capacity_ = actual_capacity;
        }

        /**
         * Adds and adopts ownership of a reader.
         *
         * @param new_reader The added reader
         * @throws invalid_argument if reader is nullptr
         */
        void add(KeyVariableReader *new_reader)
        {
            if (new_reader == nullptr) {
                throw std::invalid_argument("Cannot add NULL reader");
            }
            size_t new_size = size_ + 1;
            ensure_capacity(new_size);
            readers_[size_] = new_reader;
            size_ = new_size;
        }

        template<typename T> void add_reader(const string &name, bool runtime_changeable, T &field)
        {
            add(new KeyVariableReader(name, new TypedVariableReader<T>(runtime_changeable, *this, field)));
        }

        template<typename T, size_t N> void add_array_reader(const string &name, bool runtime_changeable, T &field)
        {
            add(new KeyVariableReader(name, new TypedArrayVariableReader<T, N>(runtime_changeable, *this, field)));
        }

    public:

        ConfigManager()
        {
            add_reader(KEY_SNIPPET_GROUP_COUNT, false, groups);
            add_reader(KEY_SNIPPET_CHANNELS, false, groupChannels);
            add_reader(KEY_SNIPPET_CROSSOVERS, false, crossovers);

            add_reader(KEY_SNIPPET_SUB_THRESHOLD, true, relativeSubThreshold);
            add_reader(KEY_SNIPPET_SUB_DELAY, true, subDelay);
            add_reader(KEY_SNIPPET_SUB_OUTPUT, false, subOutput);

            add_reader(KEY_SNIPPET_GENERATE_NOISE, true, generateNoise);
            add_reader(KEY_SNIPPET_INPUT_OFFSET, false, inputOffset);

            string key;
            for (size_t group_idx = 0; group_idx < SpeakermanConfig::MAX_GROUPS; group_idx++) {
                string groupKey = GroupConfig::KEY_SNIPPET_GROUP;
                groupKey += "/";
                groupKey += (char) (group_idx + '0');
                groupKey += "/";

                key = groupKey;
                key += GroupConfig::KEY_SNIPPET_EQ_COUNT;
                add_reader(key, true, group[group_idx].eqs);
                key = groupKey;
                key += GroupConfig::KEY_SNIPPET_THRESHOLD;
                add_reader(key, true, group[group_idx].threshold);
                key = groupKey;
                key += GroupConfig::KEY_SNIPPET_VOLUME;
                add_array_reader<double, MAX_SPEAKERMAN_GROUPS>(key, true, group[group_idx].volume[0]);
                key = groupKey;
                key += GroupConfig::KEY_SNIPPET_DELAY;
                add_reader(key, true, group[group_idx].delay);
                key = groupKey;
                key += GroupConfig::KEY_SNIPPET_USE_SUB;
                add_reader(key, true, group[group_idx].use_sub);
                key = groupKey;
                key += GroupConfig::KEY_SNIPPET_MONO;
                add_reader(key, true, group[group_idx].mono);

                string eqBase = groupKey;
                eqBase += EqualizerConfig::KEY_SNIPPET_EQUALIZER;
                eqBase += "/";
                for (size_t eq_idx = 0; eq_idx < GroupConfig::MAX_EQS; eq_idx++) {
                    string eqKey = eqBase;
                    eqKey += (char) ('0' + eq_idx);
                    eqKey += "/";

                    key = eqKey;
                    key += EqualizerConfig::KEY_SNIPPET_CENTER;
                    add_reader(key, true, group[group_idx].eq[eq_idx].center);
                    key = eqKey;
                    key += EqualizerConfig::KEY_SNIPPET_GAIN;
                    add_reader(key, true, group[group_idx].eq[eq_idx].gain);
                    key = eqKey;
                    key += EqualizerConfig::KEY_SNIPPET_BANDWIDTH;
                    add_reader(key, true, group[group_idx].eq[eq_idx].bandwidth);
                }
            }

            for (size_t band_idx = 0;
                band_idx <= SpeakermanConfig::MAX_CROSSOVERS; band_idx++) {
                string bandKey = BandConfig::KEY_SNIPPET_BAND;
                bandKey += "/";
                bandKey += (char) (band_idx + '0');
                bandKey += "/";

                key = bandKey;
                key += BandConfig::KEY_SNIPPET_MAXIMUM_WINDOW_SECONDS;
                add_reader(key, false, band[band_idx].maximum_window_seconds);
                key = bandKey;
                key += BandConfig::KEY_SNIPPET_PERCEPTIVE_TO_MAXIMUM_WINDOW_STEPS;
                add_reader(key, false, band[band_idx].perceptive_to_maximum_window_steps);
                key = bandKey;
                key += BandConfig::KEY_SNIPPET_PERCEPTIVE_TO_PEAK_STEPS;
                add_reader(key, false, band[band_idx].perceptive_to_peak_steps);
                key = bandKey;
                key += BandConfig::KEY_SNIPPET_SMOOTHING_TO_WINDOW_RATIO;
                add_reader(key, false, band[band_idx].smoothing_to_window_ratio);
            }
            add_reader(GroupConfig::KEY_SNIPPET_EQ_COUNT, true, eqs);
            string eqBase = "";
            eqBase += EqualizerConfig::KEY_SNIPPET_EQUALIZER;
            eqBase += "/";
            for (size_t eq_idx = 0; eq_idx < GroupConfig::MAX_EQS; eq_idx++) {
                string eqKey = eqBase;
                eqKey += (char) ('0' + eq_idx);
                eqKey += "/";

                key = eqKey;
                key += EqualizerConfig::KEY_SNIPPET_CENTER;
                add_reader(key, true, eq[eq_idx].center);
                key = eqKey;
                key += EqualizerConfig::KEY_SNIPPET_GAIN;
                add_reader(key, true, eq[eq_idx].gain);
                key = eqKey;
                key += EqualizerConfig::KEY_SNIPPET_BANDWIDTH;
                add_reader(key, true, eq[eq_idx].bandwidth);
            }
        }

        size_t size() const
        { return size_; }

        bool read_line(SpeakermanConfig &config, const char * line, const SpeakermanConfig &basedUpon)
        {
            const char *key_start;

            ReaderStatus status = KeyVariableReader::skip_to_key_start(key_start, line);
            if (status != ReaderStatus::SUCCESS) {
                return status != ReaderStatus::FAULT;
            }
            KeyVariableReader *reader = nullptr;
            const char *after_key  = nullptr;

            for (size_t i = 0; i < size_; i++) {
                KeyVariableReader *r = readers_[i];
                status = r->read_key(after_key, key_start);
                if (status == ReaderStatus::SUCCESS) {
                    reader = r;
                }
                else if (status != ReaderStatus::SKIP) {
                    return false;
                }
            }

            if (reader == nullptr) {
                return true;
            }

            const char * value_start = nullptr;
            status = KeyVariableReader::skip_assignment(value_start, after_key, line);
            if (status != ReaderStatus::SUCCESS) {
                return false;
            }

            return reader->read(config, reader->get_key(), value_start, basedUpon);
        }

        void dump(const SpeakermanConfig &config, ostream &stream)
        {
            for (size_t i = 0; i < size(); i++) {
                readers_[i]->write(config, stream);
            }
        }

    };

    static ConfigManager config_manager;

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
        static constexpr const char *prefix = INSTALLATION_PREFIX;
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

    const char *configFileName()
    {
        static string name = getConfigFileName();

        return name.c_str();
    }

    static string internalGetWatchDogScript()
    {
        string watchDog = getInstallBaseDirectory();
        watchDog += "share/speakerman/script/speakerman-watchdog.sh";

        if (access(watchDog.c_str(), F_OK) == 0) {
            cout << "Watch-dog script: " << watchDog << endl;
            return watchDog;
        }

        return "";
    }

    const char * getWatchDogScript()
    {
        static string script = internalGetWatchDogScript();

        return script.length() > 0 ? script.c_str() : nullptr;
    }


    static void resetStream(istream &stream)
    {
        stream.clear(istream::eofbit);
        stream.seekg(0, stream.beg);
        if (stream.fail()) {
            throw ReadConfigException::noSeek();
        }
    }

    static void
    actualReadConfig(SpeakermanConfig &config, istream &stream,
                     const SpeakermanConfig &basedUpon, bool initial)
    {
        static constexpr size_t LINE_LENGTH = 1024;
        char line[LINE_LENGTH + 1];
        size_t line_pos = 0;
        config = SpeakermanConfig::unsetConfig();

        cout << "Reading config @ " << &config << endl;
        resetStream(stream);
        while (true) {
            char c = static_cast<char>(stream.get());
            if (stream.eof()) {
                break;
            }
            if (config::isLineDelimiter(c)) {
                if (line_pos != 0) {
                    line[std::min(line_pos, LINE_LENGTH)] = '\0';
                    config_manager.read_line(config, line, initial ? config : basedUpon);
                    line_pos = 0;
                }
            }
            else if (line_pos < LINE_LENGTH) {
                line[line_pos++] = c;
                continue;
            }
            else if (line_pos == LINE_LENGTH) {
                line[LINE_LENGTH] = '\0';
                config_manager.read_line(config, line, initial ? config : basedUpon);
                line_pos++;
            }
        }
        if (line_pos != 0) {
            line[std::min(line_pos, LINE_LENGTH)] = '\0';
            config_manager.read_line(config, line, initial ? config : basedUpon);
        }
        config.set_if_unset(basedUpon);
        config.threshold_scaling = basedUpon.threshold_scaling;
    }


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

    SpeakermanConfig
    readSpeakermanConfig(const SpeakermanConfig &basedUpon, bool initial)
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

    SpeakermanConfig readSpeakermanConfig()
    {
        SpeakermanConfig basedUpon = SpeakermanConfig::defaultConfig();
        return readSpeakermanConfig(basedUpon, true);
    }

    void dumpSpeakermanConfig(const SpeakermanConfig &dump, ostream &output)
    {
        output << "# Speakerman configuration dump" << endl << endl;
        config_manager.dump(dump, output);
    }

    void GroupConfig::set_if_unset(const GroupConfig &config_if_unset)
    {
        size_t eq_idx;
        if (set_if_unset_or_invalid_config_value(eqs, config_if_unset.eqs,
                                                 MIN_EQS, MAX_EQS)) {
            for (eq_idx = 0; eq_idx < eqs; eq_idx++) {
                eq[eq_idx] = config_if_unset.eq[eq_idx];
            }
        }
        else {
            for (eq_idx = 0; eq_idx < eqs; eq_idx++) {
                eq[eq_idx].set_if_unset(config_if_unset.eq[eq_idx]);
            }
        }
        for (; eq_idx < MAX_EQS; eq_idx++) {
            eq[eq_idx] = EqualizerConfig::unsetConfig();
        }

        for (size_t group = 0; group < MAX_SPEAKERMAN_GROUPS; group++) {
            set_if_unset_or_invalid_config_value(volume[group],
                                                 config_if_unset.volume[group],
                                                 MIN_VOLUME, MAX_VOLUME);
        }

        box_if_out_of_range(threshold, config_if_unset.threshold,
                            MIN_THRESHOLD, MAX_THRESHOLD);
        box_if_out_of_range(delay, config_if_unset.delay, MIN_DELAY,
                            MAX_DELAY);
        box_if_out_of_range(use_sub, config_if_unset.use_sub, 0, 1);
        box_if_out_of_range(mono, config_if_unset.mono, 0, 1);
    }

    const GroupConfig GroupConfig::defaultConfig(size_t group_id)
    {
        GroupConfig result;
        for (size_t i = 0; i < MAX_SPEAKERMAN_GROUPS; i++) {
            result.volume[i] == i == group_id ? DEFAULT_VOLUME : 0;
        }
        return result;
    }

    const GroupConfig GroupConfig::unsetConfig()
    {
        GroupConfig result;
        for (size_t i = 0; i < MAX_EQS; i++) {
            result.eq[i] = EqualizerConfig::unsetConfig();
        }
        for (size_t i = 0; i < MAX_SPEAKERMAN_GROUPS; i++) {
            unset_config_value(result.volume[i]);
        }
        unset_config_value(result.eqs);
        unset_config_value(result.threshold);
        unset_config_value(result.delay);
        unset_config_value(result.use_sub);
        unset_config_value(result.mono);
        return result;
    }

    const EqualizerConfig EqualizerConfig::unsetConfig()
    {
        return {UnsetValue<double>::value, UnsetValue<double>::value,
                UnsetValue<double>::value};
    }

    void
    EqualizerConfig::set_if_unset(const EqualizerConfig &base_config_if_unset)
    {
        if (set_if_unset_or_invalid_config_value(center,
                                                 base_config_if_unset.center,
                                                 MIN_CENTER_FREQ,
                                                 MAX_CENTER_FREQ)) {
            (*this) = base_config_if_unset;
        }
        else {
            unset_if_invalid(center, MIN_CENTER_FREQ, MAX_CENTER_FREQ);
            box_if_out_of_range(gain, DEFAULT_GAIN, MIN_GAIN, MAX_GAIN);
            box_if_out_of_range(bandwidth, DEFAULT_BANDWIDTH, MIN_BANDWIDTH, MAX_BANDWIDTH);
        }
    }

    const BandConfig BandConfig::unsetConfig()
    {
        BandConfig result;

        unset_config_value(result.smoothing_to_window_ratio);
        unset_config_value(result.maximum_window_seconds);
        unset_config_value(result.perceptive_to_peak_steps);
        unset_config_value(result.perceptive_to_maximum_window_steps);

        return result;
    }

    void BandConfig::set_if_unset(const BandConfig &config_if_unset)
    {
        box_if_out_of_range(smoothing_to_window_ratio,
                            config_if_unset.smoothing_to_window_ratio,
                            MIN_SMOOTHING_TO_WINDOW_RATIO,
                            MAX_SMOOTHING_TO_WINDOW_RATIO);
        box_if_out_of_range(maximum_window_seconds,
                            config_if_unset.maximum_window_seconds,
                            MIN_MAXIMUM_WINDOW_SECONDS,
                            MAX_MAXIMUM_WINDOW_SECONDS);
        box_if_out_of_range(perceptive_to_peak_steps,
                            config_if_unset.perceptive_to_peak_steps,
                            MIN_PERCEPTIVE_TO_PEAK_STEPS,
                            MAX_PERCEPTIVE_TO_PEAK_STEPS);
        box_if_out_of_range(perceptive_to_maximum_window_steps,
                            config_if_unset.perceptive_to_maximum_window_steps,
                            MIN_PERCEPTIVE_TO_MAXIMUM_WINDOW_STEPS,
                            MAX_PERCEPTIVE_TO_MAXIMUM_WINDOW_STEPS);
    }

    const SpeakermanConfig SpeakermanConfig::defaultConfig()
    {
        SpeakermanConfig result;
        for (size_t i = 0; i < MAX_SPEAKERMAN_GROUPS; i++) {
            result.group[i] = GroupConfig::defaultConfig(i);
        }
        for (size_t i = 0; i <= MAX_CROSSOVERS; i++) {
            result.band[i] = BandConfig::defaultConfig();
        }
        for (size_t i = 0; i < MAX_EQS; i++) {
            result.eq[i] = EqualizerConfig::defaultConfig();
        }
        return result;
    }

    const SpeakermanConfig SpeakermanConfig::unsetConfig()
    {
        SpeakermanConfig result;
        for (size_t i = 0; i < MAX_SPEAKERMAN_GROUPS; i++) {
            result.group[i] = GroupConfig::unsetConfig();
        }
        for (size_t i = 0; i <= MAX_CROSSOVERS; i++) {
            result.band[i] = BandConfig::unsetConfig();
        }
        for (size_t i = 0; i < MAX_EQS; i++) {
            result.eq[i] = EqualizerConfig::unsetConfig();
        }
        unset_config_value(result.groups);
        unset_config_value(result.groupChannels);
        unset_config_value(result.subOutput);
        unset_config_value(result.crossovers);
        unset_config_value(result.inputOffset);
        unset_config_value(result.relativeSubThreshold);
        unset_config_value(result.subDelay);
        unset_config_value(result.generateNoise);
        unset_config_value(result.eqs);
        result.timeStamp = -1;

        return result;
    }

    void SpeakermanConfig::set_if_unset(const SpeakermanConfig &config_if_unset)
    {
        size_t group_idx;
        if (set_if_unset_or_invalid_config_value(groups, config_if_unset.groups, MIN_GROUPS, MAX_GROUPS)) {
            for (group_idx = 0; group_idx < groups; group_idx++) {
                group[group_idx] = config_if_unset.group[group_idx];
            }
        }
        else {
            for (group_idx = 0; group_idx < groups; group_idx++) {
                group[group_idx].set_if_unset(config_if_unset.group[group_idx]);
            }
        }
        for (; group_idx < MAX_GROUPS; group_idx++) {
            group[group_idx] = GroupConfig::unsetConfig();
        }

        size_t band_id;
        if (set_if_unset_or_invalid_config_value(crossovers, config_if_unset.crossovers, MIN_CROSSOVERS, MAX_CROSSOVERS)) {
            for (band_id = 0; band_id <= crossovers; band_id++) {
                band[band_id] = config_if_unset.band[band_id];
            }
        }
        else {
            for (band_id = 0; band_id <= crossovers; band_id++) {
                band[band_id].set_if_unset(config_if_unset.band[band_id]);
            }
        }
        for (; band_id <= MAX_CROSSOVERS; band_id++) {
            band[band_id] = BandConfig::unsetConfig();
        }
        size_t eq_idx;
        if (set_if_unset_or_invalid_config_value(eqs, config_if_unset.eqs,
                                                 MIN_EQS, MAX_EQS)) {
            for (eq_idx = 0; eq_idx < eqs; eq_idx++) {
                eq[eq_idx] = config_if_unset.eq[eq_idx];
            }
        }
        else {
            for (eq_idx = 0; eq_idx < eqs; eq_idx++) {
                eq[eq_idx].set_if_unset(config_if_unset.eq[eq_idx]);
            }
        }
        for (; eq_idx < MAX_EQS; eq_idx++) {
            eq[eq_idx] = EqualizerConfig::unsetConfig();
        }

        set_if_unset_or_invalid_config_value(groupChannels, config_if_unset.groupChannels, MIN_GROUP_CHANNELS, MAX_GROUP_CHANNELS);
        set_if_unset_or_invalid_config_value(subOutput, config_if_unset.subOutput, MIN_SUB_OUTPUT, MAX_SUB_OUTPUT);
        set_if_unset_or_invalid_config_value(inputOffset, config_if_unset.inputOffset, MIN_INPUT_OFFSET, MAX_INPUT_OFFSET);
        set_if_unset_or_invalid_config_value(relativeSubThreshold, config_if_unset.relativeSubThreshold, MIN_REL_SUB_THRESHOLD, MAX_REL_SUB_THRESHOLD);
        set_if_unset_or_invalid_config_value(subDelay, config_if_unset.subDelay, MIN_SUB_DELAY, MAX_SUB_DELAY);
        set_if_unset_or_invalid_config_value(generateNoise, config_if_unset.generateNoise, 0, 1);
        set_if_unset_or_invalid_config_value(threshold_scaling, config_if_unset.threshold_scaling, MIN_THRESHOLD_SCALING, MAX_THRESHOLD_SCALING);
        timeStamp = -1;
    }

    StreamOwner::StreamOwner(std::ifstream &owned) :
            stream_(owned), owns_(true)
    {}

    StreamOwner::StreamOwner(const StreamOwner &source) :
            stream_(source.stream_), owns_(false)
    {}

    StreamOwner::StreamOwner(StreamOwner &&source) noexcept
            : stream_(source.stream_), owns_(true)
    {
        source.owns_ = false;
    }
    StreamOwner StreamOwner::open(const char *file_name)
    {
        std::ifstream stream;
        stream.open(file_name);
        return StreamOwner(stream);
    }
    StreamOwner::~StreamOwner()
    {
        if (owns_ && stream_.is_open()) {
            stream_.close();
        }
    }
    bool StreamOwner::is_open() const { return stream_.is_open(); }


} /* End of namespace speakerman */

