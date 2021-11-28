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

#include <fstream>
#include <istream>
#include <type_traits>
#include <unordered_map>

namespace speakerman {

using namespace tdap;

struct config {
  enum class CallbackResult { CONTINUE, STOP };

  /**
   * A callback that is called by #readConfig() each time a
   * key-value pair has been parsed.
   */
  typedef CallbackResult (*configReaderCallback)(const char *key,
                                                 const char *value, void *data);

  enum class ReadResult {
    SUCCESS,
    STOPPED,
    NO_CALLBACK,
    KEY_TOO_LONG,
    VALUE_TOO_LONG,
    INVALID_START_OF_LINE,
    INVALID_KEY_CHARACTER,
    INVALID_ASSIGNMENT,
    UNEXPECTED_EOL,
    UNEXPECTED_EOF
  };

  static char getEscaped(char c) {
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
      return c;
    }
  }

  static constexpr bool isWhiteSpace(char c) { return c == ' ' || c == '\t'; }

  static constexpr bool isLineDelimiter(char c) {
    return c == '\n' || c == '\r';
  }

  static constexpr bool isAssignment(char c) { return c == '=' || c == ':'; }

  static constexpr bool isCommentStart(char c) { return c == ';' || c == '#'; }

  static constexpr bool isEscape(char c) { return c == '\\'; }

  static constexpr bool isQuote(char c) { return c == '"' || c == '\''; }

  static constexpr bool isKeyChar(char c) {
    return isKeyStartChar(c) || c == '-' || c == '[' || c == ']';
  }

  static constexpr bool isKeyStartChar(char c) {
    return isAlphaNum(c) || c == '_' || c == '.' || c == '/';
  }

  static constexpr bool isAlphaNum(char c) { return isAlpha(c) || isNum(c); }

  static constexpr bool isAlpha(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
  }

