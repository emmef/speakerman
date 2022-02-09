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
static constexpr const char *INSTALLATION_PREFIX =
    TO_STR(SPEAKERMAN_INSTALL_PREFIX);
#endif

#include <cmath>
#include <cstring>
#include <iostream>
#include <mutex>
#include <speakerman/SpeakermanConfig.hpp>
#include <speakerman/JsonCanonicalReader.h>
#include <speakerman/StreamOwner.h>
#include <speakerman/UnsetValue.h>
#include <speakerman/utils/Config.hpp>
#include <string>
#include <sys/stat.h>
#include <tdap/FixedSizeArray.hpp>
#include <tdap/Value.hpp>
#include <unistd.h>

namespace speakerman {

using namespace std;
using namespace tdap;

namespace { // anonymous namespace for key snippets.
static constexpr const char *DETECTION_CONFIG_KEY_MAXIMUM_WINDOW_SECONDS =
    "detection.slow-seconds";
static constexpr const char *DETECTION_CONFIG_KEY_PERCEPTIVE_LEVELS =
    "detection.time-constants";
static constexpr const char *DETECTION_CONFIG_KEY_MINIMUM_WINDOW_SECONDS =
    "detection.fast-seconds";
static constexpr const char *DETECTION_CONFIG_KEY_RMS_FAST_RELEASE_SECONDS =
    "detection.rms-fast-release-seconds";
static constexpr const char *DETECTION_CONFIG_KEY_USE_BRICK_WALL_PREDICTION =
    "detection.use-brick-wall-prediction";
static constexpr const char *EQ_CONFIG_KEY_EQUALIZER = "equalizer";
static constexpr const char *EQ_CONFIG_KEY_CENTER = "center";
static constexpr const char *EQ_CONFIG_KEY_GAIN = "gain";
static constexpr const char *EQ_CONFIG_KEY_BANDWIDTH = "bandwidth";
static constexpr const char *LOGICAL_GROUP_CONFIG_KEY_INPUT = "logicalInput";
[[maybe_unused]] static constexpr const char *LOGICAL_GROUP_CONFIG_KEY_OUTPUT =
    "logicalOutput";
static constexpr const char *LOGICAL_GROUP_CONFIG_KEY_VOLUME = "volume";
static constexpr const char *LOGICAL_GROUP_CONFIG_KEY_NUMBER = "port-numbers";
static constexpr const char *NAMED_CONFIG_KEY_NAME = "name";
static constexpr const char *PROCESSING_GROUP_CONFIG_KEY_GROUP = "group";
static constexpr const char *PROCESSING_GROUP_CONFIG_KEY_MONO = "mono";
static constexpr const char *PROCESSING_GROUP_CONFIG_KEY_USE_SUB = "use-sub";
static constexpr const char *PROCESSING_GROUP_CONFIG_KEY_DELAY = "delay";
static constexpr const char *PROCESSING_GROUP_CONFIG_KEY_THRESHOLD =
    "threshold";
static constexpr const char *PROCESSING_GROUP_CONFIG_KEY_EQ_COUNT =
    "equalizers";
[[maybe_unused]] static constexpr const char
    *SPEAKER_MANAGER_CONFIG_KEY_EQ_COUNT = "equalizers";
static constexpr const char *SPEAKER_MANAGER_CONFIG_KEY_GROUP_COUNT = "groups";
static constexpr const char *SPEAKER_MANAGER_CONFIG_KEY_CHANNELS =
    "group-channels";
static constexpr const char *SPEAKER_MANAGER_CONFIG_KEY_SUB_THRESHOLD =
    "sub-relative-threshold";
static constexpr const char *SPEAKER_MANAGER_CONFIG_KEY_SUB_DELAY = "sub-delay";
static constexpr const char *SPEAKER_MANAGER_CONFIG_KEY_SUB_OUTPUT =
    "sub-output";
static constexpr const char *SPEAKER_MANAGER_CONFIG_KEY_CROSSOVERS =
    "crossovers";
[[maybe_unused]] static constexpr const char
    *SPEAKER_MANAGER_CONFIG_KEY_INPUT_OFFSET = "input-offset";
[[maybe_unused]] static constexpr const char
    *SPEAKER_MANAGER_CONFIG_KEY_INPUT_COUNT = "input-count";
static constexpr const char *SPEAKER_MANAGER_CONFIG_KEY_GENERATE_NOISE =
    "generate-noise";

} // anonymous namespace

class ReadConfigException : public runtime_error {
public:
  explicit ReadConfigException(const char *message) : runtime_error(message) {}

