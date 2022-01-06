/*
 * Names.hpp
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

#include <mutex>

#include <speakerman/jack/Names.hpp>
#include <tdap/Allocation.hpp>
#include <tdap/Count.hpp>

namespace speakerman {

using namespace std;
using namespace tdap;

static Names names_;

Names::Names() {
  get_port_regex();
  get_client_regex();
  get_full_regex();
}

void Names::checkLengthOrThrow(size_t bufferSize, int result) {
  if (result > 0 && (size_t)(result) < bufferSize) {
    return;
  }
  throw std::logic_error(
      "Jack port/client name regular expression initialization error");
}

size_t Names::client_port_separator_length() {
  return strlen(client_port_separator());
}

const char *Names::template_name_regex() {
  return "[-_\\.,0-9a-zA-Z ]{%zu,%zu}";
}

size_t Names::template_name_regex_length() {
  return strlen(template_name_regex());
}

size_t Names::pattern_max_length() {
  static const size_t value =
      2 +                                // Begin and end markers ^$
      2 * template_name_regex_length() + // basic tempates without length
      client_port_separator_length() +   // client/port separator
      2 * MAX_SIZE_LENGTH;
  return value;
}

size_t Names::pattern_max_buffer_size() {
  return pattern_max_length() + 1;
}

std::string Names::get_name_pattern(size_t clientLength, size_t portLength) {
  static const int SIZE = pattern_max_buffer_size();
  std::unique_ptr<char> formatTemplateP(new char [SIZE]);
  auto formatTemplate = formatTemplateP.get();
  std::unique_ptr<char> patternP(new char [SIZE]);
  auto pattern = patternP.get();
  if (clientLength > 0 && portLength > 0) {
    checkLengthOrThrow(
        SIZE, snprintf(formatTemplate, SIZE, "^%s%s%s$", template_name_regex(),
                       client_port_separator(), template_name_regex()));
  } else {
    checkLengthOrThrow(
        SIZE, snprintf(formatTemplate, SIZE, "^%s$", template_name_regex()));
  }
  if (clientLength > 0) {
    if (portLength > 0) {
      checkLengthOrThrow(SIZE, snprintf(pattern, SIZE, formatTemplate,
                                        minimum_name_length, clientLength,
                                        minimum_name_length, portLength));
    } else {
      checkLengthOrThrow(SIZE, snprintf(pattern, SIZE, formatTemplate,
                                        minimum_name_length, clientLength));
    }
  } else if (portLength > 0) {
    checkLengthOrThrow(SIZE, snprintf(pattern, SIZE, formatTemplate,
                                      minimum_name_length, portLength));
  } else {
    snprintf(pattern, SIZE, "^$");
  }

  return pattern;
}

const char *Names::valid_name(const regex &regex, const char *name,
                              const char *description) {
  consecutive_alloc::Disable disable;
  if (regex_match(name, regex)) {
    return name;
  }
  string message = "Invalid name (";
  message += description;
  message += "): '";
  message += name;
  message += "'";
  throw std::invalid_argument(message);
}

const char *Names::client_port_separator() { return ":"; }

size_t Names::get_full_size() {
  static const size_t value = jack_port_name_size();
  return value;
}

size_t Names::get_client_size() {
  static const size_t value = jack_client_name_size();
  return value;
}

size_t Names::get_port_size() {
  static const size_t value =
      get_full_size() - get_client_size() - client_port_separator_length();
  return value;
}

const std::string Names::get_port_pattern() {
  static const string pattern = get_name_pattern(0, get_port_size());
  return pattern;
}

const std::string Names::get_client_pattern() {
  static const string pattern = get_name_pattern(get_client_size(), 0);
  return pattern;
}

const std::string Names::get_full_pattern() {
  static const string pattern =
      get_name_pattern(get_client_size(), get_port_size());
  return pattern;
}

const std::regex &Names::get_port_regex() {
  static const regex NAME_REGEX(get_port_pattern());
  return NAME_REGEX;
}

const std::regex &Names::get_client_regex() {
  static const regex NAME_REGEX(get_client_pattern());
  return NAME_REGEX;
}

const std::regex &Names::get_full_regex() {
  static const regex NAME_REGEX(get_full_pattern());
  return NAME_REGEX;
}

bool Names::is_valid_port(const char *unchecked) {
  return std::regex_match(unchecked, get_port_regex());
}

bool Names::is_valid_port_full(const char *unchecked) {
  return std::regex_match(unchecked, get_port_regex());
}

bool Names::is_valid_client(const char *unchecked) {
  return std::regex_match(unchecked, get_port_regex());
}

std::string &Names::valid_port_full(string &unchecked) {
  valid_name(get_full_regex(), unchecked.c_str(), "full port");
  return unchecked;
}

const std::string &Names::valid_port_full(const string &unchecked) {
  valid_name(get_full_regex(), unchecked.c_str(), "full port");
  return unchecked;
}

const char *Names::valid_port_full(const char *unchecked) {
  return valid_name(get_full_regex(), unchecked, "full port");
}

std::string &Names::valid_client(string &unchecked) {
  valid_name(get_client_regex(), unchecked.c_str(), "port");
  return unchecked;
}

const std::string &Names::valid_client(const string &unchecked) {
  valid_name(get_client_regex(), unchecked.c_str(), "port");
  return unchecked;
}

const char *Names::valid_client(const char *unchecked) {
  return valid_name(get_client_regex(), unchecked, "port");
}

std::string &Names::valid_port(string &unchecked) {
  valid_name(get_port_regex(), unchecked.c_str(), "port");
  return unchecked;
}

const std::string &Names::valid_port(const string &unchecked) {
  valid_name(get_port_regex(), unchecked.c_str(), "port");
  return unchecked;
}

const char *Names::valid_port(const char *unchecked) {
  return valid_name(get_port_regex(), unchecked, "port");
}

size_t NameListPolicy::checkAndGetlength(const NameList &,
                                         const char *name) const {
  return strlen(name);
}

size_t NameListPolicy::maxNames() const { return Count<const char *>::max(); }

size_t NameListPolicy::maxCharacters() const { return Count<char>::max(); }

void NameList::ensureCapacity(size_t length) {
  const char *oldCharacters = characters_;
  policy_.ensureCapacity(characters_, characterCapacity_, characterCount_,
                         characterCount_ + length + 1, policy_.maxCharacters());
  if (oldCharacters != characters_) {
    correctNames(oldCharacters, characters_);
  }
  policy_.ensureCapacity(names_, nameCapacity_, nameCount_, nameCount_ + 1,
                         policy_.maxNames());
}

void NameList::correctNames(const char *oldChars, const char *newChars) {
  ptrdiff_t offset = newChars - oldChars;
  for (size_t i = 0; i < nameCount_; i++) {
    names_[i] += offset;
  }
}

NameList::NameList(const NameListPolicy &policy, size_t initialNameCapacity,
                   size_t initialCharacterCapacity)
    : nameCount_(0), nameCapacity_(initialNameCapacity),
      names_(initialNameCapacity > 0 ? new const char *[initialNameCapacity]
                                     : nullptr),
      characterCount_(0), characterCapacity_(initialCharacterCapacity),
      characters_(initialCharacterCapacity > 0
                      ? new char[initialCharacterCapacity]
                      : nullptr),
      policy_(policy) {}

NameList::NameList(const NameListPolicy &policy) : NameList(policy, 16, 1024) {}

NameList::NameList(NameList &&source)
    : nameCount_(source.nameCount_), nameCapacity_(source.nameCapacity_),
      names_(source.names_), characterCount_(source.characterCount_),
      characterCapacity_(source.characterCapacity_),
      characters_(source.characters_), policy_(source.policy_) {
  source.removeAll();
  source.characters_ = nullptr;
  source.names_ = nullptr;
}

NameList::NameList(const NameList &source)
    : nameCount_(source.nameCount_), nameCapacity_(source.nameCapacity_),
      names_(nameCapacity_ > 0 ? new const char *[nameCapacity_] : nullptr),
      characterCount_(source.characterCount_),
      characterCapacity_(source.characterCapacity_),
      characters_(characterCapacity_ > 0 ? new char[characterCapacity_]
                                         : nullptr),
      policy_(source.policy_) {
  memmove(characters_, source.characters_, characterCount_);
  // NOT same as correctNames()
  ptrdiff_t offset = characters_ - source.characters_;
  for (size_t i = 0; i < nameCount_; i++) {
    names_[i] = source.names_[i] + offset;
  }
}

void NameList::add(const char *name) {
  size_t length = policy_.checkAndGetlength(*this, name);
  ensureCapacity(length);
  char *addedName = characters_ + characterCount_;
  strncpy(addedName, name, length);
  addedName[length] = '\0';
  characterCount_ += length + 1;
  names_[nameCount_++] = addedName;
}

const char *NameList::get(size_t i) const {
  if (i < nameCount_) {
    return names_[i];
  }
  throw std::invalid_argument("Name index out of bounds");
}

const char *NameList::operator[](size_t i) const { return get(i); }

void NameList::removeAll() {
  characterCount_ = 0;
  nameCount_ = 0;
}

void NameList::free() {
  if (names_) {
    delete[] names_;
  }
  if (characters_) {
    delete[] characters_;
  }
  removeAll();
}

NameList::~NameList() { free(); }

size_t PortNames::rangeCheck(size_t index) const {
  if (index < count_) {
    return index;
  }
  throw out_of_range("Port name index out of range");
}

size_t PortNames::countPorts(const char **portNames, size_t maxSensibleNames) {
  if (!portNames) {
    return 0;
  }
  const char **name = portNames;
  size_t count = 0;
  while (name[count] != nullptr && count <= maxSensibleNames) {
    count++;
  }
  if (count > maxSensibleNames) {
    throw std::invalid_argument("Names list not null-terminated");
  }
  return count;
}

PortNames::PortNames(const char **names, FreeNames free,
                     size_t maxSensibleNames)
    : portNames_(names), count_(countPorts(portNames_, maxSensibleNames)),
      free_(free) {}

PortNames::PortNames(PortNames &&source)
    : portNames_(source.portNames_), count_(source.count_),
      free_(source.free_) {
  source.portNames_ = nullptr;
  source.count_ = 0;
}

size_t PortNames::count() const { return count_; }

const char *PortNames::get(size_t idx) const {
  return portNames_[rangeCheck(idx)];
}

const char *PortNames::operator[](size_t idx) const { return get(idx); }

PortNames::~PortNames() {
  if (portNames_ && free_) {
    free_(portNames_);
  }
}

} /* End of namespace speakerman */