  static constexpr bool isNum(char c) { return c >= '0' && c <= '9'; }

//  class Reader {
//    enum class ParseState {
//      START,
//      COMMENT,
//      KEYNAME,
//      ASSIGNMENT,
//      START_VALUE,
//      VALUE,
//      QUOTE,
//      ESC
//    };
//
//  public:
//    static constexpr size_t MAX_KEY_LENGTH = 127;
//    static constexpr size_t MAX_VALUE_LENGTH = 1023;
//
//    Reader() { setStartState(); }
//
//    ReadResult read(std::istream &stream, configReaderCallback callback,
//                    void *data) {
//      if (callback == nullptr) {
//        return ReadResult::NO_CALLBACK;
//      }
//      CallbackResult cbr;
//      setStartState();
//      ParseState popState_ = state_;
//      char quote = 0;
//      while (!stream.eof()) {
//        int i = stream.get();
//        if (i == -1) {
//          break;
//        }
//        char c = i;
//        switch (state_) {
//        case ParseState::START:
//          if (isCommentStart(c)) {
//            state_ = ParseState::COMMENT;
//            break;
//          }
//          if (isLineDelimiter(c)) {
//            break;
//          }
//          if (isKeyStartChar(c)) {
//            state_ = ParseState::KEYNAME;
//            addKeyChar(c);
//            break;
//          }
//          if (isWhiteSpace(c)) {
//            break;
//          }
//          return ReadResult::INVALID_START_OF_LINE;
//
//        case ParseState::COMMENT:
//          if (isLineDelimiter(c)) {
//            setStartState();
//          }
//          break;
//
//        case ParseState::KEYNAME:
//          if (isKeyChar(c)) {
//            if (!addKeyChar(c)) {
//              return ReadResult::KEY_TOO_LONG;
//            }
//            break;
//          }
//          if (isWhiteSpace(c)) {
//            state_ = ParseState::ASSIGNMENT;
//            break;
//          }
//          if (isAssignment(c)) {
//            state_ = ParseState::START_VALUE;
//            break;
//          }
//          return ReadResult::INVALID_KEY_CHARACTER;
//
//        case ParseState::ASSIGNMENT:
//          if (isAssignment(c)) {
//            state_ = ParseState::START_VALUE;
//            break;
//          }
//          if (isLineDelimiter(c)) {
//            if (reportKeyValue(callback, data) == CallbackResult::STOP) {
//              return ReadResult::STOPPED;
//            }
//            setStartState();
//            break;
//          }
//          if (isWhiteSpace(c)) {
//            break;
//          }
//
//          return ReadResult::INVALID_ASSIGNMENT;
//
//        case ParseState::START_VALUE:
//          if (isLineDelimiter(c)) {
//            if (reportKeyValue(callback, data) == CallbackResult::STOP) {
//              return ReadResult::STOPPED;
//            }
//            setStartState();
//            break;
//          }
//          if (isWhiteSpace(c)) {
//            break;
//          }
//          if (isEscape(c)) {
//            popState_ = ParseState::VALUE;
//            state_ = ParseState::ESC;
//            break;
//          }
//          if (isQuote(c)) {
//            state_ = ParseState::QUOTE;
//            quote = c;
//            break;
//          }
//          addValueChar(c);
//          break;
//
//        case ParseState::ESC:
//          if (isLineDelimiter(c)) {
//            return ReadResult::UNEXPECTED_EOL;
//          }
//          if (!addValueChar(getEscaped(c))) {
//            return ReadResult::VALUE_TOO_LONG;
//          }
//          state_ = popState_;
//
//          break;
//        case ParseState::VALUE:
//        case ParseState::QUOTE:
//          if (isLineDelimiter(c)) {
//            if (state_ == ParseState::QUOTE) {
//              return ReadResult::UNEXPECTED_EOL;
//            }
//            if (reportKeyValue(callback, data) == CallbackResult::STOP) {
//              return ReadResult::STOPPED;
//            }
//            setStartState();
//            break;
//          }
//          if (isEscape(c)) {
//            popState_ = state_;
//            state_ = ParseState::ESC;
//            break;
//          }
//          if (c == quote) {
//            if (reportKeyValue(callback, data) == CallbackResult::STOP) {
//              return ReadResult::STOPPED;
//            }
//            setStartState();
//          }
//          if (!addValueChar(c)) {
//            return ReadResult::VALUE_TOO_LONG;
//          }
//          break;
//        }
//      }
//      switch (state_) {
//      case ParseState::COMMENT:
//      case ParseState::START:
//        return ReadResult::SUCCESS;
//
//      case ParseState::VALUE:
//      case ParseState::START_VALUE:
//        if (reportKeyValue(callback, data) == CallbackResult::STOP) {
//          return ReadResult::STOPPED;
//        }
//        return ReadResult::SUCCESS;
//
//      default:
//        return ReadResult::UNEXPECTED_EOF;
//      }
//    }
//
//    ReadResult readFile(const char *fileName, configReaderCallback callback,
//                        void *data) {
//      std::ifstream stream;
//      stream.open(fileName);
//      if (stream.is_open()) {
//        try {
//          return read(stream, callback, data);
//        } catch (...) {
//          throw;
//        }
//      }
//      return ReadResult::UNEXPECTED_EOF;
//    }
//
//  private:
//    char key_[MAX_KEY_LENGTH + 1];
//    size_t keyLen_;
//    char value_[MAX_VALUE_LENGTH + 1];
//    size_t valueLen_;
//    ParseState state_;
//
//    bool addKeyChar(char c) {
//      if (keyLen_ == MAX_KEY_LENGTH) {
//        return false;
//      }
//      key_[keyLen_++] = c;
//      return true;
//    }
//
//    bool addValueChar(char c) {
//      if (valueLen_ == MAX_VALUE_LENGTH) {
//        return false;
//      }
//      value_[valueLen_++] = c;
//      return true;
//    }
//
//    void setStartState() {
//      state_ = ParseState::START;
//      keyLen_ = 0;
//      valueLen_ = 0;
//    }
//
//    CallbackResult reportKeyValue(configReaderCallback callback, void *data) {
//      key_[keyLen_] = 0;
//      value_[valueLen_] = 0;
//      return callback(key_, value_, data);
//    }
//  };
//
//  class Typed : protected Reader {
//    static CallbackResult callback(const char *key, const char *value,
//                                   void *data) {
//      return static_cast<Typed *>(data)->onKeyValue(key, value);
//    }
//
//  public:
//    ReadResult read(std::istream &stream) {
//      return Reader::read(stream, Typed::callback, this);
//    }
//
//    ReadResult readFile(const char *fileName) {
//      return Reader::readFile(fileName, Typed::callback, this);
//    }
//
//    virtual CallbackResult onKeyValue(const char *key, const char *value) = 0;
//
//    virtual ~Typed() = default;
//  };
};

enum class ValueSetPolicy { Fail, BoxValue, DefaultValue, FailReset };

enum class ValueSetResult { Ok, AppliedPolicy, Fail };

enum class ParserValueType { Integral, Boolean, Float, String, Unsupported };