  static ReadConfigException noSeek() {
    return ReadConfigException("Could not reset file read position");
  }
};

template <typename T, int type> struct ValueParser_ {};

template <typename T> struct ValueParser_<T, 1> {
  static_assert(is_integral<T>::value, "Expected integral type parameter");
  using V = long long int;

  static bool parse(T &field, const char *value, char *&end) {
    V parsed = strtoll(value, &end, 10);
    if (*end == '\0' || utils::config::isWhiteSpace(*end) ||
        utils::config::isCommentStart(*end)) {
      field = tdap::Value<T>::force_between(parsed,
                                            std::numeric_limits<T>::lowest(),
                                            std::numeric_limits<T>::max());
      return true;
    }
    cerr << "Error parsing integer" << endl;
    return false;
  }
};

template <typename T> struct ValueParser_<T, 2> {
  static_assert(is_integral<T>::value, "Expected integral type parameter");
  using V = int;

  static bool matches(const char *keyword, const char *value, const char *end) {
    size_t scan_length = end - value;
    size_t key_length = strnlen(keyword, 128);
    if (scan_length < key_length) {
      return false;
    }

    return strncasecmp(keyword, value, key_length) == 0 &&
           (value[key_length] == '\0' ||
            utils::config::isWhiteSpace(value[key_length]));
  }

  static bool parse(T &field, const char *value, char *&end) {
    for (end = const_cast<char *>(value); utils::config::isAlphaNum(*end);
         end++) {
    }

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

template <typename T> struct ValueParser_<T, 3> {
  static_assert(is_floating_point<T>::value,
                "Expected floating point type parameter");
  using V = long double;

  static bool parse(T &field, const char *value, char *&end) {
    V parsed = strtold(value, &end);
    if (*end == '\0' || utils::config::isWhiteSpace(*end) ||
        utils::config::isCommentStart(*end)) {
      field = tdap::Value<V>::force_between(parsed,
                                            std::numeric_limits<T>::lowest(),
                                            std::numeric_limits<T>::max());
      return true;
    }
    cerr << "Error parsing float" << endl;
    return false;
  }
};

template <typename T> struct ValueParser_<T, 4> {
  static bool parse(T &field, const char *value, char *&end) {
    const char *src = value;
    char *dst = field;
    while (src != 0 && (dst - field) < ssize_t(NamedConfig::NAME_LENGTH)) {
      char c = *src++;
      if (c == '\t' || c == ' ') {
        if (dst > field) {
          *dst++ = ' ';
        }
      } else if (utils::config::isAlphaNum(c) || utils::config::isQuote(c) ||
                 strchr(".!|,;:/[]{}*#@~%^()-_+=\\", c) != nullptr) {
        *dst++ = c;
      }
    }
    *dst++ = '\0';
    end = (char *)src;
    return true;
  }
};

template <typename T> static constexpr int get_value_parser_type() {
  return std::is_floating_point<T>::value                           ? 3
         : std::is_same<int, T>::value                              ? 2
         : std::is_integral<T>::value                               ? 1
         : std::is_same<char[NamedConfig::NAME_CAPACITY], T>::value ? 4
                                                                    : 5;
}

template <typename T>
struct ValueParser : public ValueParser_<T, get_value_parser_type<T>()> {};

template <typename T, size_t N>
static int read_value_array(FixedSizeArray<T, N> &values, const char *value,
                            char *&end) {
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
    while (utils::config::isWhiteSpace(*end) || *end == ';' || *end == ',') {
      if (*end == ';' || *end == ',') {
        if (hadDelimiter) {
          return -1;
        }
        hadDelimiter = true;
      }
      end++;
    }
    if (*end == '\0' || utils::config::isCommentStart(*end)) {
      break;
    }
    read_from = end;
  }
  return i + 1;
};

using CountValueParser = ValueParser<size_t>;
using BooleanValueParser = ValueParser<bool>;
using FloatValueParser = ValueParser<double>;

class VariableReader {
  bool runtime_changeable_ = false;
  bool is_deprecated_ = false;

protected:
  virtual void read(SpeakermanConfig &config, const char *key,
                    const char *value) const = 0;

public:
  [[nodiscard]] bool runtime_changeable() const { return runtime_changeable_; }
  [[nodiscard]] bool is_deprecated() const { return is_deprecated_; }

  virtual void write(const SpeakermanConfig &config, const char *key,
                     ostream &stream) const = 0;

  VariableReader(bool runtime_changeable, bool is_deprecated)
      : runtime_changeable_(runtime_changeable), is_deprecated_(is_deprecated) {
  }

  bool read(SpeakermanConfig &config, const char *key, const char *value,
            bool isRuntime) const {
    if (!isRuntime || runtime_changeable_) {
      read(config, key, value);
      return true;
    }
    cout << "Ignoring (not runtime changeable): " << key << endl;
    return false;
  }

  virtual ~VariableReader() = default;
};

template <size_t SIZE> class PositionedVariableReader : public VariableReader {
  size_t offset_;

  static size_t valid_offset(const SpeakermanConfig &config,
                             const void *field) {
    const char *config_address =
        static_cast<const char *>(static_cast<const void *>(&config));
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
  size_t offset() const { return offset_; }

  void *variable(SpeakermanConfig &config) const {
    char *config_address = static_cast<char *>(static_cast<void *>(&config));
    return static_cast<void *>(config_address + offset_);
  }

  const void *variable(const SpeakermanConfig &config) const {
    const char *config_address =
        static_cast<const char *>(static_cast<const void *>(&config));
    return static_cast<const void *>(config_address + offset_);
  }

  void copy(SpeakermanConfig &config, const SpeakermanConfig &basedUpon) const {
    memcpy(variable(config), variable(basedUpon), SIZE);
  }

public:
  PositionedVariableReader(bool runtime_changeable,
                           const SpeakermanConfig &config, const void *field,
                           bool is_deprecated)
      : VariableReader(runtime_changeable, is_deprecated),
        offset_(valid_offset(config, field)) {}

  //  PositionedVariableReader(bool runtime_changeable,
  //                           const SpeakermanConfig &config, const void
  //                           *field)
  //      : PositionedVariableReader(runtime_changeable, false, config, field)
  //      {}
};

template <typename T>
class TypedVariableReader : public PositionedVariableReader<sizeof(T)> {
  using PositionedVariableReader<sizeof(T)>::variable;

protected:
  void read(SpeakermanConfig &config, const char *key,
            const char *value) const override {

    T &field = *static_cast<T *>(variable(config));
    char *end = const_cast<char *>(value);
    if (!ValueParser<T>::parse(field, value, end)) {
      cerr << "Error parsing \"" << key << "\" @" << (end - value) << ": "
           << value << endl;
    }
  }

  template <typename X>
  void write(const X &variable, const char *key, ostream &stream) const {
    if (!isUnsetConfigValue(variable)) {
      stream << key << " = " << variable << endl;
    }
  }

public:
  TypedVariableReader(bool runtime_changeable, const SpeakermanConfig &config,
                      const T &field, bool is_deprecated)
      : PositionedVariableReader<sizeof(T)>(runtime_changeable, config, &field,
                                            is_deprecated){};

  void write(const SpeakermanConfig &config, const char *key,
             ostream &stream) const override {
    if constexpr (std::is_same_v<const char[NamedConfig::NAME_CAPACITY], T>) {
      write(static_cast<const char *>(variable(config)), key, stream);
    } else {
      write(*static_cast<const T *>(variable(config)), key, stream);
    }
  }
};

template <typename T, size_t N>
class TypedArrayVariableReader
    : public PositionedVariableReader<N * sizeof(T)> {
  using PositionedVariableReader<N * sizeof(T)>::variable;

protected:
  void read(SpeakermanConfig &config, const char *key,
            const char *value) const override {
    FixedSizeArray<T, N> fieldValues;
    T *field = static_cast<T *>(variable(config));
    char *end = const_cast<char *>(value);
    int count = read_value_array(fieldValues, value, end);
    if (count < 0) {
      cerr << "Error parsing \"" << key << "\" @" << (end - value) << ": "
           << value << endl;
      return;
    }
    size_t positiveCount = count;

    size_t i;
    for (i = 0; i < positiveCount; i++) {
      field[i] = fieldValues[i];
    }
    for (; i < N; i++) {
      field[i] = UnsetValue<T>::value;
    }
  }

public:
  TypedArrayVariableReader(bool runtime_changeable,
                           const SpeakermanConfig &config, const T &field,
                           bool is_deprecated)
      : PositionedVariableReader<N * sizeof(T)>(runtime_changeable, config,
                                                &field, is_deprecated){};

  void write(const SpeakermanConfig &config, const char *key,
             ostream &stream) const override {
    const T *field = static_cast<const T *>(variable(config));
    bool print = false;

    for (size_t i = 0; i < N; i++) {
      if (UnsetValue<T>::is(field[i])) {
        break;
      } else {
        print = true;
        break;
      }
    }
    if (print) {
      stream << key << " =";
      for (size_t i = 0; i < N; i++) {
        if (UnsetValue<T>::is(field[i])) {
          break;
        } else {
          stream << " " << field[i];
        }
      }
      stream << endl;
    }
  }
};

enum class ReaderStatus { SKIP, SUCCESS, FAULT };

class KeyVariableReader {
  const string key_;
  const VariableReader *reader_;

public:
  KeyVariableReader(const string &key, VariableReader *reader)
      : key_(key), reader_(reader) {}

  ~KeyVariableReader() { delete reader_; }

  const string &get_key() const { return key_; }

  static ReaderStatus skip_to_key_start(const char *&result, const char *line) {
    if (line == nullptr || *line == '\0') {
      return ReaderStatus::SKIP;
    }
    const char *start;
    for (start = line; *start != '\0' && start - line <= 1024; start++) {
      char c = *start;
      if (utils::config::isCommentStart(c)) {
        return ReaderStatus::SKIP;
      }
      if (utils::config::isAlpha(c)) {
        result = start;
        return ReaderStatus::SUCCESS;
      }
      if (!utils::config::isWhiteSpace(c)) {
        cerr << "Unexpected characters @ " << (start - line) << ": " << line
             << endl;
        break;
      }
    }
    return ReaderStatus::FAULT;
  }

  ReaderStatus is(const char *start) const {
    if (strncasecmp(key_.c_str(), start, key_.length()) != 0) {
      return ReaderStatus::SKIP;
    }
    return ReaderStatus::SUCCESS;
  }

  ReaderStatus read_key(const char *&after_key, const char *start) const {
    if (strncasecmp(key_.c_str(), start, key_.length()) != 0) {
      return ReaderStatus::SKIP;
    }
    char end = start[key_.length()];
    if (utils::config::isWhiteSpace(end) || end == '=') {
      after_key = start + key_.length();
      return ReaderStatus::SUCCESS;
    }
    return ReaderStatus::SKIP;
  }

  static ReaderStatus skip_assignment(const char *&value, const char *start,
                                      const char *line) {
    const char *rd = start;
    while (utils::config::isWhiteSpace(*rd)) {
      rd++;
    }
    if (*rd != '=') {
      cerr << "Unexpected character @ " << (rd - line) << ": " << line << endl;
      cout << "1line :" << line << endl;
      cout << "1start:" << start << endl;
      return ReaderStatus::FAULT;
    }
    rd++;
    while (utils::config::isWhiteSpace(*rd)) {
      rd++;
    }
    if (*rd == '\0') {
      cerr << "Unexpected end of line @ " << (rd - line) << ": " << line
           << endl;
      cout << "2line :" << line << endl;
      cout << "2start:" << start << endl;
      return ReaderStatus::FAULT;
    }
    value = rd;
    return ReaderStatus::SUCCESS;
  }

  bool read(SpeakermanConfig &manager, const string &key, const char *value,
            bool runtime) const {
    if (reader_->is_deprecated()) {
      std::cout << "Warning: variable \"" << key_ << "\" is deprecated!"
                << std::endl;
    }
    return reader_->read(manager, key.c_str(), value, runtime);
  }

  bool read(SpeakermanConfig &manager, const char *key, const char *value,
            bool runtime) const {
    if (reader_->is_deprecated()) {
      std::cout << "Warning: variable \"" << key_ << "\" is deprecated!"
                << std::endl;
    }
    return reader_->read(manager, key, value, runtime);
  }

  void write(const SpeakermanConfig &config, ostream &output) const {
    reader_->write(config, key_.c_str(), output);
  }
};

class ConfigManager : protected SpeakermanConfig, protected JsonCanonicalReader {
  KeyVariableReader **readers_ = nullptr;
  size_t capacity_ = 0;
  size_t size_ = 0;
  static thread_local SpeakermanConfig *threadLocalConfig;

  void ensure_capacity(size_t new_capacity) {
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
    size_t actual_capacity =
        max(new_capacity, 3 * max(capacity_, (size_t)7) / 2);
    size_t moves = min(actual_capacity, size_);
    KeyVariableReader **new_readers = new KeyVariableReader *[actual_capacity];
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
  void add(KeyVariableReader *new_reader) {
    if (new_reader == nullptr) {
      throw std::invalid_argument("Cannot add NULL reader");
    }
    size_t new_size = size_ + 1;
    ensure_capacity(new_size);
    readers_[size_] = new_reader;
    size_ = new_size;
  }

  template <typename T>
  void add_reader(const string &name, bool runtime_changeable, T &field) {
    add(new KeyVariableReader(
        name,
        new TypedVariableReader<T>(runtime_changeable, *this, field, false)));
  }

  template <typename T, size_t N>
  void add_array_reader(const string &name, bool runtime_changeable, T &field) {
    add(new KeyVariableReader(
        name, new TypedArrayVariableReader<T, N>(runtime_changeable, *this,
                                                 field, false)));
  }

  template <typename T>
  void add_deprecated_reader(const string &name, bool runtime_changeable,
                             T &field) {
    add(new KeyVariableReader(
        name,
        new TypedVariableReader<T>(runtime_changeable, *this, field, true)));
  }

  template <typename T, size_t N>
  void add_deprecated_array_reader(const string &name, bool runtime_changeable,
                                   T &field) {
    add(new KeyVariableReader(
        name, new TypedArrayVariableReader<T, N>(runtime_changeable, *this,
                                                 field, true)));
  }

public:
  void addLogicalGroups(AbstractLogicalGroupsConfig &config,
                        const char *snippet) {
    string logicalInput = snippet;
    logicalInput += "/";
    for (size_t i = 0; i < AbstractLogicalGroupsConfig::MAX_GROUPS; i++) {
      string grp = logicalInput;
      if (i >= 10) {
        grp += char('0' + i / 10);
      }
      grp += char('0' + i % 10);
      grp += "/";
      string item;
      item = grp;
      item += NAMED_CONFIG_KEY_NAME;
      add_reader(item, true, config.group[i].name);
      item = grp;
      item += LOGICAL_GROUP_CONFIG_KEY_VOLUME;
      add_reader(item, true, config.group[i].volume);
      item = grp;
      item += LOGICAL_GROUP_CONFIG_KEY_NUMBER;
      add_array_reader<size_t, LogicalGroupConfig::MAX_CHANNELS>(
          item, false, config.group[i].ports[0]);
    }
  }

  template <LogicalGroupConfig::Direction D>
  void addLogicalGroups(LogicalGroupsConfig<D> &logicalGroupsConfig) {
    const char *snippet = D == LogicalGroupConfig::Direction::Input
                              ? LOGICAL_GROUP_CONFIG_KEY_INPUT
                              : LOGICAL_GROUP_CONFIG_KEY_OUTPUT;
    addLogicalGroups(logicalGroupsConfig, snippet);
  }

  ConfigManager() : JsonCanonicalReader(128, 128, 10) {
    add_reader(SPEAKER_MANAGER_CONFIG_KEY_GROUP_COUNT, false,
               processingGroups.groups);
    add_reader(SPEAKER_MANAGER_CONFIG_KEY_CHANNELS, false,
               processingGroups.channels);
    add_reader(SPEAKER_MANAGER_CONFIG_KEY_CROSSOVERS, false, crossovers);

    add_reader(SPEAKER_MANAGER_CONFIG_KEY_SUB_THRESHOLD, true,
               relativeSubThreshold);
    add_reader(SPEAKER_MANAGER_CONFIG_KEY_SUB_DELAY, true, subDelay);
    add_reader(SPEAKER_MANAGER_CONFIG_KEY_SUB_OUTPUT, false, subOutput);

    add_reader(SPEAKER_MANAGER_CONFIG_KEY_GENERATE_NOISE, true, generateNoise);

    add_reader(DETECTION_CONFIG_KEY_MAXIMUM_WINDOW_SECONDS, false,
               detection.maximum_window_seconds);
    add_reader(DETECTION_CONFIG_KEY_MINIMUM_WINDOW_SECONDS, false,
               detection.minimum_window_seconds);
    add_reader(DETECTION_CONFIG_KEY_RMS_FAST_RELEASE_SECONDS, false,
               detection.rms_fast_release_seconds);
    add_reader(DETECTION_CONFIG_KEY_PERCEPTIVE_LEVELS, false,
               detection.perceptive_levels);
    add_reader(DETECTION_CONFIG_KEY_USE_BRICK_WALL_PREDICTION, false,
               detection.useBrickWallPrediction);

    addLogicalGroups(logicalInputs, LOGICAL_GROUP_CONFIG_KEY_INPUT);
    // Disabled outputs for now, as we are not going to use them
    // addLogicalGroups(logicalOutputs,
    // LogicalGroupConfig::KEY_SNIPPET_OUTPUT);

    string key;

    add_reader(PROCESSING_GROUP_CONFIG_KEY_EQ_COUNT, true, eqs);
    string eqBase = "";
    eqBase += EQ_CONFIG_KEY_EQUALIZER;
    eqBase += "/";
    for (size_t eq_idx = 0; eq_idx < ProcessingGroupConfig::MAX_EQS; eq_idx++) {
      string eqKey = eqBase;
      eqKey += (char)('0' + eq_idx);
      eqKey += "/";

      key = eqKey;
      key += EQ_CONFIG_KEY_CENTER;
      add_reader(key, true, eq[eq_idx].center);
      key = eqKey;
      key += EQ_CONFIG_KEY_GAIN;
      add_reader(key, true, eq[eq_idx].gain);
      key = eqKey;
      key += EQ_CONFIG_KEY_BANDWIDTH;
      add_reader(key, true, eq[eq_idx].bandwidth);
    }

    for (size_t group_idx = 0; group_idx < ProcessingGroupsConfig::MAX_GROUPS;
         group_idx++) {
      string groupKey = PROCESSING_GROUP_CONFIG_KEY_GROUP;
      groupKey += "/";
      groupKey += (char)(group_idx + '0');
      groupKey += "/";

      key = groupKey;
      key += PROCESSING_GROUP_CONFIG_KEY_EQ_COUNT;
      add_reader(key, true, processingGroups.group[group_idx].eqs);
      key = groupKey;
      key += PROCESSING_GROUP_CONFIG_KEY_THRESHOLD;
      add_reader(key, true, processingGroups.group[group_idx].threshold);
      key = groupKey;
      key += PROCESSING_GROUP_CONFIG_KEY_DELAY;
      add_reader(key, true, processingGroups.group[group_idx].delay);
      key = groupKey;
      key += PROCESSING_GROUP_CONFIG_KEY_USE_SUB;
      add_reader(key, true, processingGroups.group[group_idx].useSub);
      key = groupKey;
      key += PROCESSING_GROUP_CONFIG_KEY_MONO;
      add_reader(key, true, processingGroups.group[group_idx].mono);
      key = groupKey;
      key += NAMED_CONFIG_KEY_NAME;
      add_reader(key, true, processingGroups.group[group_idx].name);

      string eqBase = groupKey;
      eqBase += EQ_CONFIG_KEY_EQUALIZER;
      eqBase += "/";
      for (size_t eq_idx = 0; eq_idx < ProcessingGroupConfig::MAX_EQS;
           eq_idx++) {
        string eqKey = eqBase;
        eqKey += (char)('0' + eq_idx);
        eqKey += "/";

        key = eqKey;
        key += EQ_CONFIG_KEY_CENTER;
        add_reader(key, true,
                   processingGroups.group[group_idx].eq[eq_idx].center);
        key = eqKey;
        key += EQ_CONFIG_KEY_GAIN;
        add_reader(key, true,
                   processingGroups.group[group_idx].eq[eq_idx].gain);
        key = eqKey;
        key += EQ_CONFIG_KEY_BANDWIDTH;
        add_reader(key, true,
                   processingGroups.group[group_idx].eq[eq_idx].bandwidth);
      }
    }

    string matrixSnippet = PROCESSING_GROUP_CONFIG_KEY_GROUP;
    matrixSnippet += "s/in/";
    for (size_t pc = 0; pc < ProcessingGroupConfig::MAX_CHANNELS; pc++) {
      string pcChannelSnippet = matrixSnippet;
      if (pc > 9) {
        pcChannelSnippet += char('0' + (pc / 10));
      }
      pcChannelSnippet += char('0' + (pc % 10));
      pcChannelSnippet += "/logical-channel-weights";
      add_array_reader<double, LogicalGroupConfig::MAX_CHANNELS>(
          pcChannelSnippet, true, *inputMatrix.weightsFor(pc));
    }
  }

  size_t size() const { return size_; }

  const KeyVariableReader *find(const char *keyName) const {
    for (size_t i = 0; i < size_; i++) {
      const KeyVariableReader *reader = readers_[i];
      if (reader->is(keyName) == ReaderStatus::SUCCESS) {
        return reader;
      }
    }
    return nullptr;
  }

  bool read_line(SpeakermanConfig &config, const char *line, bool runtime) const {
    const char *key_start;

    ReaderStatus status = KeyVariableReader::skip_to_key_start(key_start, line);
    if (status != ReaderStatus::SUCCESS) {
      return status != ReaderStatus::FAULT;
    }
    KeyVariableReader *reader = nullptr;
    const char *after_key = nullptr;

    for (size_t i = 0; i < size_; i++) {
      KeyVariableReader *r = readers_[i];
      status = r->read_key(after_key, key_start);
      if (status == ReaderStatus::SUCCESS) {
        reader = r;
      } else if (status != ReaderStatus::SKIP) {
        return false;
      }
    }

    if (reader == nullptr) {
      return true;
    }

    const char *value_start = nullptr;
    status = KeyVariableReader::skip_assignment(value_start, after_key, line);
    if (status != ReaderStatus::SUCCESS) {
      return false;
    }
    return reader->read(config, reader->get_key(), value_start, runtime);
  }

  void dump(const SpeakermanConfig &config, ostream &stream) {
    for (size_t i = 0; i < size(); i++) {
      readers_[i]->write(config, stream);
    }
  }

  bool readJson(SpeakermanConfig &config, org::simple::util::text::InputStream<char> &input, std::string &message) {
    std::vector<std::string> stack;
    std::string workSpace;

    struct Guard {
      Guard(SpeakermanConfig &c) {
        threadLocalConfig = &c;
      }
      ~Guard() {
        threadLocalConfig = nullptr;
      }
    } guard(config);
    org::simple::util::text::TextFilePositionData<char> position;
    try {
      JsonCanonicalReader::readJson(input, position);
      return true;
    } catch (const std::exception &e) {
      message = "At line ";
      message += std::to_string(position.getLine() + 1);
      message += ", col ";
      message += std::to_string(position.getColumn() + 1);
      message += ": ";
      message += e.what();
      std::cerr << message << std::endl;
    }
    return false;
  }

  void setString(const char *path, const char *string) final {
    const KeyVariableReader *reader = find(path);
    if (reader && threadLocalConfig) {
      std::cout << "JSON: String " << path << " = " << string << std::endl;
      reader->read(*threadLocalConfig, path, string, true);
    }
    else {
      std::cout << "JSON: NOT found: String " << path << " = " << string << std::endl;
    }
  }

  void setNumber(const char *path, const char *string) final {
    const KeyVariableReader *reader = find(path);
    if (reader && threadLocalConfig) {
      std::cout << "JSON: Number " << path << " = " << string << std::endl;
      reader->read(*threadLocalConfig, path, string, true);
    }
    else {
      std::cout << "JSON: NOT found: Number " << path << " = " << string << std::endl;
    }
  }

  void setBoolean(const char *path, bool value) final {
    const KeyVariableReader *reader = find(path);
    if (value && strncmp(path, "reload", 30) == 0) {
      std::cout << "JSON: Reset config!" << std::endl;
      if (threadLocalConfig) {
        threadLocalConfig->timeStamp = 0;
      }
      return;
    }
    if (reader && threadLocalConfig) {
      std::cout << "JSON: Boolean " << path << " = " << (value ? "true" : "false") << std::endl;
      reader->read(*threadLocalConfig, path, value ? "1" : "0", true);
    }
    else {
      std::cout << "JSON: NOT found: Boolean " << path << " = " << (value ? "true" : "false") << std::endl;
    }
  }

  void setNull(const char *path) final {
    const KeyVariableReader *reader = find(path);
    if (reader && threadLocalConfig) {
      std::cout << "JSON: NULL " << path << std::endl;
      reader->read(*threadLocalConfig, path, "", true);
    }
    else {
      std::cout << "JSON: NOT found " << path << std::endl;
    }
  }

};

thread_local SpeakermanConfig *ConfigManager::threadLocalConfig;

static ConfigManager config_manager;

class InstallBase {
  static const string internalGetInstallBase() {
    static string error_message;

    const char *used_prefix_type;
    const char *prefix_value = nullptr;

    prefix_value = getenv("SPEAKERMAN_INSTALLATION_PREFIX");
    if (prefix_value && strnlen(prefix_value, 2) > 0) {
      used_prefix_type = "Environment";
    } else if (INSTALLATION_PREFIX && strnlen(INSTALLATION_PREFIX, 2) > 0) {
      used_prefix_type = "Compile-time";
      prefix_value = INSTALLATION_PREFIX;
    }

    if (!prefix_value) {
      throw std::runtime_error(
          "Installation prefix not set. This should be done at compile "
          "time or "
          "using the SPEAKERMAN_INSTALLATION_PREFIX environment variable");
    }

    if (access(INSTALLATION_PREFIX, F_OK) == 0) {
      string prefixDir = prefix_value;
      if (prefixDir.at(prefixDir.length() - 1) != '/') {
        prefixDir += '/';
      }
      std::cout << "Using installation base (" << used_prefix_type << ") \""
                << prefixDir << "\"" << std::endl;
      return prefixDir;
    }
    error_message = "Installation base (";
    error_message += used_prefix_type;
    error_message += ") does not point to an existing/accessible directory: ";
    error_message += prefix_value;

    throw std::runtime_error(error_message);
  }

public:
  static const char *getBase() {
    static const string base = internalGetInstallBase();

    return base.c_str();
  }
};

const char *getInstallBaseDirectory() { return InstallBase::getBase(); }

static string internalGetWebSiteDirectory() {
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

const char *getWebSiteDirectory() {
  static string dir = internalGetWebSiteDirectory();

  return dir.length() > 0 ? dir.c_str() : nullptr;
}

static string getConfigFileName() {
  string configFileName = std::getenv("HOME");
  configFileName += "/.config/speakerman/speakerman.conf";

  return configFileName;
}

const char *configFileName() {
  static string name = getConfigFileName();

  return name.c_str();
}

static string internalGetWatchDogScript() {
  string watchDog = getInstallBaseDirectory();
  watchDog += "share/speakerman/script/speakerman-watchdog.sh";

  if (access(watchDog.c_str(), F_OK) == 0) {
    cout << "Watch-dog script: " << watchDog << endl;
    return watchDog;
  }

  return "";
}

const char *getWatchDogScript() {
  static string script = internalGetWatchDogScript();

  return script.length() > 0 ? script.c_str() : nullptr;
}

static void resetStream(istream &stream) {
  stream.clear(istream::eofbit);
  stream.seekg(0, stream.beg);
  if (stream.fail()) {
    throw ReadConfigException::noSeek();
  }
}

static SpeakermanConfig actualReadConfig(istream &stream, bool runtime) {
  static constexpr size_t LINE_LENGTH = 1024;
  char line[LINE_LENGTH + 1];
  size_t line_pos = 0;
  SpeakermanConfig config = SpeakermanConfig::unsetConfig();

  resetStream(stream);
  while (true) {
    char c = static_cast<char>(stream.get());
    if (stream.eof()) {
      break;
    }
    if (utils::config::isLineDelimiter(c)) {
      if (line_pos != 0) {
        line[std::min(line_pos, LINE_LENGTH)] = '\0';
        config_manager.read_line(config, line, runtime);
        line_pos = 0;
      }
    } else if (line_pos < LINE_LENGTH) {
      line[line_pos++] = c;
      continue;
    } else if (line_pos == LINE_LENGTH) {
      line[LINE_LENGTH] = '\0';
      config_manager.read_line(config, line, runtime);
      line_pos++;
    }
  }
  if (line_pos != 0) {
    line[std::min(line_pos, LINE_LENGTH)] = '\0';
    config_manager.read_line(config, line, runtime);
  }
  return config;
}

long long getFileTimeStamp(const char *fileName) {
  struct stat stats;
  if (stat(fileName, &stats) == 0) {
    return stats.st_mtim.tv_sec;
  }
  return -1;
}

long long getConfigFileTimeStamp() {
  return getFileTimeStamp(configFileName());
}

SpeakermanConfig readSpeakermanConfig(const SpeakermanConfig &basedUpon,
                                      bool isRuntime) {
  ifstream stream;
  long long stamp = getFileTimeStamp(configFileName());
  stream.open(configFileName());

  StreamOwner owner(stream);
  if (!stream.is_open()) {
    cerr << "Stream not open..." << endl;
    return basedUpon;
  }
  SpeakermanConfig result;
  try {
    if (isRuntime) {
      result = basedUpon;
      result.updateRuntimeValues(actualReadConfig(stream, isRuntime));
    } else {
      result = actualReadConfig(stream, isRuntime);
      result.setInitial();
    }
  } catch (const ReadConfigException &e) {
    cerr << "E: " << e.what() << endl;
    return basedUpon;
  }
  result.timeStamp = stamp;
  return result;
}

SpeakermanConfig readSpeakermanConfig() {
  SpeakermanConfig basedUpon = SpeakermanConfig::defaultConfig();
  return readSpeakermanConfig(basedUpon, false);
}

void dumpSpeakermanConfig(const SpeakermanConfig &configuration,
                          std::ostream &output, const char *comment) {
  if (comment) {
    output << "# " << comment;
  }
  else {
    output << "# Speakerman configuration dump";
  }
  output << endl << endl;
  config_manager.dump(configuration, output);
}

bool readConfigFromJson(SpeakermanConfig &destination, const char *json,
                        const SpeakermanConfig &basedUpon) {
  class Input : public org::simple::util::text::InputStream<char> {
    const char *string;
    const char *at;
  public:
    Input(const char *source) : string(source), at(string) {}

    bool get(char &c) final {
      if (*at && (at - string) <= 1048576l) {
        c = *at++;
        return true;
      }
      return false;
    }
  };
  Input stream(json);
  std::string errorMessage;

  if (config_manager.readJson(destination, stream, errorMessage)) {
    return true;
  }
  else {
    std::cerr << "Failed to read JSON: " << errorMessage << std::endl;
    std::cerr << json << std::endl;
  }

  return false;
}

const SpeakermanConfig SpeakermanConfig::defaultConfig() {
  SpeakermanConfig result;
  result.logicalInputs = LogicalInputsConfig::defaultConfig();
  result.logicalOutputs = LogicalOutputsConfig ::defaultConfig();
  result.detection = DetectionConfig::defaultConfig();
  for (size_t i = 0; i < MAX_EQS; i++) {
    result.eq[i] = EqualizerConfig::defaultConfig();
  }
  return result;
}

const SpeakermanConfig SpeakermanConfig::unsetConfig() {
  SpeakermanConfig result;
  result.logicalInputs = LogicalInputsConfig::unsetConfig();
  result.logicalOutputs = LogicalOutputsConfig ::unsetConfig();

  for (size_t i = 0; i < ProcessingGroupsConfig::MAX_GROUPS; i++) {
    result.processingGroups.group[i] = ProcessingGroupConfig::unsetConfig();
  }
  result.detection.unsetConfig();
  for (size_t i = 0; i < MAX_EQS; i++) {
    result.eq[i] = EqualizerConfig::unsetConfig();
  }
  unsetConfigValue(result.processingGroups.groups);
  unsetConfigValue(result.processingGroups.channels);
  unsetConfigValue(result.subOutput);
  unsetConfigValue(result.crossovers);
  unsetConfigValue(result.relativeSubThreshold);
  unsetConfigValue(result.subDelay);
  unsetConfigValue(result.generateNoise);
  unsetConfigValue(result.eqs);
  result.timeStamp = -1;

  return result;
}

void SpeakermanConfig::updateRuntimeValues(
    const SpeakermanConfig &newRuntimeConfig) {
  logicalInputs.changeRuntimeValues(newRuntimeConfig.logicalInputs);
  logicalOutputs.changeRuntimeValues(newRuntimeConfig.logicalOutputs);
  processingGroups.changeRuntimeValues(newRuntimeConfig.processingGroups);
  inputMatrix.changeRuntimeValues(newRuntimeConfig.inputMatrix,
                                  processingGroups.groups *
                                      processingGroups.channels,
                                  logicalInputs.getTotalChannels());

  detection.set_if_unset(newRuntimeConfig.detection);
  copyEqualizers(newRuntimeConfig);

  setBoxedFromSetSource(relativeSubThreshold,
                        newRuntimeConfig.relativeSubThreshold,
                        MIN_REL_SUB_THRESHOLD, MAX_REL_SUB_THRESHOLD);
  setBoxedFromSetSource(subDelay, newRuntimeConfig.subDelay, MIN_SUB_DELAY,
                        MAX_SUB_DELAY);
  setBoxedFromSetSource(generateNoise, newRuntimeConfig.generateNoise, 0, 1);
  setBoxedFromSetSource(threshold_scaling, newRuntimeConfig.threshold_scaling,
                        MIN_THRESHOLD_SCALING, MAX_THRESHOLD_SCALING);
  timeStamp = -1;
}

void SpeakermanConfig::setInitial() {
  logicalInputs.sanitizeInitial();

  logicalOutputs.sanitizeInitial();

  processingGroups.sanitizeInitial(logicalInputs.getTotalChannels());

  inputMatrix.replaceWithDefaultsIfUnset(processingGroups.groups *
                                             processingGroups.channels,
                                         logicalInputs.getTotalChannels());

  detection.set_if_unset(DetectionConfig::defaultConfig());
  copyEqualizers(*this);

  setDefaultOrBoxedFromSourceIfUnset(subOutput, DEFAULT_SUB_OUTPUT, subOutput,
                                     MIN_SUB_OUTPUT, MAX_SUB_OUTPUT);

  setDefaultOrBoxedFromSourceIfUnset(
      relativeSubThreshold, DEFAULT_REL_SUB_THRESHOLD, relativeSubThreshold,
      MIN_REL_SUB_THRESHOLD, MAX_REL_SUB_THRESHOLD);
  setDefaultOrBoxedFromSourceIfUnset(subDelay, DEFAULT_SUB_DELAY, subDelay,
                                     MIN_SUB_DELAY, MAX_SUB_DELAY);
  setDefaultOrBoxedFromSourceIfUnset(generateNoise, DEFAULT_GENERATE_NOISE,
                                     generateNoise, 0, 1);
  setDefaultOrBoxedFromSourceIfUnset(
      threshold_scaling, DEFAULT_THRESHOLD_SCALING, threshold_scaling,
      MIN_THRESHOLD_SCALING, MAX_THRESHOLD_SCALING);
  timeStamp = -1;
}

void SpeakermanConfig::copyEqualizers(const SpeakermanConfig &sourceConfig) {
  size_t eq_idx;
  if (fixedValueIfUnsetOrOutOfRange(eqs, sourceConfig.eqs, MIN_EQS, MAX_EQS)) {
    for (eq_idx = 0; eq_idx < eqs; eq_idx++) {
      eq[eq_idx] = sourceConfig.eq[eq_idx];
      eq[eq_idx].set_if_unset(EqualizerConfig::defaultConfig());
    }
  } else {
    for (eq_idx = 0; eq_idx < eqs; eq_idx++) {
      eq[eq_idx].set_if_unset(sourceConfig.eq[eq_idx]);
      eq[eq_idx].set_if_unset(EqualizerConfig::defaultConfig());
    }
  }
  for (; eq_idx < MAX_EQS; eq_idx++) {
    eq[eq_idx] = EqualizerConfig::defaultConfig();
  }
}

} // namespace speakerman
