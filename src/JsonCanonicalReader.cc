//
// Created by michel on 05-02-22.
//

#include <speakerman/JsonCanonicalReader.h>

namespace speakerman {

size_t PartitionBasedJsonStringBuilder::validLength(size_t length) {
  if (length > 1 && length < 1048576lu) {
    return length;
  }
  throw std::invalid_argument("PartitionBasedJsonStringBuilder: length must "
                              "be at least 1 and not exceed 1048576.");
}
const char *PartitionBasedJsonStringBuilder::getValue() const { return local; }
bool PartitionBasedJsonStringBuilder::addChar(const char &c) {
  if (at < last) {
    *at++ = c;
    *at = '\0';
    return true;
  }
  return false;
}
PartitionBasedJsonStringBuilder::PartitionBasedJsonStringBuilder(
    size_t maxLength)
    : start(new char[validLength(maxLength) + 1]),
      rendered(new char[maxLength + 1]), last(start + maxLength), local(start),
      at(local) {
  start[0] = '\0';
}
size_t PartitionBasedJsonStringBuilder::getLength() const { return at - local; }
void PartitionBasedJsonStringBuilder::resetValue() {
  at = local;
  *at = '\0';
}
const char *PartitionBasedJsonStringBuilder::getTotalString(char separator) const {
  char *dst = rendered;
  for (const char *src = start; src < at; src++, dst++) {
    const char c = *src;
    if (c != '\0') {
      *dst = c;
    }
    else {
      *dst = separator;
    }
  }
  dst--;
  while (dst > rendered && *dst == separator) {
    dst--;
  }
  dst++;
  *dst = '\0';
  return rendered;
}
void PartitionBasedJsonStringBuilder::setLocal(char *newValue) {
  if (newValue >= start && newValue < last - 1) {
    local = newValue;
    at = local;
    *at = '\0';
    return;
  }
  throw std::range_error("Start value out of range");
}
char *PartitionBasedJsonStringBuilder::getAt() const { return at; }
char *PartitionBasedJsonStringBuilder::getLocal() const { return local; }
PartitionBasedJsonStringBuilder::~PartitionBasedJsonStringBuilder() {
  delete[] start;
  start = nullptr;
  local = nullptr;
  at = nullptr;
  last = nullptr;
}

constexpr size_t JsonCanonicalReader::validDepth(size_t depth) {
  if (depth > 1 && depth < 1024u) {
    return depth;
  }
  throw std::invalid_argument(
      "JsonCanonicalReader: depth must be at least 1 and not exceed 1024.");
}
void JsonCanonicalReader::checkPush() const {
  if (stackAt >= stackLast) {
    throw std::runtime_error(
        "JsonCanonicalReader: Push: maximum depth would be exceeded.");
  }
}
void JsonCanonicalReader::checkPop() const {
  if (stackAt <= stack) {
    throw std::runtime_error("JsonCanonicalReader: Pop: already topmost.");
  }
}
org::simple::util::text::JsonStringBuilder &JsonCanonicalReader::nameBuilder() {
  return path;
}
org::simple::util::text::JsonStringBuilder &
JsonCanonicalReader::stringBuilder() {
  return value;
}
void JsonCanonicalReader::pushIndex(int index) {
  checkPush();
  if (path.getLength() != 0) {
    std::runtime_error("JsonCanonicalReader: unexpected index pushIndex with "
                       "non-empty string.");
  }
  if (index < 0 || index > 999) {
    std::runtime_error("JsonCanonicalReader: index out of range.");
  }
  if (index >= 100) {
    path.add(char('0' + (index / 100)));
  }
  if (index >= 10) {
    path.add(char('0' + ((index % 100) / 10)));
  }
  path.add(char('0' + (index % 10)));
  path.setLocal(path.getAt() + 1);
  stackAt++;
  stackAt->start = path.getAt();
}
void JsonCanonicalReader::popIndex() {
  checkPop();
  stackAt--;
  path.setLocal(stackAt->start);
}
void JsonCanonicalReader::pushName(const char *name) {
  if (std::strncmp(path.getLocal(), name, path.getLength()) == 0) {
    checkPush();
    path.setLocal(path.getAt() + 1);
    stackAt++;
    stackAt->start = path.getLocal();
    return;
  }
  throw std::runtime_error("JsonCanonicalReader: unexpected name pushed.");
}
void JsonCanonicalReader::popName() {
  checkPop();
  stackAt--;
  path.setLocal(stackAt->start);
}
void JsonCanonicalReader::setString(const char *string) {
  setString(path.getTotalString('/'), string);
}
void JsonCanonicalReader::setNumber(const char *string) {
  setNumber(path.getTotalString('/'), string);
}
void JsonCanonicalReader::setBoolean(bool value) {
  setBoolean(path.getTotalString('/'), value);
}
void JsonCanonicalReader::setNull() { setNull(path.getTotalString('/')); }
JsonCanonicalReader::JsonCanonicalReader(size_t pathLength, size_t valueLength,
                                         size_t depth)
    : stack(new StackEntry[validDepth(depth)]), stackLast(stack + depth - 1),
      stackAt(stack), path(pathLength), value(valueLength) {
  stackAt->start = path.getLocal();
}
} // namespace speakerman
