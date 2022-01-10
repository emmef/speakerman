#ifndef SPEAKERMAN_UNSETVALUE_H
#define SPEAKERMAN_UNSETVALUE_H
/*
 * speakerman/UnsetValue.h
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

#include <speakerman/NamedConfig.h>
#include <limits>
#include <algorithm>

namespace speakerman {

template <typename T> struct UnsetValue {};

template <> struct UnsetValue<size_t> {
  static constexpr size_t value = static_cast<size_t>(-1);

  static bool is(size_t test) { return test == value; }

  static void set(size_t &destination) { destination = value; }
};

template <> struct UnsetValue<char *> {
  static constexpr char *value = {0};

  static bool is(const char *test) { return !test || !*test; }

  static void set(char *destination) {
    if (destination) {
      *destination = 0;
    }
  }
};

template <> struct UnsetValue<char[NamedConfig::NAME_CAPACITY]> {
  static constexpr char *value = {0};

  static bool is(const char *test) { return !test || !*test; }

  static void set(char *destination) {
    if (destination) {
      *destination = 0;
    }
  }
};

template <> struct UnsetValue<double> {
  static constexpr double value = std::numeric_limits<double>::quiet_NaN();

  union Tester {
    long long l;
    double f;

    Tester(double v) : l(0) { f = v; }

    bool operator==(const Tester &other) const { return l == other.l; }
  };

  static bool is(double test) {
    static const Tester sNan = {std::numeric_limits<double>::signaling_NaN()};
    static const Tester qNan = {value};
    Tester t{test};
    return t == sNan || t == qNan;
  }

  static void set(double &destination) { destination = value; }
};

template <> struct UnsetValue<int> {
  static constexpr int value = -1;

  static bool is(int test) { return test == value; }

  static void set(int &destination) { destination = value; }
};

template <typename T> static void unsetConfigValue(T &target) {
  UnsetValue<T>::set(target);
}

template <typename T> static bool setConfigValueIfUnset(T &target, T copyFrom) {
  if (UnsetValue<T>::is(target)) {
    target = copyFrom;
    return true;
  }
  return false;
}

template <typename T> static bool isUnsetConfigValue(const T &value) {
  return UnsetValue<T>::is(value);
}

template <typename T>
static bool fixedValueIfUnsetOrOutOfRange(T &value, T value_if_unset,
                                                 T minimum, T maximum) {
  if (UnsetValue<T>::is(value) || value < minimum || value > maximum) {
    value = value_if_unset;
    return true;
  }
  return false;
}

template <typename T>
static bool unsetIfInvalid(T &value, T minimum, T maximum) {
  if (value < minimum || value > maximum) {
    value = UnsetValue<T>::value;
    return true;
  }
  return false;
}

template <typename T>
static void boxIfSetAndOutOfRange(T &value, T minimum, T maximum) {
  if (UnsetValue<T>::is(value)) {
    return;
  }
  if (value < minimum) {
    value = minimum;
  } else if (value > maximum) {
    value = maximum;
  }
}

template <typename T>
static void fixedValueIfUnsetOrBoxedIfOutOfRange(T &value, T value_if_unset, T minimum,
                                T maximum) {
  if (UnsetValue<T>::is(value)) {
    value = value_if_unset;
  } else if (value < minimum) {
    value = minimum;
  } else if (value > maximum) {
    value = maximum;
  }
}

template <typename T>
static bool setDefaultOrBoxedFromSourceIfUnset(T &value, T defaultValue, T sourceValue, T minimum, T maximum) {
  if (!isUnsetConfigValue(value)) {
    return true;
  }
  if (isUnsetConfigValue(sourceValue)) {
    if (isUnsetConfigValue(defaultValue)) {
      return false;
    }
    value = defaultValue;
  }
  else {
    value = std::clamp(sourceValue, minimum, maximum);
  }
  return true;
}

template <typename T>
static bool setDefaultOrFromSourceIfUnset(T &value, T defaultValue, T sourceValue) {
  if (!isUnsetConfigValue(value)) {
    return true;
  }
  if (isUnsetConfigValue(sourceValue)) {
    if (isUnsetConfigValue(defaultValue)) {
      return false;
    }
    value = defaultValue;
  }
  else {
    value = sourceValue;
  }
  return true;
}

template <typename T>
static bool setBoxedFromSetSource(T &value, T sourceValue, T minimum, T maximum) {
  if (isUnsetConfigValue(sourceValue)) {
    return false;
  }
  value = std::clamp(sourceValue, minimum, maximum);
  return true;
}

template <typename T>
static bool setFromSetSource(T &value, T sourceValue) {
  if (isUnsetConfigValue(sourceValue)) {
    return false;
  }
  value = sourceValue;
  return true;
}

} // namespace speakerman

#endif // SPEAKERMAN_UNSETVALUE_H
