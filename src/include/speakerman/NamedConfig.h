#ifndef SPEAKERMAN_NAMEDCONFIG_H
#define SPEAKERMAN_NAMEDCONFIG_H
/*
 * speakerman/ConfigNames.h
 *
 * Added by michel on 2022-01-09
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

#include <cstddef>
#include <cstdarg>

namespace speakerman {

struct NamedConfig {
  static constexpr size_t NAME_LENGTH = 31;
  static constexpr size_t NAME_CAPACITY = NAME_LENGTH + 1;

  char name[NAME_CAPACITY];

  /**
   * Copies the source string to the name as destination. Returns \c true if the
   * string was completely copied and \c false if it was too long or one of the
   * arguments was \c nullptr. A truncated string yields a valid result but will
   * return \c false.
   *
   * @param destination The name that the string should be copied to.
   * @param source The source string to be copied.
   * @return Whether the arguments are valid and \c source is fully copied to
   * the destination.
   */
  bool copyToName(const char *source);

  /**
   * Prints a format and its arguments to the destination name. Returns \c true
   * if the string was completely copied without formatting errors,  and \c
   * false if it was too long or one of the arguments was \c nullptr. A
   * truncated string yields a valid result but will return \c false.
   * @param destination The name that the string should be copied to.
   * @param source The source string to be copied.
   * @param arguments Arguments for the format
   * @return Whether the arguments are valid and a valid formatted string was
   * fully generated into the destination.
   */
  bool vPrintToName(const char *format, va_list arguments);

  /**
   * Prints a format and its arguments to the destination name. Returns \c true
   * if the string was completely copied without formatting errors,  and \c
   * false if it was too long or one of the arguments was \c nullptr. A
   * truncated string yields a valid result but will return \c false.
   * @param destination The name that the string should be copied to.
   * @param source The source string to be copied.
   * @param ... Arguments for the format
   * @return Whether the arguments are valid and a valid formatted string was
   * fully generated into the destination.
   */
  bool printToName(const char *format, ...);
};

} // namespace speakerman

#endif // SPEAKERMAN_NAMEDCONFIG_H
