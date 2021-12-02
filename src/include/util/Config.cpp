//
// Created by michel on 02-12-21.
//

#include <speakerman/utils/Config.hpp>

namespace tdap::config {

KeyValueParser::KeyValueParser(const tdap::config::CharClassifier &classifier,
                               size_t keyLength, size_t valueLength)
    : cls(classifier), maxKeyLen_(std::clamp(keyLength, 1lu, MAX_KEY_LENGTH)),
      maxValueLen_(std::clamp(valueLength, 1lu, MAX_VALUE_LENGTH)),
      key_(new char[maxKeyLen_ + 1]), value_(new char[maxValueLen_ + 1]) {
  setStartState();
}

ReadResult KeyValueParser::read(KeyValueParser::Reader &reader,
                                configReaderCallback callback, void *data) {
  if (callback == nullptr) {
    return ReadResult::NoCallback;
  }
  setStartState();
  ParseState popState_ = state_;
  char quote = 0;
  char escapeChar = 0;
  char c;
  while (reader.read(c)) {
    switch (state_) {
    case ParseState::Start:
      if (cls.isCommentStart(c)) {
        state_ = ParseState::Comment;
        break;
      }
      if (cls.isLineDelimiter(c)) {
        break;
      }
      if (cls.isKeyStartChar(c)) {
        state_ = ParseState::KeyName;
        addKeyChar(c);
        break;
      }
      if (cls.isWhiteSpace(c)) {
        break;
      }
      return ReadResult::InvalidStartOfLine;

    case ParseState::Comment:
      if (cls.isLineDelimiter(c)) {
        setStartState();
      }
      break;

    case ParseState::KeyName:
      if (cls.isKeyChar(c)) {
        if (!addKeyChar(c)) {
          return ReadResult::KeyTooLong;
        }
        break;
      }
      if (cls.isWhiteSpace(c)) {
        state_ = ParseState::Assignment;
        break;
      }
      if (cls.isAssignment(c)) {
        state_ = ParseState::StartValue;
        break;
      }
      return ReadResult::InvalidKeyCharacter;

    case ParseState::Assignment:
      if (cls.isAssignment(c)) {
        state_ = ParseState::StartValue;
        break;
      }
      if (cls.isLineDelimiter(c)) {
        if (reportKeyValue(callback, data) == CallbackResult::Stop) {
          return ReadResult::Stopped;
        }
        setStartState();
        break;
      }
      if (cls.isWhiteSpace(c)) {
        break;
      }

      return ReadResult::InvalidAssignment;

    case ParseState::StartValue:
      if (cls.isLineDelimiter(c)) {
        if (reportKeyValue(callback, data) == CallbackResult::Stop) {
          return ReadResult::Stopped;
        }
        setStartState();
        break;
      }
      if (cls.isWhiteSpace(c)) {
        break;
      }
      if (cls.isEscape(c)) {
        popState_ = ParseState::Value;
        state_ = ParseState::Escaped;
        escapeChar = c;
        break;
      }
      if (cls.isQuote(c)) {
        state_ = ParseState::Quote;
        quote = c;
        break;
      }
      addValueChar(c);
      break;

    case ParseState::Escaped:
      if (cls.isLineDelimiter(c)) {
        return ReadResult::UnexpectedEol;
      }
      if (!addValueChar(cls.getEscaped(c, escapeChar))) {
        return ReadResult::ValueTooLong;
      }
      state_ = popState_;
      escapeChar = 0;
      break;
    case ParseState::Value:
    case ParseState::Quote:
      if (cls.isLineDelimiter(c)) {
        if (state_ == ParseState::Quote) {
          return ReadResult::UnexpectedEol;
        }
        if (reportKeyValue(callback, data) == CallbackResult::Stop) {
          return ReadResult::Stopped;
        }
        setStartState();
        break;
      }
      if (cls.isEscape(c)) {
        popState_ = state_;
        state_ = ParseState::Escaped;
        escapeChar = c;
        break;
      }
      if (c == quote) {
        if (reportKeyValue(callback, data) == CallbackResult::Stop) {
          return ReadResult::Stopped;
        }
        setStartState();
      }
      if (!addValueChar(c)) {
        return ReadResult::ValueTooLong;
      }
      break;
    }
  }
  switch (state_) {
  case ParseState::Comment:
  case ParseState::Start:
    return ReadResult::Success;

  case ParseState::Value:
  case ParseState::StartValue:
    if (reportKeyValue(callback, data) == CallbackResult::Stop) {
      return ReadResult::Stopped;
    }
    return ReadResult::Success;

  default:
    return ReadResult::UnexpectedEof;
  }
}
bool KeyValueParser::addKeyChar(char c) {
  if (keyLen_ == maxKeyLen_) {
    return false;
  }
  key_[keyLen_++] = c;
  return true;
}
bool KeyValueParser::addValueChar(char c) {
  if (valueLen_ == maxValueLen_) {
    return false;
  }
  value_[valueLen_++] = c;
  return true;
}
void KeyValueParser::setStartState() {
  state_ = ParseState::Start;
  keyLen_ = 0;
  valueLen_ = 0;
}
CallbackResult KeyValueParser::reportKeyValue(configReaderCallback callback,
                                              void *data) {
  key_[keyLen_] = 0;
  value_[valueLen_] = 0;
  return callback(key_, value_, data);
}
KeyValueParser::~KeyValueParser() {
  delete key_;
  delete value_;
}

ReadResult MappingKeyValueParser::parse(KeyValueParser::Reader &reader) {
  std::lock_guard<std::mutex> guard(m_);
  return parser_.read(reader, callback, this);
}
bool MappingKeyValueParser::add(const std::string &key,
                                AbstractValueHandler *handler) {
  if (!handler) {
    return false;
  }
  std::lock_guard<std::mutex> guard(m_);
  return keyMap.emplace(key, handler).second;
}
bool MappingKeyValueParser::replace(const std::string &key,
                                    AbstractValueHandler *handler) {
  if (!handler) {
    return false;
  }
  std::lock_guard<std::mutex> guard(m_);
  auto entry = keyMap.find(key);
  if (entry != keyMap.end()) {
    if (entry->second) {
      delete entry->second;
    }
    entry->second = handler;
    return true;
  }
  return false;
}
bool MappingKeyValueParser::remove(const std::string &key) {
  std::lock_guard<std::mutex> guard(m_);
  AbstractValueHandler *p = nullptr;
  auto entry = keyMap.find(key);
  if (entry == keyMap.end() || entry->second == nullptr) {
    return false;
  }
  delete entry->second;
  std::remove(keyMap.begin(), keyMap.end(), entry);
  return true;
}
void MappingKeyValueParser::removeAll() {
  std::lock_guard<std::mutex> guard(m_);
  for (auto entry = keyMap.begin(); entry != keyMap.end(); entry++) {
    if (entry->second != nullptr) {
      delete entry->second;
    }
  }
  keyMap.clear();
}
MappingKeyValueParser::~MappingKeyValueParser() {
  std::lock_guard<std::mutex> guard(m_);
  removeAll();
}
CallbackResult MappingKeyValueParser::callback(const char *key,
                                               const char *value, void *data) {
  return static_cast<MappingKeyValueParser *>(data)->handleKeyAndValue(key,
                                                                       value);
}
CallbackResult MappingKeyValueParser::handleKeyAndValue(const char *key,
                                                        const char *value) {
  keyString = key;
  auto entry = keyMap.find(key);

  if (entry == keyMap.end()) {
    keyNotFound(key, value);
    return CallbackResult::Continue;
  }
  const char *error;
  const char *message;
  if (!entry->second->handleValue(value, &message, &error)) {
    errorHandlingValue(key, value, error, message);
  }
  return CallbackResult::Continue;
}
} // namespace tdap::config
