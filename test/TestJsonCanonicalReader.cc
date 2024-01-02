//
// Created by michel on 05-02-22.
//
#include "boost-unit-tests.h"
#include <org-simple/text/StringStream.h>
#include <speakerman/JsonCanonicalReader.h>

using StringStream = org::simple::text::StringInputStream<char>;

struct Setter {
  std::string path;
  std::string value;

  Setter(const std::string p, const std::string v) : path(p), value(v) {}

  bool operator==(const Setter &s) const {
    return this == &s || (path == s.path && value == s.value);
  }
};

std::ostream &operator<<(std::ostream &out, const Setter &setter) {
  out << "{" << setter.path << " = " << setter.value << "}";
  return out;
}

std::ostream &operator<<(std::ostream &out, const std::vector<Setter> &list) {
  out << "\t{" << std::endl;
  for (const Setter &setter : list) {
    out << "\t\t" << setter << std::endl;
  }
  out << "\t}" << std::endl;
  return out;
}

class CanonicalReader : speakerman::JsonCanonicalReader {
  std::vector<Setter> actualOutput;

public:
  CanonicalReader() : speakerman::JsonCanonicalReader(128, 128, 10) {}

  const std::vector<Setter> &run(const std::string &input) {
    actualOutput.clear();
    org::simple::text::StringInputStream<char> stream(input);
    org::simple::text::TextFilePositionData<char> position;
    this->readJson(stream, position);
    return actualOutput;
  }

  void setString(const char *path, const char *string) final {
    std::string key = "String ";
    key += path;
    actualOutput.push_back({key, string});
  }

  void setNumber(const char *path, const char *string) final {
    std::string key = "Number ";
    key += path;
    actualOutput.push_back({key, string});
  }

  void setBoolean(const char *path, bool value) final {
    std::string key = "Boolean ";
    key += path;
    actualOutput.push_back({key, (value ? "true" : "false")});
  }

  void setNull(const char *path) final {
    std::string key = "Null ";
    key += path;
    actualOutput.push_back({key, ""});
  }

  const std::vector<Setter> &getVector() const { return actualOutput; }
};

class Scenario {
  std::string input;
  std::vector<Setter> expectedOutput;

public:
  Scenario(std::string string, std::initializer_list<Setter> expected)
      : input(string), expectedOutput(expected) {}

  void writeTo(std::ostream &out) const {
    out << "Actual{" << std::endl;
    out << "\t" << input << std::endl;
    out << " =>" << std::endl;
    out << expectedOutput;
    out << '}';
  }

  const std::string getInput() const { return input; }

  void test() const {
    CanonicalReader reader;
    const auto &actual = reader.run(input);
    BOOST_CHECK_EQUAL(expectedOutput.size(), actual.size());
    for (size_t i = 0; i < expectedOutput.size(); i++) {
      BOOST_CHECK_EQUAL(expectedOutput.at(i), actual.at(i));
    }
  }
};

std::ostream &operator<<(std::ostream &out, const Scenario &reader) {
  reader.writeTo(out);
  return out;
}

std::vector<Scenario> generateScenerios() {
  std::vector<Scenario> result;

  result.push_back({R"(
      {
        "name1" : true,
        "name2" : 13
      })",
                    {{"Boolean name1", "true"}, {"Number name2", "13"}}});

  result.push_back(
      {R"(
      {
        "name1" : [
          {"name2" : 13},
          {"name3" : 14 }
        ]
      })",
       {{"Number name1/0/name2", "13"}, {"Number name1/1/name3", "14"}}});

  result.push_back(
      {R"(
      {
        "name1" : [
          {"name2" : 13},
          {"name2" : 14 }
        ]
      })",
       {{"Number name1/0/name2", "13"}, {"Number name1/1/name2", "14"}}});

  result.push_back(
      {R"(
        {
          "name1" : [
            {"name2" : null},
            {"name2" : 16 }
          ]
        })",
       {{"Null name1/0/name2", ""}, {"Number name1/1/name2", "16"}}});

  result.push_back({
      R"(
      {
        "name1" : {
          "name2" : null,
          "name2" : 16
        }
      })",
      {{"Null name1/name2", ""}, {"Number name1/name2", "16"}}});

  return result;
}

BOOST_AUTO_TEST_SUITE(test_speakerman_TestJsonCanonicalReader)

BOOST_DATA_TEST_CASE(testBasicScenarios, generateScenerios()) { sample.test(); }

BOOST_AUTO_TEST_SUITE_END()
