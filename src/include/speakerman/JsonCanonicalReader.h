#ifndef SPEAKERMAN_M_JSON_CANONICAL_READER_H
#define SPEAKERMAN_M_JSON_CANONICAL_READER_H
/*
 * speakerman/JsonCanonicalReader.h
 *
 * Added by michel on 2022-02-05
 * Copyright (C) 2015-2022 Michel Fleur.
 * Source https://github.com/emmef/speakerman
 * Email speakerman@emmef.org
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
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <istream>
#include <org-simple/text/Json.h>

namespace speakerman {

class PartitionBasedJsonStringBuilder
    : public org::simple::text::JsonStringBuilder {
  char *start = nullptr;
  char *rendered = nullptr;
  char *last = nullptr;
  char *local = nullptr;
  char *at = nullptr;

  static size_t validLength(size_t length);

protected:
  const char *getValue() const final;
  bool addChar(const char &c) final;

public:
  explicit PartitionBasedJsonStringBuilder(size_t maxLength);

  size_t getLength() const final;

  void resetValue() final;

  const char *getTotalString(char separator) const;

  void setLocal(char *newValue);

  char *getAt() const;
  char *getLocal() const;

  ~PartitionBasedJsonStringBuilder();
};

class JsonCanonicalReader : protected org::simple::text::JsonContext {
  struct StackEntry {
    char *start = nullptr;
  };

  static constexpr size_t validDepth(size_t depth);

  StackEntry *stack;
  StackEntry *stackLast;
  StackEntry *stackAt = nullptr;
  PartitionBasedJsonStringBuilder path;
  PartitionBasedJsonStringBuilder value;

  void checkPush() const;

  void checkPop() const;

protected:
  org::simple::text::JsonStringBuilder &nameBuilder() final;

  org::simple::text::JsonStringBuilder &stringBuilder() final;

  void pushIndex(int index) final;

  void popIndex() final;

  void pushName(const char *name) final;

  void popName() final;

  void setString(const char *string) final;

  void setNumber(const char *string) final;

  void setBoolean(bool value) final;

  void setNull() final;

public:
  JsonCanonicalReader(size_t pathLength, size_t valueLength, size_t depth);

  virtual void setString(const char *path, const char *string) = 0;

  virtual void setNumber(const char *path, const char *string) = 0;

  virtual void setBoolean(const char *path, bool value) = 0;

  virtual void setNull(const char *path) = 0;

  void readJson(org::simple::text::InputStream<char> &input,
                org::simple::text::TextFilePositionData<char> &position) {
    JsonContext::readJson(*this, input, position);
  }

  void readJson(std::istream &input) {
    class Stream : public org::simple::text::InputStream<char> {
      std::istream &input;

    public:
      Stream(std::istream &in) : input(in) {}
      bool get(char &c) final {
        if (!input.eof()) {
          input.get(c);
          return true;
        }
        return false;
      }
    } stream(input);

    JsonContext::readJson(*this, stream);
  }
};

} // namespace speakerman

#endif // SPEAKERMAN_M_JSON_CANONICAL_READER_H
