/*
 * SpeakermanWebServer.cpp
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

#include <cstdio>
#include <cstring>
#include <iostream>
#include <speakerman/SpeakermanWebServer.hpp>
#include <speakerman/jack/SignalHandler.hpp>
#include <speakerman/utils/Config.hpp>
#include <sys/types.h>
#include <tdap/MemoryFence.hpp>
#include <unistd.h>

namespace speakerman {

static void create_command_and_file(string &rangeFile, string &command_line) {
  char number[33];
  snprintf(number, 33, "%x", rand());

  rangeFile = "/tmp/";
  rangeFile += number;
  snprintf(number, 33, "%llx", (long long int)getpid());
  rangeFile += number;
  rangeFile += ".ranges";

  const char *script = getWatchDogScript();
  if (script == nullptr) {
    command_line = "";
    return;
  }

  command_line = script;
  command_line += " ";
  command_line += rangeFile;
  command_line += " ";
  command_line += configFileName();
}

struct TemporaryFile {
  ifstream file_;
  string name_;

  TemporaryFile(const char *name) : name_(name) { file_.open(name_); }

  bool is_open() const { return file_.is_open(); }

  const char *name() const { return name_.c_str(); }

  istream &stream() { return file_; }

  ~TemporaryFile() {
    if (file_.is_open()) {
      file_.close();
    }
    int result = std::remove(name_.c_str());
    if (result != 0) {
      cerr << "Could not remove " << name_ << ": " << strerror(errno) << endl;
    }
  }
};

static constexpr int SLEEP_MILLIS = 50;
static constexpr int CONFIG_NUMBER_OF_SLEEPS = 10;
static constexpr int CONFIG_MILLIS = SLEEP_MILLIS * CONFIG_NUMBER_OF_SLEEPS;
static constexpr int WAIT_MILLIS = 1000;
static constexpr int SECONDS_PER_6_DB_UP = 30;
static constexpr int SECONDS_PER_6_DB_DOWN = 180;

static void approach_threshold_scaling(double &value, int new_value) {
  static const double FACTOR_UP =
      std::pow(2.0, 0.001 * CONFIG_MILLIS / SECONDS_PER_6_DB_UP);
  static const double FACTOR_DOWN =
      std::pow(0.5, 0.001 * CONFIG_MILLIS / SECONDS_PER_6_DB_DOWN);
  if (new_value > value) {
    value *= FACTOR_UP;
    if (value > new_value) {
      value = new_value;
    }
  } else if (new_value < value) {
    value *= FACTOR_DOWN;
    if (value < new_value) {
      value = new_value;
    }
  }
}

void web_server::thread_static_function(web_server *server) {
  jack::CountedThreadGuard guard("Web server configuration updater");

  try {
    server->thread_function();
  } catch (const jack::signal_exception &e) {
    e.handle("Web server configuration update and level fetching");
  }
}

void web_server::thread_function() {
  static std::chrono::milliseconds wait(WAIT_MILLIS);
  static std::chrono::milliseconds sleep(SLEEP_MILLIS);
  int count = 1;

  {
    tdap::MemoryFence fence;
    configFileConfig = manager_.getConfig();
  }
  DynamicProcessorLevels levels;
  string range_file;
  string command_line;
  int threshold_scaling_setting = 1;
  double threshold_scaling = threshold_scaling_setting;
  double new_threshold_scaling = threshold_scaling;

  while (!jack::SignalHandler::check_raised()) {
    count++;
    bool got_levels = false;
    if ((count % CONFIG_NUMBER_OF_SLEEPS) == 0) {
      approach_threshold_scaling(new_threshold_scaling,
                                 threshold_scaling_setting);

      bool read = false;
      auto stamp = getConfigFileTimeStamp();
      if (stamp != configFileConfig.timeStamp) {
        cout << "read config!" << std::endl;
        try {
          configFileConfig = readSpeakermanConfig(configFileConfig, true);
          const char * comment = configFileConfig.timeStamp != 0 ? "Configuration file was updated" : "Reset and re-read configuration request";
          dumpSpeakermanConfig(configFileConfig, std::cout, comment);
          read = true;
        } catch (const runtime_error &e) {
          cerr << "Error reading configuration: " << e.what() << endl;
          configFileConfig.timeStamp = stamp;
        }
      }
      if (new_threshold_scaling != threshold_scaling) {
        threshold_scaling = new_threshold_scaling;
        configFileConfig.threshold_scaling = threshold_scaling;
        read = true;
      }
      if (read) {
        got_levels = applyConfigAndGetLevels(levels, wait);
      }
    }
    if (!got_levels && manager_.getLevels(&levels, wait)) {
      level_buffer.put(levels);
    }
    if (count == 100) {
      count = 0;
      create_command_and_file(range_file, command_line);
      int old_setting = threshold_scaling_setting;
      threshold_scaling_setting = 1;
      if (command_line.length() == 0) {
        cerr << "Cannot find watchdog command" << endl;
      } else if (system(command_line.c_str()) == 0) {
        TemporaryFile file{range_file.c_str()};
        if (file.is_open()) {
          istream &stream = file.stream();
          while (!stream.eof()) {
            char chr = stream.get();
            if (chr >= '1' && chr <= '5') {
              threshold_scaling_setting = chr - '0';
              break;
            } else if (!utils::config::isWhiteSpace(chr)) {
              break;
            }
          }
        }
      }
      if (old_setting != threshold_scaling_setting) {
        cout << "Threshold scaling set from " << old_setting << " to "
             << threshold_scaling_setting << endl;
      }
    }
    this_thread::sleep_for(sleep);
  }
}
bool web_server::applyConfigAndGetLevels(DynamicProcessorLevels &levels,
                                         milliseconds &wait) {
  if (manager_.applyConfigAndGetLevels(configFileConfig, &levels, wait)) {
    level_buffer.put(levels);
    return true;
  }
  return false;
}

web_server::web_server(SpeakerManagerControl &speakerManager)
    : WebServer(getWebSiteDirectory()), manager_(speakerManager) {
  thread t(thread_static_function, this);
  level_fetch_thread.swap(t);
  level_fetch_thread.detach();
}

// TODO rename to handleCookieTimestamp
void web_server::handleTimeStampCookie(const char *header, const char *value) {
  static constexpr int ASSIGN = 1;
  static constexpr int VALUE = 2;
  static constexpr int NUM = 3;
  static constexpr int DONE = 4;
  unsigned long long number = 0;
  unsigned long long previousNumber = 0;
  if (strcasecmp("cookie", header) == 0) {
    const char *pos = strstr(value, COOKIE_TIME_STAMP);
    if (pos) {
      pos += COOKIE_TIME_STAMP_LENGTH;
      int status = ASSIGN;
      char c;
      while (status != DONE && (c = *pos++) != 0) {
        switch (status) {
        case ASSIGN:
          if (c == '=') {
            status = VALUE;
          } else if (c != ' ') {
            return;
          }
          break;
        case VALUE:
          if (c == ' ') {
            continue;
          }
          if (c == ';') {
            return;
          }
          if (c >= '0' && c <= '9') {
            number = c - '0';
            status = NUM;
          } else {
            return;
          }
          break;
        case NUM:
          if (c >= '0' && c <= '9') {
            previousNumber = number;
            number *= 10;
            number += c - '0';
            if (number < previousNumber) {
              status = DONE;
            }
          } else if (c == ';') {
            status = DONE;
          }
          break;
        }
      }
      levelTimeStamp = number;
    }
  }
}

static bool matches(const mg_str &string1, const char *string2) {
  return strncmp(string2, string1.ptr, string1.len) == 0;
}

static bool matchesCI(const mg_str &string1, const char *string2) {
  return strncasecmp(string2, string1.ptr, string1.len) == 0;
}

static bool operator==(const mg_str &string1, const char *string2) {
  return matches(string1, string2);
}


HttpResultHandleResult web_server::handle(mg_connection *connection,
                                          mg_http_message *httpMessage) {
  std::unique_lock<std::mutex> guard(handlingMutex);
  response.clear();

  static constexpr size_t LENGTH = 10240;
  std::unique_ptr<char> str(new char[LENGTH + 1]);

  mg_str &method = httpMessage->method;
  mg_str &uri = httpMessage->uri;
  if (matchesCI(method, "GET")) {
    if (uri == "/levels") {
      LevelEntry entry;
      level_buffer.get(levelTimeStamp, entry);
      if (entry.set) {
        mg_str *cookie = mg_http_get_header(httpMessage, "cookie");
        if (cookie) {
          handleTimeStampCookie("cookie", cookie->ptr);
        }
        DynamicProcessorLevels levels = entry.levels;
        response.addCookie(COOKIE_TIME_STAMP, entry.stamp, "SameSite=Strict");
        response.addHeader("Access-Control-Allow-Origin", "*");
        response.setContentType("application/json", true);
        {
          Json json(response);
          json.setNumber("elapsedMillis", entry.stamp - levelTimeStamp);
          json.setNumber("thresholdScale", manager_.getConfig().threshold_scaling);
          json.setNumber("subLevel", levels.getSignal(0));
          json.setNumber("periods", levels.count());
          const jack::ProcessingStatistics &statistics = manager_.getStatistics();
          json.setNumber("cpuLongTerm", statistics.getLongTermCorePercentage());
          json.setNumber("cpuShortTerm", statistics.getShortTermCorePercentage());
          {
            auto groups = json.addArray("group");
            for (size_t i = 0; i < levels.groups(); i++) {
              Json group = groups.addArrayObject();
              group.setString("group_name", manager_.getConfig().processingGroups.group[i].name);
              group.setNumber("level", levels.getSignal(i + 1));
            }
          }
          writeInputVolumes(json);
        }
        response.createReply(connection, 200);
        return HttpResultHandleResult::Ok;
      } else {
        mg_http_reply(connection, 503, NULL, "Temporarily unavailable");
        return HttpResultHandleResult::Ok;
      }
    } else if (uri == "/config") {
      response.addHeader("Access-Control-Allow-Origin", "*");
      response.setContentType("application/json", true);
      {
        Json json(response);
        writeInputVolumes(json);
      }
      response.createReply(connection, 200);
      return HttpResultHandleResult::Ok;
    }
  }
  else if (matchesCI(method, "POST") || matchesCI(method, "PUT")) {
    if (uri == "/config") {
      handleConfigurationChanges(connection, httpMessage->body.ptr);
      return HttpResultHandleResult::Ok;
    }
  }
  return HttpResultHandleResult::Default;
}

void web_server::writeInputVolumes(Json &json) {
  const LogicalInputsConfig &liConfig = manager_.getConfig().logicalInputs;
  size_t groupCount = liConfig.getGroupCount();
  Json inputs = json.addArray("logicalInput");
  for (size_t i = 0; i < groupCount; i++) {
    Json group = inputs.addArrayObject();
    group.setString("name", liConfig.group[i].name);
    group.setNumber("volume", liConfig.group[i].volume);
  }
}

void web_server::handleConfigurationChanges(mg_connection *connection,
                                            const char *configurationJson) {
  static std::chrono::milliseconds wait(WAIT_MILLIS);
  DynamicProcessorLevels levels;
  if (readConfigFromJson(configFileConfig, configurationJson, configFileConfig)) {
    applyConfigAndGetLevels(levels, wait);
    {
      Json json(response);
      writeInputVolumes(json);
    }
    response.addHeader("Access-Control-Allow-Origin", "*");
    response.setContentType("application/json", true);
    response.createReply(connection, 200);
  } else {
    mg_http_reply(connection, 400, nullptr, "Unable to parse configuration from input.");
  }
}


} // namespace speakerman
