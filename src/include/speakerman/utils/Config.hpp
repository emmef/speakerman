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

  class Reader {
    enum class ParseState {
      START,
      COMMENT,
      KEYNAME,
      ASSIGNMENT,
      START_VALUE,
      VALUE,
      QUOTE,
      ESC
    };

  public:
    static constexpr size_t MAX_KEY_LENGTH = 127;
    static constexpr size_t MAX_VALUE_LENGTH = 1023;

    Reader() { setStartState(); }

    ReadResult read(std::istream &stream, configReaderCallback callback,
                    void *data) {
      if (callback == nullptr) {
        return ReadResult::NO_CALLBACK;
      }
      setStartState();
      ParseState popState_ = state_;
      char quote = 0;
      while (!stream.eof()) {
        int i = stream.get();
        if (i == -1) {
          break;
        }
        char c = i;
        switch (state_) {
        case ParseState::START:
          if (isCommentStart(c)) {
            state_ = ParseState::COMMENT;
            break;
          }
          if (isLineDelimiter(c)) {
            break;
          }
          if (isKeyStartChar(c)) {
            state_ = ParseState::KEYNAME;
            addKeyChar(c);
            break;
          }
          if (isWhiteSpace(c)) {
            break;
          }
          return ReadResult::INVALID_START_OF_LINE;

        case ParseState::COMMENT:
          if (isLineDelimiter(c)) {
            setStartState();
          }
          break;

        case ParseState::KEYNAME:
          if (isKeyChar(c)) {
            if (!addKeyChar(c)) {
              return ReadResult::KEY_TOO_LONG;
            }
            break;
          }
          if (isWhiteSpace(c)) {
            state_ = ParseState::ASSIGNMENT;
            break;
          }
          if (isAssignment(c)) {
            state_ = ParseState::START_VALUE;
            break;
          }
          return ReadResult::INVALID_KEY_CHARACTER;

        case ParseState::ASSIGNMENT:
          if (isAssignment(c)) {
            state_ = ParseState::START_VALUE;
            break;
          }
          if (isLineDelimiter(c)) {
            if (reportKeyValue(callback, data) == CallbackResult::STOP) {
              return ReadResult::STOPPED;
            }
            setStartState();
            break;
          }
          if (isWhiteSpace(c)) {
            break;
          }

          return ReadResult::INVALID_ASSIGNMENT;

        case ParseState::START_VALUE:
          if (isLineDelimiter(c)) {
            if (reportKeyValue(callback, data) == CallbackResult::STOP) {
              return ReadResult::STOPPED;
            }
            setStartState();
            break;
          }
          if (isWhiteSpace(c)) {
            break;
          }
          if (isEscape(c)) {
            popState_ = ParseState::VALUE;
            state_ = ParseState::ESC;
            break;
          }
          if (isQuote(c)) {
            state_ = ParseState::QUOTE;
            quote = c;
            break;
          }
          addValueChar(c);
          break;

        case ParseState::ESC:
          if (isLineDelimiter(c)) {
            return ReadResult::UNEXPECTED_EOL;
          }
          if (!addValueChar(getEscaped(c))) {
            return ReadResult::VALUE_TOO_LONG;
          }
          state_ = popState_;

          break;
        case ParseState::VALUE:
        case ParseState::QUOTE:
          if (isLineDelimiter(c)) {
            if (state_ == ParseState::QUOTE) {
              return ReadResult::UNEXPECTED_EOL;
            }
            if (reportKeyValue(callback, data) == CallbackResult::STOP) {
              return ReadResult::STOPPED;
            }
            setStartState();
            break;
          }
          if (isEscape(c)) {
            popState_ = state_;
            state_ = ParseState::ESC;
            break;
          }
          if (c == quote) {
            if (reportKeyValue(callback, data) == CallbackResult::STOP) {
              return ReadResult::STOPPED;
            }
            setStartState();
          }
          if (!addValueChar(c)) {
            return ReadResult::VALUE_TOO_LONG;
          }
          break;
        }
      }
      switch (state_) {
      case ParseState::COMMENT:
      case ParseState::START:
        return ReadResult::SUCCESS;

      case ParseState::VALUE:
      case ParseState::START_VALUE:
        if (reportKeyValue(callback, data) == CallbackResult::STOP) {
          return ReadResult::STOPPED;
        }
        return ReadResult::SUCCESS;

      default:
        return ReadResult::UNEXPECTED_EOF;
      }
    }

    ReadResult readFile(const char *fileName, configReaderCallback callback,
                        void *data) {
      std::ifstream stream;
      stream.open(fileName);
      if (stream.is_open()) {
        try {
          return read(stream, callback, data);
        } catch (...) {
          throw;
        }
      }
      return ReadResult::UNEXPECTED_EOF;
    }

  private:
    char key_[MAX_KEY_LENGTH + 1];
    size_t keyLen_;
    char value_[MAX_VALUE_LENGTH + 1];
    size_t valueLen_;
    ParseState state_;

    bool addKeyChar(char c) {
      if (keyLen_ == MAX_KEY_LENGTH) {
        return false;
      }
      key_[keyLen_++] = c;
      return true;
    }

    bool addValueChar(char c) {
      if (valueLen_ == MAX_VALUE_LENGTH) {
        return false;
      }
      value_[valueLen_++] = c;
      return true;
    }

    void setStartState() {
      state_ = ParseState::START;
      keyLen_ = 0;
      valueLen_ = 0;
    }

    CallbackResult reportKeyValue(configReaderCallback callback, void *data) {
      key_[keyLen_] = 0;
      value_[valueLen_] = 0;
      return callback(key_, value_, data);
    }
  };

  class Typed : protected Reader {
    static CallbackResult callback(const char *key, const char *value,
                                   void *data) {
      return static_cast<Typed *>(data)->onKeyValue(key, value);
    }

  public:
    ReadResult read(std::istream &stream) {
      return Reader::read(stream, Typed::callback, this);
    }

    ReadResult readFile(const char *fileName) {
      return Reader::readFile(fileName, Typed::callback, this);
    }

    virtual CallbackResult onKeyValue(const char *key, const char *value) = 0;

    virtual ~Typed() = default;
  };
};

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_CONFIG_GUARD_H_ */
