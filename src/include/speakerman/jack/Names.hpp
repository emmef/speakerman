/*
 * Names.cpp
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

#ifndef SMS_SPEAKERMAN_NAMES_GUARD_H_
#define SMS_SPEAKERMAN_NAMES_GUARD_H_

#include <regex>

#include <jack/jack.h>
#include <tdap/CapacityPolicy.hpp>

namespace speakerman {

using namespace std;
using namespace tdap;

class Names {
  static void checkLengthOrThrow(size_t bufferSize, int result);

  static constexpr size_t MAX_SIZE_LENGTH = 20;

  static constexpr size_t minimum_name_length = 2;

  static size_t client_port_separator_length();

  static const char *template_name_regex();

  static size_t template_name_regex_length();

  static size_t pattern_max_length();

  static size_t pattern_max_buffer_size();

  static string get_name_pattern(size_t clientLength, size_t portLength);

  static const char *valid_name(const regex &regex, const char *name,
                                const char *description);

public:
  Names();

  static const char *client_port_separator();

  static size_t get_full_size();

  static size_t get_client_size();

  static size_t get_port_size();

  static const string get_port_pattern();

  static const string get_client_pattern();

  static const string get_full_pattern();

  static const regex &get_port_regex();

  static const regex &get_client_regex();

  static const regex &get_full_regex();

  static bool is_valid_port(const char *unchecked);

  static bool is_valid_port_full(const char *unchecked);

  static bool is_valid_client(const char *unchecked);

  static const char *valid_port(const char *unchecked);

  static const char *valid_port_full(const char *unchecked);

  static const char *valid_client(const char *unchecked);

  static const string &valid_port(const string &unchecked);

  static const string &valid_port_full(const string &unchecked);

  static const string &valid_client(const string &unchecked);

  static string &valid_port(string &unchecked);

  static string &valid_port_full(string &unchecked);

  static string &valid_client(string &unchecked);
};

class NameList;

class NameListPolicy : public CapacityPolicy {
public:
  virtual size_t checkAndGetlength(const NameList &list,
                                   const char *name) const;

  virtual size_t maxNames() const;

  virtual size_t maxCharacters() const;
};

class NameList {
  size_t nameCount_;
  size_t nameCapacity_;
  const char **names_ = nullptr;
  size_t characterCount_;
  size_t characterCapacity_;
  char *characters_ = nullptr;
  const NameListPolicy &policy_;

  void ensureCapacity(size_t length);

  void correctNames(const char *oldChars, const char *newChars);

public:
  NameList(const NameListPolicy &policy, size_t initialNameCapacity,
           size_t initialCharacterCapacity);

  NameList(const NameListPolicy &policy);

  NameList(NameList &&source);

  NameList(const NameList &source);

  void add(const char *name);

  const char *get(size_t i) const;

  const char *operator[](size_t i) const;

  size_t count() const { return nameCount_; }

  size_t characters() const { return characterCount_; }

  /**
   * Remove all names, but don't free any memory
   */
  void removeAll();

  void free();

  ~NameList();
};

class PortNames {
public:
  typedef void (*FreeNames)(const char **names);

  const char **portNames_;
  size_t count_;
  FreeNames free_;

  size_t rangeCheck(size_t index) const;

  static size_t countPorts(const char **portNames, size_t maxSensibleNames);

public:
  PortNames(const char **names, FreeNames free, size_t maxSensibleNames);

  PortNames(PortNames &&source);

  size_t count() const;

  const char *get(size_t idx) const;

  const char *operator[](size_t idx) const;

  ~PortNames();
};

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_NAMES_GUARD_H_ */