template <typename T, ParserValueType type, size_t LENGTH = 0> struct ValueParser_ {};

template <typename T> struct ValueParser_<T, ParserValueType::Integral> {
  static_assert(std::is_integral<T>::value, "Expected integral type parameter");
  using V = long long int;

  static bool parse(T &value, const char *start, char *&end) {
    V parsed = strtoll(start, &end, 10);
    if (*end == '\0' || config::isWhiteSpace(*end) ||
        config::isCommentStart(*end)) {
      value = tdap::Value<T>::force_between(parsed,
                                            std::numeric_limits<T>::lowest(),
                                            std::numeric_limits<T>::max());
      return true;
    }
    return false;
  }
};

template <typename T> struct ValueParser_<T, ParserValueType::Boolean> {
  static_assert(std::is_integral<T>::value, "Expected integral type parameter");
  using V = int;

  static bool matches(const char *keyword, const char *start, const char *end) {
    size_t scan_length = end - start;
    size_t key_length = strnlen(keyword, 128);
    if (scan_length < key_length) {
      return false;
    }

    return strncasecmp(keyword, start, key_length) == 0 &&
           (start[key_length] == '\0' ||
            config::isWhiteSpace(start[key_length]));
  }

  static bool parse(T &field, const char *value, char *&end) {
    for (end = const_cast<char *>(value); config::isAlphaNum(*end); end++) {
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
  using V = long double;

  static bool parse(T &field, const char *value, char *&end) {
    V parsed = strtold(value, &end);
    if (*end == '\0' || config::isWhiteSpace(*end) ||
        config::isCommentStart(*end)) {
      field = tdap::Value<V>::force_between(parsed,
                                            std::numeric_limits<T>::lowest(),
                                            std::numeric_limits<T>::max());
      return true;
    }
    return false;
  }
};

template <typename T, size_t NAME_LENGTH> struct ValueParser_<T, ParserValueType::String, NAME_LENGTH> {
  static bool parse(T field, const char *value, char *&end) {
    const char *src = value;
    char *dst = field;
    while (src != 0 && (dst - field) < NAME_LENGTH) {
      char c = *src++;
      if (c == '\t' || c == ' ') {
        if (dst > field) {
          *dst++ = ' ';
        }
      } else if (config::isAlphaNum(c) || config::isQuote(c) ||
                 strchr(".!|,;:/[]{}*#@~%^()-_+=\\", c) != nullptr) {
        *dst++ = c;
      }
    }
    *dst++ = '\0';
    return true;
  }
};

template <typename T> static constexpr ParserValueType get_value_parser_type() {
  return std::is_floating_point<T>::value ? ParserValueType::Float
         : std::is_same<int, T>::value    ? ParserValueType::Boolean
         : std::is_integral<T>::value     ? ParserValueType::Integral
         : std::is_same<char*, T>::value
             ? ParserValueType::String
             : ParserValueType::Unsupported;
}

template <typename T, size_t NAME_LENGTH = 0>
struct ValueParser : public ValueParser_<T, get_value_parser_type<T>(), NAME_LENGTH> {};

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
  const ValueSetPolicy policy_;

  static ValueAndResult set_with_policy(T newValue, T min, T def, T max,
                                        ValueSetPolicy policy) {
    if (newValue >= min && newValue <= max) {
      return {newValue, true, ValueSetResult::Ok};
    }
    switch (policy) {
    case ValueSetPolicy::BoxValue:
      return {newValue < min   ? min
              : newValue > max ? max
                               : newValue,
              true, ValueSetResult::AppliedPolicy};
    case ValueSetPolicy::DefaultValue:
      return {def, true, ValueSetResult::AppliedPolicy};
    case ValueSetPolicy::FailReset:
    case ValueSetPolicy::Fail:
    default:
      return {def, false, ValueSetResult::Fail};
    }
  }

public:
  constexpr ConfigNumericDefinition(
      T min, T def, T max, const char *name,
      ValueSetPolicy policy = ValueSetPolicy::BoxValue)
      : min_(min < max ? min : max), max_(max > min ? max : min),
        def_(def < min   ? min
             : def > max ? max
                         : def),
        name_(name ? name : "[undefined]"), policy_(policy) {}
  [[nodiscard]] T min() const { return min_; }
  [[nodiscard]] T max() const { return max_; }
  [[nodiscard]] T def() const { return def_; }
  [[nodiscard]] const char *name() const { return name_; }
  [[nodiscard]] ValueSetPolicy policy() const { return policy_; }

  ValueAndResult setWithUpper(T newValue, T upper) const {
    return set_with_policy(newValue, min(), def(), std::min(upper, max()),
                           policy());
  }

  ValueAndResult setWithLower(T newValue, T lower) const {
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

  bool same_range(const ConfigNumericDefinition &other) const {
    return &other == this || (min_ == other.min_ && max_ == other.max_);
  }

  bool compatible_range(const ConfigNumericDefinition &other) const {
    return &other == this || (min_ <= other.min_ && max_ >= other.max_);
  }

  bool same_number(const ConfigNumericDefinition &other) const {
    return &other == this ||
           (min_ == other.min_ && def_ == other.def_ && max_ == other.max_);
  }
};

template <typename T> class ConfigNumeric {
  const ConfigNumericDefinition<T> &definition_;
  using Value = typename ConfigNumericDefinition<T>::Value;
  typename ConfigNumericDefinition<T>::Value value_;

public:
  ConfigNumeric(const ConfigNumericDefinition<T> &definition)
      : definition_(definition), value_({definition_.def(), false}) {}
  const ConfigNumericDefinition<T> &definition() const { return definition_; }

  ConfigNumeric &operator=(const ConfigNumeric &source) {
    if (&source == this) {
      return *this;
    }
    value_.assign(definition_.set(source.get()));
    return *this;
  }

  void reset() { value_ = {definition_.def(), false}; }

  ValueSetResult setWithUpper(T newValue, T upper) {
    return value_.assign(definition_.setWithUpper(newValue, upper));
  }

  ValueSetResult setWithLower(T newValue, T lower) {
    return value_.assign(definition_.setWithUpper(newValue, lower));
  }

  ValueSetResult setBounded(T &value, bool &set, T newValue, T lower, T upper) {
    return value_.assign(definition_.setBounded(newValue, upper, lower));
  }

  ValueSetResult set(T &value, bool &set, T newValue) {
    return value_.assign(definition_.set(newValue));
  }

  bool is_set() const { return value_.set; }

  T get() const { return is_set() ? value_.value : definition_.def(); }

  T get_with_fallback(const T fallback) const {
    if (is_set()) {
      return value_;
    }
    auto r = definition_.set(fallback);
    return r.set ? r.value : definition_.def();
  }
};

template <typename T, size_t C> class ConfigNumericArray {
public:
  static constexpr size_t CAPACITY = C;

private:
  const ConfigNumericDefinition<T> &definition_;
  using Value = typename ConfigNumericDefinition<T>::Value;
  Value data_[CAPACITY];

  Value &ref(size_t i) { return data_[tdap::IndexPolicy::force(i, CAPACITY)]; }

  const Value &ref(size_t i) const {
    return data_[tdap::IndexPolicy::force(i, CAPACITY)];
  }

public:
  ConfigNumericArray(const ConfigNumericDefinition<T> &definition)
      : definition_(definition) {}

  explicit ConfigNumericArray(const ConfigNumericArray &source)
      : definition_(source.definition_) {
    this->operator=(source);
  }

  const ConfigNumericDefinition<T> &definition() const { return definition_; }

  constexpr size_t capacity() const { return CAPACITY; }

  size_t length() const {
    for (size_t i = 0; i < CAPACITY; i++) {
      if (!data_[i].set) {
        return i + 1;
      }
    }
    return CAPACITY;
  }

  void reset() {
    for (size_t i = 0; i < CAPACITY; i++) {
      data_[i].value = {definition_.def(), false};
    }
  }

  ConfigNumericArray &operator=(const ConfigNumericArray &source) {
    if (source == *this) {
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

  ValueSetResult setWithUpper(size_t index, T newValue, T upper) {
    return ref(index).assign(definition_.setWithUpper(newValue, upper));
  }

  ValueSetResult setWithLower(size_t index, T newValue, T lower) {
    return ref(index).assign(definition_.setWithUpper(newValue, lower));
  }

  ValueSetResult setBounded(size_t index, T &value, bool &set, T newValue,
                            T lower, T upper) {
    return ref(index).assign(definition_.setBounded(newValue, upper, lower));
  }

  ValueSetResult set(size_t index, T &value, bool &set, T newValue) {
    return ref(index).assign(definition_.set(newValue));
  }

  bool is_set(size_t index) const {
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
  T get_with_fallback(size_t index, const T fallback) {
    if (is_set(index)) {
      return ref(index).value;
    }
    auto r = definition_.set(fallback);
    return r.set ? r.value : definition_.def();
  }
};



} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_CONFIG_GUARD_H_ */
