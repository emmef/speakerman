#ifndef ORG_SIMPLE_TEST_HELPER_H
#define ORG_SIMPLE_TEST_HELPER_H
/*
 * simple-dsp/test-helper.h
 *
 * Added by michel on 2019-08-18
 * Copyright (C) 2015-2020 Michel Fleur.
 * Source https://github.com/emmef/simple-dsp
 * Email simple-dsp@emmef.org
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

#include "boost-unit-tests.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <typeinfo>

namespace org::simple::test {


namespace {
class AbstractValueTestCase {
protected:
  static std::stringstream &log() {
    static thread_local std::stringstream *log_ptr = nullptr;
    if (!log_ptr) {
      log_ptr = new std::stringstream();
    }
    log_ptr->str("");
    return *log_ptr;
  }

public:
  virtual void test() const = 0;

  virtual std::ostream &print(std::ostream &stream) const = 0;

  virtual ~AbstractValueTestCase() = default;
};

class AbstractSimpleTestCase {
protected:
public:
  virtual void test() noexcept = 0;

  ~AbstractSimpleTestCase() = default;
};

template <typename Value> class SimpleTestCase : public AbstractValueTestCase {
  Value expectedValue;
  bool throws;

public:
  virtual Value actualValue() const = 0;

  explicit SimpleTestCase(Value expected)
      : expectedValue(expected), throws(false) {}

  SimpleTestCase() : throws(true) {}

  void test() const final {
    std::stringstream &out = log();
    if (throws) {
      try {
        Value actual = actualValue();
        // Expected an Result thrown
        print(out);
        out << " expected exception, but instead got value " << actual << ".";
        BOOST_TEST_FAIL(out.str());
        return;
      } catch (const std::exception &e) {
        return;
      } catch (...) {
        print(out);
        out << " expected exception, but instead something else was thrown.";
        BOOST_TEST_FAIL(out.str());
        throw;
      }
    } else {
      try {
        Value actual = actualValue();
        if (actual != expectedValue) {
          print(out);
          out << " expected " << expectedValue << ", but instead got "
              << actual;
          BOOST_TEST_FAIL(out.str());
        }
        return;
      } catch (const std::exception &e) {
        print(out);
        out << " expected value " << expectedValue
            << ", but instead got thrown " << typeid(e).name() << " saying "
            << e.what();
        BOOST_TEST_FAIL(out.str());
      } catch (...) {
        print(out);
        out << " expected value " << expectedValue
            << ", but instead something "
               "was thrown";
        BOOST_TEST_FAIL(out.str());
      }
    }
  }
};

template <typename Result, typename V1>
class OneArgumentFunctionTestCase : public SimpleTestCase<Result> {
  std::string name;
  Result (*fn)(V1);
  V1 arg1;

public:
  OneArgumentFunctionTestCase(const std::string functionName,
                              Result (*function)(V1), Result expectedValue,
                              V1 argument)
      : SimpleTestCase<Result>(expectedValue), name(functionName), fn(function),
        arg1(argument) {}

  OneArgumentFunctionTestCase(const std::string functionName,
                              Result (*function)(V1), V1 argument)
      : SimpleTestCase<Result>(), name(functionName), fn(function),
        arg1(argument) {}

  Result actualValue() const override { return fn(arg1); }

  std::ostream &print(std::ostream &stream) const override {
    stream << name << "(" << arg1 << ")";
    return stream;
  }
};

template <typename Result, typename V1, typename V2>
class TwoArgumentFunctionTestCase : public SimpleTestCase<Result> {
  std::string name;
  Result (*fn)(V1, V2);
  V1 arg1;
  V2 arg2;

public:
  TwoArgumentFunctionTestCase(const std::string functionName,
                              Result (*function)(V1, V2), Result expectedValue,
                              V1 argument1, V2 argument2)
      : SimpleTestCase<Result>(expectedValue), name(functionName), fn(function),
        arg1(argument1), arg2(argument2) {}

  TwoArgumentFunctionTestCase(const std::string functionName,
                              Result (*function)(V1, V2), V1 argument1,
                              V2 argument2)
      : SimpleTestCase<Result>(), name(functionName), fn(function),
        arg1(argument1), arg2(argument2) {}

  Result actualValue() const override { return fn(arg1, arg2); }

  std::ostream &print(std::ostream &stream) const override {
    stream << name << "(" << arg1 << ", " << arg2 << ")";
    return stream;
  }
};

template <typename Result, typename V1, typename V2, typename V3>
class ThreeArgumentFunctionTestCase : public SimpleTestCase<Result> {
  std::string name;
  Result (*fn)(V1, V2, V3);
  V1 arg1;
  V2 arg2;
  V3 arg3;

public:
  ThreeArgumentFunctionTestCase(const std::string functionName,
                                Result (*function)(V1, V2, V3),
                                Result expectedValue, V1 argument1,
                                V2 argument2, V3 argument3)
      : SimpleTestCase<Result>(expectedValue), name(functionName), fn(function),
        arg1(argument1), arg2(argument2), arg3(argument3) {}

  ThreeArgumentFunctionTestCase(const std::string functionName,
                                Result (*function)(V1, V2, V3), V1 argument1,
                                V2 argument2, V3 argument3)
      : SimpleTestCase<Result>(), name(functionName), fn(function),
        arg1(argument1), arg2(argument2), arg3(argument3) {}

  Result actualValue() const override { return fn(arg1, arg2, arg3); }

  std::ostream &print(std::ostream &stream) const override {
    stream << name << "(" << arg1 << ", " << arg2 << ")";
    return stream;
  }
};

struct FunctionTestCases {

  template <typename Result, typename V1>
  static AbstractValueTestCase *create(const std::string &nm, Result (*fn)(V1),
                                       Result expected, V1 v1) {
    return new OneArgumentFunctionTestCase<Result, V1>(nm, fn, expected, v1);
  }

  template <typename Result, typename V1>
  static AbstractValueTestCase *create(const std::string &nm, Result (*fn)(V1),
                                       V1 v1) {
    return new OneArgumentFunctionTestCase<Result, V1>(nm, fn, v1);
  }

  template <typename Result, typename V1, typename V2>
  static AbstractValueTestCase *create(const std::string &nm,
                                       Result (*fn)(V1, V2), Result expected,
                                       V1 v1, V2 v2) {
    return new TwoArgumentFunctionTestCase<Result, V1, V2>(nm, fn, expected, v1,
                                                           v2);
  }

  template <typename Result, typename V1, typename V2>
  static AbstractValueTestCase *create(const std::string &nm,
                                       Result (*fn)(V1, V2), V1 v1, V2 v2) {
    return new TwoArgumentFunctionTestCase<Result, V1, V2>(nm, fn, v1, v2);
  }

  template <typename Result, typename V1, typename V2, typename V3>
  static AbstractValueTestCase *create(const std::string &nm,
                                       Result (*fn)(V1, V2, V3),
                                       Result expected, V1 v1, V2 v2, V3 v3) {
    return new ThreeArgumentFunctionTestCase<Result, V1, V2, V3>(
        nm, fn, expected, v1, v2, v3);
  }

  template <typename Result, typename V1, typename V2, typename V3>
  static AbstractValueTestCase *
  create(const std::string &nm, Result (*fn)(V1, V2, V3), V1 v1, V2 v2, V3 v3) {
    return new ThreeArgumentFunctionTestCase<Result, V1, V2, V3>(nm, fn, v1, v2,
                                                                 v3);
  }
};

template <typename T, typename A, class TestInterface>
class CompareWithReferenceTestCase : public AbstractValueTestCase {
  const TestInterface &expectedValues;
  const TestInterface &actualValues;
  const size_t count;
  const std::array<A, 3> arguments;

  [[nodiscard]] bool effectiveValue(T &result,
                                    const TestInterface &values) const {
    try {
      result = generateValue(values);
      return false;
    } catch (const std::exception &) {
      return true;
    }
  }

  void printValue(std::ostream &stream, const T &value, bool thrown) const {
    if (thrown) {
      stream << "std::exception";
    } else {
      stream << value;
    }
  }

protected:
  A getArgument(size_t i) const { return arguments.at(i); }

public:
  CompareWithReferenceTestCase(const TestInterface &expected,
                               const TestInterface &actual)
      : expectedValues(expected), actualValues(actual), count(1) {}

  CompareWithReferenceTestCase(const TestInterface &expected,
                               const TestInterface &actual, A value)
      : expectedValues(expected), actualValues(actual), count(1),
        arguments({value, value, value}) {}

  CompareWithReferenceTestCase(const TestInterface &expected,
                               const TestInterface &actual, A v1, A v2)
      : expectedValues(expected), actualValues(actual), count(2),
        arguments({v1, v2, v1}) {}

  CompareWithReferenceTestCase(const TestInterface &expected,
                               const TestInterface &actual, A v1, A v2, A v3)
      : expectedValues(expected), actualValues(actual), count(3),
        arguments({v1, v2, v3}) {}

  [[nodiscard]] virtual const char *methodName() const = 0;

  [[nodiscard]] virtual const char *typeOfTestName() const = 0;

  [[nodiscard]] virtual const char *getArgumentName(size_t) const {
    return nullptr;
  }

  [[nodiscard]] virtual T generateValue(const TestInterface &) const = 0;

  void test() const override {
    T expected;
    T actual;
    bool expectedThrown = effectiveValue(expected, expectedValues);
    bool actualThrown = effectiveValue(actual, actualValues);

    bool result =
        isCorrectResult(expected, expectedThrown, actual, actualThrown);
    if (!result) {
      expectedThrown = effectiveValue(expected, expectedValues);
      actualThrown = effectiveValue(actual, actualValues);
      isCorrectResult(expected, expectedThrown, actual, actualThrown);
    }
    BOOST_CHECK_MESSAGE(result, *this);
  }

  std::ostream &print(std::ostream &output) const override {
    T expected;
    T actual;
    bool expectedThrown = effectiveValue(expected, expectedValues);
    bool actualThrown = effectiveValue(actual, actualValues);

    output << typeOfTestName() << "::" << methodName() << "(";
    for (size_t i = 0; i < count; i++) {
      if (i > 0) {
        output << ", ";
      }
      output << getArgumentNameOrDefault(i) << "=" << arguments.at(i);
    }
    output << ")";

    if (isCorrectResult(expected, expectedThrown, actual, actualThrown)) {
      output << ": correct result(";
      printValue(output, expected, expectedThrown);
      return output << ")";
    }
    output << ": expected(";
    printValue(output, expected, expectedThrown);
    output << ") got (";
    printValue(output, actual, actualThrown);
    return output << ")";
  }

  [[nodiscard]] const char *getArgumentNameOrDefault(size_t i) const {
    const char *string = getArgumentName(i);
    if (string) {
      return string;
    }
    if (i >= count) {
      return "undefined";
    }
    if (count == 1) {
      return "value";
    }
    switch (i) {
    case 0:
      return "v1";
    case 1:
      return "v2";
    case 2:
      return "v3";
    default:
      return "<unknown>";
    }
  }

  bool isCorrectResult(T expected, bool expectedThrown, T actual,
                       bool actualThrown) const {
    return expectedThrown ? actualThrown : !actualThrown && expected == actual;
  };
};

struct AbstractFunctionTestScenario {
  [[nodiscard]] virtual bool success() const = 0;
  virtual void print(std::ostream &out) const = 0;
  [[nodiscard]] virtual AbstractFunctionTestScenario *clone() const = 0;
  virtual ~AbstractFunctionTestScenario() = default;
};

template <typename R, typename V>
struct FunctionTestScenarioImplementation
    : public AbstractFunctionTestScenario {
  V input_;
  R expected_;
  R (*function_)(V);
  const char *name_;

  FunctionTestScenarioImplementation(V input, R expected, R (*function)(V),
                                     const char *name)
      : input_(input), expected_(expected), function_(function), name_(name) {}

  [[nodiscard]] bool success() const override {
    return expected_ == function_(input_);
  }

  [[nodiscard]] AbstractFunctionTestScenario *clone() const override {
    return new FunctionTestScenarioImplementation<R, V>(input_, expected_,
                                                        function_, name_);
  }

  void print(std::ostream &out) const override {
    out << name_ << "(" << input_ << ")";
    if (success()) {
      out << " = " << expected_;
    } else {
      out << " = " << function_(input_) << ", but expected " << expected_;
    }
  }
};

class FunctionTestScenario {
  const AbstractFunctionTestScenario *scenario_;

public:
  FunctionTestScenario() : scenario_(nullptr){};
  explicit FunctionTestScenario(const AbstractFunctionTestScenario *s)
      : scenario_(s) {}
  FunctionTestScenario(const FunctionTestScenario &source)
      : scenario_(source.scenario_->clone()) {}

  FunctionTestScenario(FunctionTestScenario &&moved) noexcept
      : scenario_(moved.scenario_) {
    moved.scenario_ = nullptr;
  }
  FunctionTestScenario &operator=(const FunctionTestScenario &source) {
    if (&source != this) {
      scenario_ = source.scenario_->clone();
    }
    return *this;
  }
  [[nodiscard]] bool success() const { return scenario_->success(); }
  void print(std::ostream &out) const { scenario_->print(out); }

  ~FunctionTestScenario() {
    delete scenario_;
    scenario_ = nullptr;
  }

  template <typename R, typename V>
  static FunctionTestScenario create(V input, R expected, R (*function)(V),
                                     const char *name) {
    return FunctionTestScenario(new FunctionTestScenarioImplementation<R, V>(
        input, expected, function, name));
  }
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

std::ostream &operator<<(std::ostream &stream, const AbstractValueTestCase &s) {
  s.print(stream);
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const FunctionTestScenario &s) {
  s.print(stream);
  return stream;
}

#pragma clang diagnostic pop

} // namespace



} // namespace org::simple::test

#endif // ORG_SIMPLE_TEST_HELPER_H
