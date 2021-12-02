/*
 * Config.hpp
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

#ifndef SMS_SPEAKERMAN_CONFIG_GUARD_H_
#define SMS_SPEAKERMAN_CONFIG_GUARD_H_

#include <algorithm>
#include <cstring>
#include <istream>
#include <mutex>
#include <tdap/IndexPolicy.hpp>
#include <type_traits>
#include <unordered_map>

namespace tdap::config {

class CharClassifier {
public:
  virtual bool isWhiteSpace(char c) const = 0;

  virtual bool isLineDelimiter(char c) const = 0;

  virtual bool isAssignment(char c) const = 0;

  virtual bool isCommentStart(char c) const = 0;

  virtual bool isEscape(char c) const = 0;

  virtual bool isQuote(char c) const = 0;

  virtual bool isKeyChar(char c) const const = 0;

  virtual bool isKeyStartChar(char c) const const = 0;

  virtual bool isAlpha(char c) const = 0;

  virtual bool isNum(char c) const = 0;

  virtual char getEscaped(char escapeChar, char c) const = 0;

  virtual ~CharClassifier() = default;

  bool isAlphaNum(char c) const { return isAlpha(c) || isNum(c); }
};

struct AsciiCharClassifier : public CharClassifier {
  bool isWhiteSpace(char c) const final { return c == ' ' || c == '\t'; }

  bool isLineDelimiter(char c) const final { return c == '\n' || c == '\r'; }

  bool isAssignment(char c) const final { return c == '=' || c == ':'; }

  bool isCommentStart(char c) const final { return c == ';' || c == '#'; }

  bool isEscape(char c) const final { return c == '\\'; }

  bool isQuote(char c) const final { return c == '"' || c == '\''; }

  bool isKeyChar(char c) const final {
    return isKeyStartChar(c) || c == '-' || c == '.';
  }

  bool isKeyStartChar(char c) const final { return isAlpha(c) || c == '_'; }

  bool isAlpha(char c) const final {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
  }

  bool isNum(char c) const final { return c >= '0' && c <= '9'; }

  char getEscaped(char escapeChar, char c) const final {
    if (isEscape(escapeChar)) {
      switch (c) {
      case '\\':
        return '\\';
      case 'b':
        return '\b';
      case 'r':
        return '\r';
      case 'n':
        return '\n';
      case 't':
        return '\t';
      default:
        break;
      }
    }
    return c;
  }

  static const AsciiCharClassifier &instance() {
    static const AsciiCharClassifier c;
    return c;
  }
};

enum class CallbackResult { Continue [[maybe_unused]], Stop };

/**
 * A callback that is called by #readConfig() each time a
 * key-value pair has been parsed.
 */
typedef CallbackResult (*configReaderCallback)(const char *key,
                                               const char *value, void *data);

enum class ReadResult {
  Success,
  Stopped,
  NoCallback,
  KeyTooLong,
  ValueTooLong,
  InvalidStartOfLine,
  InvalidKeyCharacter,
  InvalidAssignment,
  UnexpectedEol,
  UnexpectedEof
};

class KeyValueParser {
  const tdap::config::CharClassifier &cls;

  enum class ParseState {
    Start,
    Comment,
    KeyName,
    Assignment,
    StartValue,
    Value,
    Quote,
    Escaped
  };

public:
  static constexpr size_t MAX_KEY_LENGTH = 127;
  static constexpr size_t MAX_VALUE_LENGTH = 1023;

  class Reader {
  public:
    virtual bool read(char &result) = 0;
  };

  KeyValueParser(const tdap::config::CharClassifier &classifier);
  KeyValueParser();

  ReadResult read(Reader &reader, configReaderCallback callback, void *data);

private:
  char key_[MAX_KEY_LENGTH + 1] = {0};
  size_t keyLen_ = 0;
  char value_[MAX_VALUE_LENGTH + 1] = {0};
  size_t valueLen_ = 0;
  ParseState state_ = ParseState::Start;

  bool addKeyChar(char c);
  bool addValueChar(char c);
  void setStartState();
  CallbackResult reportKeyValue(configReaderCallback callback, void *data);
};

