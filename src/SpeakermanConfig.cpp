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
#include <mutex>
#include <speakerman/SpeakermanConfig.hpp>
#include <speakerman/utils/Config.hpp>
#include <string>
#include <sys/stat.h>
#include <tdap/FixedSizeArray.hpp>
#include <tdap/Value.hpp>
#include <unistd.h>

namespace speakerman {

using namespace std;
using namespace tdap;

class ReadConfigException : public runtime_error {
public:
  explicit ReadConfigException(const char *message) : runtime_error(message) {}

  static ReadConfigException noSeek() {
    return ReadConfigException("Could not reset file read position");
  }
};


template <typename T, size_t N>
static ValueSetResult read_value_array(ConfigNumericArray<T, N> &values,
                                       const char *value, char *&end) {
  ConfigNumericArray<T, N> readValues(values, false);
  const char *read_from = value;
  for (size_t i = 0; i < N; i++) {
    T parsed;
    if (!ValueParser<T>::parse(parsed, read_from, end)) {
      return ValueSetResult::Fail;
    }
    if (readValues.set(parsed) == ValueSetResult::Fail) {
      return ValueSetResult::Fail;
    };
    bool hadDelimiter = false;
    while (config::isWhiteSpace(*end) || *end == ';' || *end == ',') {
      if (*end == ';' || *end == ',') {
        if (hadDelimiter) {
          return ValueSetResult::Fail;
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
  values = readValues;
  return ValueSetResult::Ok;
};

template <typename T>
static ValueSetResult read_value(ConfigNumeric<T> &values, const char *value,
                                 char *&end) {
  T parsed;
  if (!ValueParser<T>::parse(parsed, value, end)) {
    return ValueSetResult::Fail;
  }
  return values.set(values);
};

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

class VariableReader {
  bool runtime_changeable_ = false;
  bool is_deprecated_ = false;

protected:
  virtual void read(SpeakermanConfig &config, const char *key,
                    const char *value) const = 0;

  virtual void copy(SpeakermanConfig &config,
                    const SpeakermanConfig &basedUpon) const = 0;

public:
  [[nodiscard]] bool runtime_changeable() const { return runtime_changeable_; }
  [[nodiscard]] bool is_deprecated() const { return is_deprecated_; }

  virtual void write(const SpeakermanConfig &config, const char *key,
                     ostream &stream) const = 0;

  VariableReader(bool runtime_changeable, bool is_deprecated)
      : runtime_changeable_(runtime_changeable), is_deprecated_(is_deprecated) {
  }

  bool read(SpeakermanConfig &config, const char *key, const char *value,
            const SpeakermanConfig &basedUpon) const {
    if (&config == &basedUpon || runtime_changeable_) {
      read(config, key, value);
      return true;
    } else {
      copy(config, basedUpon);
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

public:
  TypedVariableReader(bool runtime_changeable, const SpeakermanConfig &config,
                      const T &field, bool is_deprecated)
      : PositionedVariableReader<sizeof(T)>(runtime_changeable, config, &field,
                                            is_deprecated){};

  void write(const SpeakermanConfig &config, const char *key,
             ostream &stream) const override {
    const T &field = *static_cast<const T *>(variable(config));
    if (!UnsetValue<T>::is(field)) {
      stream << key << " = " << field << endl;
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

    size_t i;
    for (i = 0; i < count; i++) {
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
      if (config::isCommentStart(c)) {
        return ReaderStatus::SKIP;
      }
      if (config::isAlpha(c)) {
        result = start;
        return ReaderStatus::SUCCESS;
      }
      if (!config::isWhiteSpace(c)) {
        cerr << "Unexpected characters @ " << (start - line) << ": " << line
             << endl;
        break;
      }
    }
    return ReaderStatus::FAULT;
  }

  ReaderStatus read_key(const char *&after_key, const char *start) {
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

  static ReaderStatus skip_assignment(const char *&value, const char *start,
                                      const char *line) {
    const char *rd = start;
    while (config::isWhiteSpace(*rd)) {
      rd++;
    }
    if (*rd != '=') {
      cerr << "Unexpected character @ " << (rd - line) << ": " << line << endl;
      cout << "1line :" << line << endl;
      cout << "1start:" << start << endl;
      return ReaderStatus::FAULT;
    }
    rd++;
    while (config::isWhiteSpace(*rd)) {
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
            const SpeakermanConfig &basedUpon) {
    if (reader_->is_deprecated()) {
      std::cout << "Warning: variable \"" << key_ << "\" is deprecated!"
                << std::endl;
    }
    return reader_->read(manager, key.c_str(), value, basedUpon);
  }

  void write(const SpeakermanConfig &config, ostream &output) const {
    reader_->write(config, key_.c_str(), output);
  }
};

class ConfigManager : protected SpeakermanConfig {
  KeyVariableReader **readers_ = nullptr;
  size_t capacity_ = 0;
  size_t size_ = 0;

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
  ConfigManager() {
    add_reader(KEY_SNIPPET_GROUP_COUNT, false, groups);
    add_reader(KEY_SNIPPET_CHANNELS, false, groupChannels);
    add_reader(KEY_SNIPPET_CROSSOVERS, false, crossovers);

    add_reader(KEY_SNIPPET_SUB_THRESHOLD, true, relativeSubThreshold);
    add_reader(KEY_SNIPPET_SUB_DELAY, true, subDelay);
    add_reader(KEY_SNIPPET_SUB_OUTPUT, false, subOutput);

    add_reader(KEY_SNIPPET_GENERATE_NOISE, true, generateNoise);
    add_reader(KEY_SNIPPET_INPUT_OFFSET, false, inputOffset);
    add_reader(KEY_SNIPPET_INPUT_COUNT, false, inputCount);

    add_reader(DetectionConfig::KEY_SNIPPET_MAXIMUM_WINDOW_SECONDS, false,
               detection.maximum_window_seconds);
    add_reader(DetectionConfig::KEY_SNIPPET_MINIMUM_WINDOW_SECONDS, false,
               detection.minimum_window_seconds);
    add_reader(DetectionConfig::KEY_SNIPPET_RMS_FAST_RELEASE_SECONDS, false,
               detection.rms_fast_release_seconds);
    add_reader(DetectionConfig::KEY_SNIPPET_PERCEPTIVE_LEVELS, false,
               detection.perceptive_levels);
    add_reader(DetectionConfig::KEY_SNIPPET_USE_BRICK_WALL_PREDICTION, false,
               detection.useBrickWallPrediction);

    string key;
    add_reader(GroupConfig::Definitions::equalizers.name(), true, eqs);
    string eqBase = "";
    eqBase += EqualizerConfig::KEY_SNIPPET_EQUALIZER;
    eqBase += "/";
    for (size_t eq_idx = 0; eq_idx < GroupConfig::MAX_EQS; eq_idx++) {
      string eqKey = eqBase;
      eqKey += (char)('0' + eq_idx);
      eqKey += "/";

      key = eqKey;
      key += EqualizerConfig::Definitions::center.name();
      add_reader(key, true, eq[eq_idx].center);
      key = eqKey;
      key += EqualizerConfig::Definitions::gain.name();
      add_reader(key, true, eq[eq_idx].gain);
      key = eqKey;
      key += EqualizerConfig::Definitions::bandwidth.name();
      add_reader(key, true, eq[eq_idx].bandwidth);
    }

    for (size_t group_idx = 0; group_idx < SpeakermanConfig::MAX_GROUPS;
         group_idx++) {
      string groupKey = GroupConfig::KEY_SNIPPET_GROUP;
      groupKey += "/";
      groupKey += (char)(group_idx + '0');
      groupKey += "/";

      key = groupKey;
      key += GroupConfig::Definitions::equalizers.name();
      add_reader(key, true, group[group_idx].eqs);
      key = groupKey;
      key += GroupConfig::Definitions::threshold.name();
      add_reader(key, true, group[group_idx].threshold);
      key = groupKey;
      key += GroupConfig::Definitions::volume.name();
      add_array_reader<double, MAX_SPEAKERMAN_GROUPS>(
          key, true, group[group_idx].volume[0]);
      key = groupKey;
      key += GroupConfig::Definitions::delay.name();
      add_reader(key, true, group[group_idx].delay);
      key = groupKey;
      key += GroupConfig::Definitions::useSub.name();
      add_reader(key, true, group[group_idx].use_sub);
      key = groupKey;
      key += GroupConfig::Definitions::mono.name();
      ;
      add_reader(key, true, group[group_idx].mono);
      key = groupKey;
      key += GroupConfig::KEY_SNIPPET_NAME;
      add_reader(key, true, group[group_idx].name);

      string eqBase = groupKey;
      eqBase += EqualizerConfig::KEY_SNIPPET_EQUALIZER;
      eqBase += "/";
      for (size_t eq_idx = 0; eq_idx < GroupConfig::MAX_EQS; eq_idx++) {
        string eqKey = eqBase;
        eqKey += (char)('0' + eq_idx);
        eqKey += "/";

        key = eqKey;
        key += EqualizerConfig::Definitions::center.name();
        add_reader(key, true, group[group_idx].eq[eq_idx].center);
        key = eqKey;
        key += EqualizerConfig::Definitions::gain.name();
        add_reader(key, true, group[group_idx].eq[eq_idx].gain);
        key = eqKey;
        key += EqualizerConfig::Definitions::bandwidth.name();
        add_reader(key, true, group[group_idx].eq[eq_idx].bandwidth);
      }
    }
  }

  size_t size() const { return size_; }

  bool read_line(SpeakermanConfig &config, const char *line,
                 const SpeakermanConfig &basedUpon) {
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
    return reader->read(config, reader->get_key(), value_start, basedUpon);
  }

  void dump(const SpeakermanConfig &config, ostream &stream) {
    for (size_t i = 0; i < size(); i++) {
      readers_[i]->write(config, stream);
    }
  }
};

static ConfigManager config_manager;

static bool fileExists(const char *fileName) {
  FILE *f = fopen(fileName, "r");
  if (f != nullptr) {
    fclose(f);
    return true;
  }
  return false;
}

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
          "Installation prefix not set. This should be done at compile time or "
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

static void actualReadConfig(SpeakermanConfig &config, istream &stream,
                             const SpeakermanConfig &basedUpon, bool initial) {
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
    } else if (line_pos < LINE_LENGTH) {
      line[line_pos++] = c;
      continue;
    } else if (line_pos == LINE_LENGTH) {
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

long long getFileTimeStamp(const char *fileName) {
  struct stat stats;
  long long stamp = 0;
  if (stat(fileName, &stats) == 0) {
    return stats.st_mtim.tv_sec;
  }
  return -1;
}

long long getConfigFileTimeStamp() {
  return getFileTimeStamp(configFileName());
}

SpeakermanConfig readSpeakermanConfig(const SpeakermanConfig &basedUpon,
                                      bool initial) {
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
  } catch (const ReadConfigException &e) {
    cerr << "E: " << e.what() << endl;
    return basedUpon;
  }
  dumpSpeakermanConfig(result, cout);
  result.timeStamp = stamp;
  return result;
}

SpeakermanConfig readSpeakermanConfig() {
  SpeakermanConfig basedUpon = SpeakermanConfig::defaultConfig();
  return readSpeakermanConfig(basedUpon, true);
}

void dumpSpeakermanConfig(const SpeakermanConfig &dump, ostream &output) {
  output << "# Speakerman configuration dump" << endl << endl;
  config_manager.dump(dump, output);
}

void GroupConfig::set_if_unset(const GroupConfig &config_if_unset) {
  size_t eq_idx;
  if (set_if_unset_or_invalid_config_value(eqs, config_if_unset.eqs, MIN_EQS,
                                           MAX_EQS)) {
    for (eq_idx = 0; eq_idx < eqs; eq_idx++) {
      eq[eq_idx] = config_if_unset.eq[eq_idx];
    }
  } else {
    for (eq_idx = 0; eq_idx < eqs; eq_idx++) {
      eq[eq_idx].set_if_unset(config_if_unset.eq[eq_idx]);
    }
  }
  for (; eq_idx < MAX_EQS; eq_idx++) {
    eq[eq_idx] = EqualizerConfig::unsetConfig();
  }

  for (size_t group = 0; group < MAX_SPEAKERMAN_GROUPS; group++) {
    set_if_unset_or_invalid_config_value(
        volume[group], config_if_unset.volume[group], MIN_VOLUME, MAX_VOLUME);
  }

  box_if_out_of_range(threshold, config_if_unset.threshold, MIN_THRESHOLD,
                      MAX_THRESHOLD);
  box_if_out_of_range(delay, config_if_unset.delay, MIN_DELAY, MAX_DELAY);
  box_if_out_of_range(use_sub, config_if_unset.use_sub, 0, 1);
  box_if_out_of_range(mono, config_if_unset.mono, 0, 1);
  if (UnsetValue<char[NAME_LENGTH + 1]>::is(name)) {
    name[0] = 0;
  }
}

const GroupConfig GroupConfig::defaultConfig(size_t group_id) {
  GroupConfig result;
  result = result.with_groups_separated(group_id);
  snprintf(result.name, NAME_LENGTH, "Group %zd", group_id + 1);
  return result;
}

const GroupConfig GroupConfig::unsetConfig() {
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
  result.name[0] = 0;
  return result;
}

const GroupConfig GroupConfig::with_groups_separated(size_t group_id) const {
  GroupConfig result = *this;
  for (size_t i = 0; i < MAX_SPEAKERMAN_GROUPS; i++) {
    result.volume[i] = i == group_id ? DEFAULT_VOLUME : 0;
  }
  return result;
}

const GroupConfig GroupConfig::with_groups_mixed() const

{
  GroupConfig result = *this;
  for (size_t i = 0; i < MAX_SPEAKERMAN_GROUPS; i++) {
    result.volume[i] = DEFAULT_VOLUME;
  }
  return result;
}

const DetectionConfig DetectionConfig::unsetConfig() {
  DetectionConfig result;

  unset_config_value(result.useBrickWallPrediction);
  unset_config_value(result.maximum_window_seconds);
  unset_config_value(result.minimum_window_seconds);
  unset_config_value(result.rms_fast_release_seconds);
  unset_config_value(result.perceptive_levels);

  return result;
}

void DetectionConfig::set_if_unset(const DetectionConfig &config_if_unset) {
  set_if_unset_config_value(useBrickWallPrediction,
                            config_if_unset.useBrickWallPrediction);
  box_if_out_of_range(maximum_window_seconds,
                      config_if_unset.maximum_window_seconds,
                      MIN_MAXIMUM_WINDOW_SECONDS, MAX_MAXIMUM_WINDOW_SECONDS);
  box_if_out_of_range(minimum_window_seconds,
                      config_if_unset.minimum_window_seconds,
                      MIN_MINIMUM_WINDOW_SECONDS, MAX_MINIMUM_WINDOW_SECONDS);
  box_if_out_of_range(
      rms_fast_release_seconds, config_if_unset.rms_fast_release_seconds,
      MIN_RMS_FAST_RELEASE_SECONDS, MAX_RMS_FAST_RELEASE_SECONDS);
  box_if_out_of_range(perceptive_levels, config_if_unset.perceptive_levels,
                      MIN_PERCEPTIVE_LEVELS, MAX_PERCEPTIVE_LEVELS);
}

const SpeakermanConfig SpeakermanConfig::defaultConfig() {
  SpeakermanConfig result;
  for (size_t i = 0; i < MAX_SPEAKERMAN_GROUPS; i++) {
    result.group[i] = GroupConfig::defaultConfig(i);
  }
  result.detection = DetectionConfig::defaultConfig();
  for (size_t i = 0; i < MAX_EQS; i++) {
    result.eq[i] = EqualizerConfig::unsetConfig();
  }
  return result;
}

const SpeakermanConfig SpeakermanConfig::unsetConfig() {
  SpeakermanConfig result;
  for (size_t i = 0; i < MAX_SPEAKERMAN_GROUPS; i++) {
    result.group[i] = GroupConfig::unsetConfig();
  }
  result.detection.unsetConfig();
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

void SpeakermanConfig::set_if_unset(const SpeakermanConfig &config_if_unset) {
  size_t group_idx;
  if (set_if_unset_or_invalid_config_value(groups, config_if_unset.groups,
                                           MIN_GROUPS, MAX_GROUPS)) {
    for (group_idx = 0; group_idx < groups; group_idx++) {
      group[group_idx] = config_if_unset.group[group_idx];
    }
  } else {
    for (group_idx = 0; group_idx < groups; group_idx++) {
      group[group_idx].set_if_unset(config_if_unset.group[group_idx]);
    }
  }
  for (; group_idx < MAX_GROUPS; group_idx++) {
    group[group_idx] = GroupConfig::unsetConfig();
  }
  detection.set_if_unset(config_if_unset.detection);
  size_t eq_idx;
  if (set_if_unset_or_invalid_config_value(eqs, config_if_unset.eqs, MIN_EQS,
                                           MAX_EQS)) {
    for (eq_idx = 0; eq_idx < eqs; eq_idx++) {
      eq[eq_idx] = config_if_unset.eq[eq_idx];
    }
  } else {
    for (eq_idx = 0; eq_idx < eqs; eq_idx++) {
      eq[eq_idx].set_if_unset(config_if_unset.eq[eq_idx]);
    }
  }
  for (; eq_idx < MAX_EQS; eq_idx++) {
    eq[eq_idx] = EqualizerConfig::unsetConfig();
  }

  set_if_unset_or_invalid_config_value(groupChannels,
                                       config_if_unset.groupChannels,
                                       MIN_GROUP_CHANNELS, MAX_GROUP_CHANNELS);
  set_if_unset_or_invalid_config_value(subOutput, config_if_unset.subOutput,
                                       MIN_SUB_OUTPUT, MAX_SUB_OUTPUT);
  set_if_unset_or_invalid_config_value(inputOffset, config_if_unset.inputOffset,
                                       MIN_INPUT_OFFSET, MAX_INPUT_OFFSET);

  set_if_unset_or_invalid_config_value(
      inputCount,
      !UnsetValue<size_t>::is(config_if_unset.inputOffset) &&
              config_if_unset.inputOffset > 0
          ? config_if_unset.inputOffset
          : groups * groupChannels,
      MIN_INPUT_COUNT, MAX_INPUT_COUNT);
  set_if_unset_or_invalid_config_value(
      relativeSubThreshold, config_if_unset.relativeSubThreshold,
      MIN_REL_SUB_THRESHOLD, MAX_REL_SUB_THRESHOLD);
  set_if_unset_or_invalid_config_value(subDelay, config_if_unset.subDelay,
                                       MIN_SUB_DELAY, MAX_SUB_DELAY);
  set_if_unset_or_invalid_config_value(generateNoise,
                                       config_if_unset.generateNoise, 0, 1);
  set_if_unset_or_invalid_config_value(
      threshold_scaling, config_if_unset.threshold_scaling,
      MIN_THRESHOLD_SCALING, MAX_THRESHOLD_SCALING);
  timeStamp = -1;
}

const SpeakermanConfig SpeakermanConfig::with_groups_mixed() const {
  SpeakermanConfig result = *this;

  for (size_t i = 0; i < groups; i++) {
    for (size_t j = 0; j < groups; j++) {
      result.group[i].volume[j] = group[j].volume[j];
    }
  }

  return result;
}

const SpeakermanConfig SpeakermanConfig::with_groups_separated() const {
  SpeakermanConfig result = *this;

  for (size_t i = 0; i < groups; i++) {
    for (size_t j = 0; j < groups; j++) {
      result.group[i].volume[j] = i == j ? group[j].volume[j] : 0;
    }
  }

  return result;
}

const SpeakermanConfig SpeakermanConfig::with_groups_first() const {
  SpeakermanConfig result = *this;

  for (size_t i = 0; i < groups; i++) {
    for (size_t j = 0; j < groups; j++) {
      result.group[i].volume[j] = j == 0 ? group[0].volume[0] : 0;
    }
  }

  return result;
}

StreamOwner::StreamOwner(std::ifstream &owned) : stream_(owned), owns_(true) {}

StreamOwner::StreamOwner(const StreamOwner &source)
    : stream_(source.stream_), owns_(false) {}

StreamOwner::StreamOwner(StreamOwner &&source) noexcept
    : stream_(source.stream_), owns_(true) {
  source.owns_ = false;
}
StreamOwner StreamOwner::open(const char *file_name) {
  std::ifstream stream;
  stream.open(file_name);
  return StreamOwner(stream);
}
StreamOwner::~StreamOwner() {
  if (owns_ && stream_.is_open()) {
    stream_.close();
  }
}
bool StreamOwner::is_open() const { return stream_.is_open(); }

} /* End of namespace speakerman */
