//
// Created by michel on 09-01-22.
//

#include <speakerman/NamedConfig.h>
#include <cstdio>

namespace speakerman {

bool NamedConfig::copyToName(const char *source) {
  if (!source) {
    return false;
  }
  const char *s = source;
  char *d = name;
  size_t i;
  for (i = 0; i < NAME_LENGTH && *s; i++) {
    *d++ = *s++;
  }
  *d = '\0';
  return *s == '\0';
}
bool NamedConfig::vPrintToName(const char *format, va_list arguments) {
  int print = vsnprintf(name, NAME_LENGTH, format, arguments);
  name[NAME_LENGTH] = '\0';
  return print >= 0 && size_t(print) <= NAME_LENGTH;
}
bool NamedConfig::printToName(const char *format, ...) {
  va_list args;
  va_start(args, format);
  bool result = vPrintToName(format, args);
  va_end(args);
  return result;
}
} // namespace speakerman