class AbstractValueHandler {
public:
  virtual bool handleValue(const char *value, const char **errorMessage,
                           const char **errorPosition) {};
  virtual ~AbstractValueHandler() = default;
};

class MappingKeyValueParser {
public:
  MappingKeyValueParser(KeyValueParser &parser) : parser_(parser) {}

  ReadResult parse(KeyValueParser::Reader &reader);

  bool add(const std::string &key, AbstractValueHandler *handler);

  bool replace(const std::string &key, AbstractValueHandler *handler);

  bool remove(const std::string &key);

  void removeAll();

  virtual void keyNotFound(const char *, const char *) {}
  virtual void errorHandlingValue(const char *key, const char *value,
                                  const char *message,
                                  const char *errorPosition) {}

  virtual ~MappingKeyValueParser();
private:
  std::unordered_map<std::string, AbstractValueHandler *> keyMap;
  std::mutex m_;
  KeyValueParser &parser_;
  std::string keyString;

  static CallbackResult callback(const char *key, const char *value,
                                 void *data);

  CallbackResult handleKeyAndValue(const char *key, const char *value);
};

} // namespace tdap::config

namespace speakerman {

struct config {};

enum class InvalidValuePolicy { Fail, Fit };

enum class ValueSetResult { Ok, Fitted, Fail };

enum class ParserValueType { Integral, Boolean, Float, String, Unsupported };

template <typename T, ParserValueType type, size_t LENGTH = 0>
struct ValueParser_ {};

template <typename T> struct ValueParser_<T, ParserValueType::Integral> {
  static_assert(std::is_integral<T>::value, "Expected integral type parameter");
  static const tdap::config::CharClassifier &classifier() {
    return tdap::config::AsciiCharClassifier::instance();
  }

  using V = long long int;

  [[maybe_unused]] static bool parse(T &value, const char *start, char *&end) {
    V parsed = strtoll(start, &end, 10);
    if (*end == '\0' || classifier().isWhiteSpace(*end) ||
        classifier().isCommentStart(*end)) {
      value = std::clamp(parsed, std::numeric_limits<T>::lowest(),
                         std::numeric_limits<T>::max());
      return true;
    }
    return false;
  }
};

template <typename T> struct ValueParser_<T, ParserValueType::Boolean> {
  static_assert(std::is_integral<T>::value, "Expected integral type parameter");
  static const tdap::config::CharClassifier &classifier() {
    return tdap::config::AsciiCharClassifier::instance();
  }
  using V = int;

  static bool matches(const char *keyword, const char *start, const char *end) {
    size_t scan_length = end - start;
    size_t key_length = strnlen(keyword, 128);
    if (scan_length < key_length) {
      return false;
    }

    return strncasecmp(keyword, start, key_length) == 0 &&
           (start[key_length] == '\0' ||
            classifier().isWhiteSpace(start[key_length]));
  }

  [[maybe_unused]] static bool parse(T &field, const char *value, char *&end) {
    for (end = const_cast<char *>(value); classifier().isAlphaNum(*end);
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
    return false;
  }
};

template <typename T> struct ValueParser_<T, ParserValueType::Float> {
  static_assert(std::is_floating_point<T>::value,
                "Expected floating point type parameter");
  static const tdap::config::CharClassifier &classifier() {
    return tdap::config::AsciiCharClassifier::instance();
  }
  using V = long double;

  [[maybe_unused]] static bool parse(T &field, const char *value, char *&end) {
    V parsed = strtold(value, &end);
    if (*end == '\0' || classifier().isWhiteSpace(*end) ||
        classifier().isCommentStart(*end)) {
      field = std::clamp(parsed, std::numeric_limits<T>::lowest(),
                         std::numeric_limits<T>::max());
      return true;
    }
    return false;
  }
};

template <typename T, size_t NAME_LENGTH>
struct ValueParser_<T, ParserValueType::String, NAME_LENGTH> {
  static const tdap::config::CharClassifier &classifier() {
    return tdap::config::AsciiCharClassifier::instance();
  }
  [[maybe_unused]] static bool parse(T field, const char *value, char *&end) {
    const char *src = value;
    char *dst = field;
    while (src != nullptr && (dst - field) < NAME_LENGTH) {
      char c = *src++;
      if (c == '\t' || c == ' ') {
        if (dst > field) {
          *dst++ = ' ';
        }
      } else if (classifier().isAlphaNum(c) || classifier().isQuote(c) ||
                 strchr(".!|,;:/[]{}*#@~%^()-_+=\\", c) != nullptr) {
        *dst++ = c;
      }
    }
    *dst = '\0';
    return true;
  }
};

template <typename T> static constexpr ParserValueType get_value_parser_type() {
  return std::is_floating_point<T>::value ? ParserValueType::Float
         : std::is_same<int, T>::value    ? ParserValueType::Boolean
         : std::is_integral<T>::value     ? ParserValueType::Integral
         : std::is_same<char *, T>::value ? ParserValueType::String
                                          : ParserValueType::Unsupported;
}

template <typename T, size_t NAME_LENGTH = 0>
struct ValueParser
    : public ValueParser_<T, get_value_parser_type<T>(), NAME_LENGTH> {};

template <typename T> struct StringValueOperations {

  static bool equals(const T *const value1, const T *const value2,
                     size_t length) {
    if (!value1) {
      return !value2;
    } else if (!value2) {
      return false;
    }
    const T *p1 = value1;
    const T *p2 = value2;
    const T *end = p1 + length;
    for (const T *p1 = value1, *p2 = value2; p1 <= end && *p1 || *p2;
         ++p1, ++p2) {
      if (*p1 != *p2) {
        return false;
      }
    }
    return *p1 || *p2;
  }

  static ValueSetResult
  copy(T *const destination, const T *const source, size_t maxLength,
       InvalidValuePolicy policy = InvalidValuePolicy::Fit) {
    if (!(source && *source)) {
      if (!destination) {
        return ValueSetResult::Fail;
      }
      destination[0] = 0;
      return ValueSetResult::Ok;
    }
    size_t length = 0;
    if (policy == InvalidValuePolicy::Fit) {
      while (length <= maxLength && source[length]) {
        destination[length] = source[length];
        ++length;
      }
      destination[length] = 0;
      return length <= maxLength || !source[length] ? ValueSetResult::Ok
                                                    : ValueSetResult::Fitted;
    }
    while (length <= maxLength && source[length]) {
      ++length;
    }
    if (length <= maxLength || !source[length]) {
      for (size_t i = 0; i < length; i++) {
        destination[i] == source[i];
      }
      destination[length] = 0;
      return ValueSetResult::Ok;
    }
    return ValueSetResult::Fail;
  }
};

template <typename T> class ConfigNumericDefinition {
public:
  struct ValueAndResult;
  struct Value {
    T value;
    bool set;

    ValueSetResult assign(const ValueAndResult &r) {
      if (r.result != ValueSetResult::Fail) {
        value = r.value;
        set = r.set;
      }
      return r.result;
    }
  };

  struct ValueAndResult : public Value {
    ValueSetResult result;
  };

private:
  const T min_;
  const T def_;
  const T max_;
  const char *const name_;
  const InvalidValuePolicy policy_;

  static ValueAndResult set_with_policy(T newValue, T min, T def, T max,
                                        InvalidValuePolicy policy) {
    if (newValue >= min && newValue <= max) {
      return {newValue, true, ValueSetResult::Ok};
    }
    switch (policy) {
    case InvalidValuePolicy::Fit:
      return {newValue < min   ? min
              : newValue > max ? max
                               : newValue,
              true, ValueSetResult::Fitted};
    default:
      return {def, false, ValueSetResult::Fail};
    }
  }

public:
  [[maybe_unused]] constexpr ConfigNumericDefinition(
      T min, T def, T max, const char *name,
      InvalidValuePolicy policy = InvalidValuePolicy::Fit)
      : min_(min < max ? min : max), max_(max > min ? max : min),
        def_(def < min   ? min
             : def > max ? max
                         : def),
        name_(name ? name : "[undefined]"), policy_(policy) {}
  [[nodiscard]] T min() const { return min_; }
  [[nodiscard]] T max() const { return max_; }
  [[nodiscard]] T def() const { return def_; }
  [[nodiscard]] const char *name() const { return name_; }
  [[nodiscard]] InvalidValuePolicy policy() const { return policy_; }

  ValueAndResult setWithUpper(T newValue, T upper) const {
    return set_with_policy(newValue, min(), def(), std::min(upper, max()),
                           policy());
  }

  [[maybe_unused]] ValueAndResult setWithLower(T newValue, T lower) const {
    return set_with_policy(newValue, std::max(lower, min()), def(), max(),
                           policy());
  }

  ValueAndResult setBounded(T newValue, T lower, T upper) const {
    return set_with_policy(newValue, std::max(lower, min()), def(),
                           std::min(upper, max()), policy());
  }

  ValueAndResult set(T newValue) const {
    return set_with_policy(newValue, min(), def(), max(), policy());
  }

  bool operator==(const ConfigNumericDefinition &other) const {
    return &other == this ||
           (min_ == other.min_ && def_ == other.def_ && max_ == other.max_ &&
            (name_ == other.name_ || strncmp(name_, other.name_, 128) == 0));
  }

  bool compatible_range(const ConfigNumericDefinition &other) const {
    return &other == this || (min_ <= other.min_ && max_ >= other.max_);
  }
};

template <typename T> class ConfigNumeric {
  const ConfigNumericDefinition<T> &definition_;
  using Value = typename ConfigNumericDefinition<T>::Value;
  typename ConfigNumericDefinition<T>::Value value_;

public:
  [[maybe_unused]] explicit ConfigNumeric(
      const ConfigNumericDefinition<T> &definition)
      : definition_(definition), value_({definition_.def(), false}) {}
  [[maybe_unused]] const ConfigNumericDefinition<T> &definition() const {
    return definition_;
  }

  ConfigNumeric &operator=(const ConfigNumeric &source) {
    if (&source == this) {
      return *this;
    }
    value_.assign(definition_.set(source.get()));
    return *this;
  }

  void reset() { value_ = {definition_.def(), false}; }

  [[maybe_unused]] ValueSetResult setWithUpper(T newValue, T upper) {
    return value_.assign(definition_.setWithUpper(newValue, upper));
  }

  [[maybe_unused]] ValueSetResult setWithLower(T newValue, T lower) {
    return value_.assign(definition_.setWithUpper(newValue, lower));
  }

  [[maybe_unused]] ValueSetResult setBounded(T &value, bool &set, T newValue,
                                             T lower, T upper) {
    return value_.assign(definition_.setBounded(newValue, upper, lower));
  }

  ValueSetResult set(T &value, bool &set, T newValue) {
    return value_.assign(definition_.set(newValue));
  }

  [[nodiscard]] bool is_set() const { return value_.set; }

  T get() const { return is_set() ? value_.value : definition_.def(); }

  [[maybe_unused]] T getWithFallback(const T fallback) const {
    if (is_set()) {
      return value_;
    }
    auto r = definition_.set(fallback);
    return r.set ? r.value : definition_.def();
  }

  bool operator==(const ConfigNumeric &other) const {
    return get() == other.get();
  }
};

template <typename T, size_t C> class ConfigNumericArray {
public:
  static constexpr size_t CAPACITY = C;

private:
  const ConfigNumericDefinition<T> &definition_;
  using Value = typename ConfigNumericDefinition<T>::Value;
  Value data_[CAPACITY];

  const Value &ref(size_t i) const {
    return data_[tdap::IndexPolicy::force(i, CAPACITY)];
  }

public:
  [[maybe_unused]] explicit ConfigNumericArray(
      const ConfigNumericDefinition<T> &definition)
      : definition_(definition) {}

  ConfigNumericArray(const ConfigNumericArray &source)
      : definition_(source.definition_) {
    this->operator=(source);
  }

  const ConfigNumericDefinition<T> &definition() const { return definition_; }

  [[nodiscard]] constexpr size_t capacity() const { return CAPACITY; }

  [[nodiscard]] size_t length() const {
    size_t len = 0;
    while (len < CAPACITY && data_[len].set) {
      len++;
    }
    return len;
  }

  void reset() {
    for (size_t i = 0; i < CAPACITY; i++) {
      data_[i].value = {definition_.def(), false};
    }
  }

  ConfigNumericArray &operator=(const ConfigNumericArray &source) {
    if (&source == this) {
      return *this;
    }
    size_t i;
    if (definition_.compatible_range(source.definition())) {
      for (i = 0; i < CAPACITY && source.data_[i].set; i++) {
        data_[i] = source.data_[i];
      }
    } else {
      size_t length;
      for (length = 0; length < CAPACITY && source.data_[length].set;
           length++) {
        // No relaxing policies like boxing or defaulting allowed!
        if (definition_.set(source.data_[length].value) != ValueSetResult::Ok) {
          return *this;
        }
      }
      for (i = 0; i < length; i++) {
        data_[i] = source.data_[i];
      }
    }
    for (; i < CAPACITY; i++) {
      data_[i] = {definition_.def(), false};
    }
    return *this;
  }

  [[maybe_unused]] ValueSetResult setWithUpper(size_t index, T newValue,
                                               T upper) {
    return ref(index).assign(definition_.setWithUpper(newValue, upper));
  }

  [[maybe_unused]] ValueSetResult setWithLower(size_t index, T newValue,
                                               T lower) {
    return ref(index).assign(definition_.setWithUpper(newValue, lower));
  }

  [[maybe_unused]] ValueSetResult setBounded(size_t index, T &value, bool &set,
                                             T newValue, T lower, T upper) {
    return ref(index).assign(definition_.setBounded(newValue, upper, lower));
  }

  ValueSetResult set(size_t index, T &value, bool &set, T newValue) {
    return ref(index).assign(definition_.set(newValue));
  }

  [[nodiscard]] bool is_set(size_t index) const {
    if (index >= CAPACITY) {
      throw std::out_of_range("Index out of range for config value array");
    }
    for (size_t i = 0; i < index; i++) {
      if (!data_[i].set) {
        return false;
      }
    }
    return data_[index].set;
  }
  T get(size_t index) const {
    auto e = ref(index);
    return e.set ? e.value : definition_.def();
  }
  [[maybe_unused]] T getWithFallback(size_t index, const T fallback) {
    if (is_set(index)) {
      return ref(index).value;
    }
    auto r = definition_.set(fallback);
    return r.set ? r.value : definition_.def();
  }

  bool operator==(const ConfigNumericArray &other) const {
    bool mineDone = false;
    bool yoursDone = false;
    for (size_t i = 0; i < CAPACITY; i++) {
      const Value mine = data_[i];
      const Value yours = other.data_[i];
      mineDone |= !mine.set;
      yoursDone |= !yours.set;
      if (mineDone) {
        if (yoursDone) {
          break;
        }
        if (yours.value != definition_.def()) {
          return false;
        }
      } else if (yoursDone) {
        if (mine.value != definition_.def()) {
          return false;
        }
      } else if (mine.value != yours.value) {
        return false;
      }
    }
    return true;
  }
};

class StringValueValidator {
public:
  virtual bool validate(const char *value, size_t maxLength,
                        const char **end) const {
    if (!value) {
      return false;
    }
    size_t i;
    for (i = 0; i < maxLength; i++) {
      if (!value[i]) {
        return true;
      }
      // Does not accept UTF-8 more than one byte characters
      if ((value[i] & 0x80) != 0) {
        break;
      }
    }
    if (end) {
      *end = value + i;
    }
    return false;
  };
  virtual ~StringValueValidator() = default;
};

template <size_t N, class V = StringValueValidator>
class ConfigStringFormatDefinition {
  static_assert(N > 0);
  static_assert(std::is_base_of<StringValueValidator, V>::value);

public:
  static bool validate(const char *value, const char **end) {
    static const V validator_instance;
    return validator_instance.validate(value, N, end);
  }
};

template <class V> class ConfigStringDefinition {
  /**
   * Using "substitution failure is not an error" techniques to establish that
   * the template type argument is a ConfigStringFormatDefinition, and if so:
   * pick the important parts of it.
   */
  static constexpr auto substitutionArgument = static_cast<const V *>(nullptr);
  template <size_t N1, typename V2>
  [[maybe_unused]] static constexpr auto
  subst(const ConfigStringFormatDefinition<N1, V2> *p) {
    return p;
  }
  static constexpr bool subst(...) { return false; }
  [[maybe_unused]] static constexpr auto configFmtPtr =
      subst(substitutionArgument);
  static_assert(std::is_same_v<decltype(configFmtPtr), const V *const>);

  template <size_t N1, typename V2>
  static constexpr auto
  getValidator(const ConfigStringFormatDefinition<N1, V2> *) {
    return ConfigStringFormatDefinition<N1, V2>::validate;
  }
  template <size_t N1, typename V2>
  static constexpr auto
  getLength(const ConfigStringFormatDefinition<N1, V2> *) {
    return N1;
  }

public:
  static constexpr size_t length = getLength(substitutionArgument);
  static constexpr auto validateFunction = getValidator(substitutionArgument);

  [[maybe_unused]] constexpr ConfigStringDefinition(const char *name,
                                                    const char *defaultValue)
      : name_(name), def_(defaultValue) {}

  static bool validate(const char *value, const char **end) {
    return validateFunction(value, end);
  }

  [[nodiscard]] const char *name() const {
    return name_ != nullptr ? name_ : "[no-name]";
  }
  [[nodiscard]] const char *def() const {
    return def_ != nullptr ? def_ : "[no-default]";
  }

private:
  const char *const name_;
  const char *const def_;
};

template <class Def> class [[maybe_unused]] ConfigString {
  static constexpr auto substitutionArgument =
      static_cast<const Def *>(nullptr);
  template <class V>
  [[maybe_unused]] static constexpr auto
  subst(const ConfigStringDefinition<V> *p) {
    return p;
  }
  static constexpr bool subst(...) { return false; }
  [[maybe_unused]] static constexpr auto configDef =
      subst(substitutionArgument);

  static_assert(std::is_same_v<decltype(configDef), const Def *const>);

  template <class V>
  static constexpr auto getValidator(const ConfigStringDefinition<V> *) {
    return ConfigStringDefinition<V>::validateFunction;
  }
  template <class V>
  static constexpr size_t getLength(const ConfigStringDefinition<V> *) {
    return ConfigStringDefinition<V>::length;
  }
  template <class V>
  static const char *getName(const ConfigStringDefinition<V> &def) {
    return def.name();
  }
  template <class V>
  static const char *getDefault(const ConfigStringDefinition<V> &def) {
    return def.def();
  }

public:
  static constexpr size_t length = getLength(substitutionArgument);
  static constexpr auto validateFunction = getValidator(substitutionArgument);

private:
  char value_[length + 1] = {0};
  bool set_ = false;
  const Def &definition_;

public:
  [[nodiscard]] const char *name() const { return getName(definition_); }

  [[nodiscard]] const char *def() const { return getDefault(definition_); }

  explicit ConfigString(const Def &definition) : definition_(definition) {
    value_[0] = 0;
  }

  [[nodiscard]] bool is_set() const { return set_; }

  [[nodiscard]] const char *get() const { return set_ ? value_ : def(); }

  void reset() {
    value_[0] = 0;
    set_ = false;
  }

  ValueSetResult
  setValue(const char *newValue,
           InvalidValuePolicy policy = InvalidValuePolicy::Fail) {
    if (newValue && validateFunction(newValue, nullptr)) {
      strncpy(value_, newValue, length);
      value_[length] = 0;
      set_ = true;
      return ValueSetResult::Ok;
    }
    if (policy == InvalidValuePolicy::FailReset) {
      reset();
    }
    return ValueSetResult::Fail;
  }

  ConfigString &operator=(const ConfigString &source) {
    if (&source != this && source.is_set()) {
      strncpy(value_, source.value_, length);
      value_[length] = 0;
      set_ = true;
    }
    return *this;
  }

  template <class X> ConfigString &operator=(const ConfigString<X> &source) {
    if (source.is_set()) {
      setValue(source.get());
    }
    return *this;
  }
};

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_CONFIG_GUARD_H_ */
