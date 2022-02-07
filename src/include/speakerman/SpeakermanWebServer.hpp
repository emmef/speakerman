#ifndef SPEAKERMAN_M_SPEAKERMAN_WEB_SERVER_HPP
#define SPEAKERMAN_M_SPEAKERMAN_WEB_SERVER_HPP
/*
 * speakerman/SpeakermanWebServer.hpp
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

#include <chrono>
#include <mutex>
#include <speakerman/Webserver.h>
#include <speakerman/DynamicProcessorLevels.h>
#include <speakerman/SpeakerManagerControl.h>
#include <tdap/Count.hpp>
#include <tdap/Power2.hpp>
#include <org-simple/util/text/Json.h>
#include <thread>

namespace speakerman {
using namespace std;
using namespace std::chrono;

static long long current_millis() {
  return system_clock::now().time_since_epoch().count() / 1000000;
}

struct LevelEntry {
  DynamicProcessorLevels levels;
  bool set;
  long long stamp;

  LevelEntry() : set(false) {}

  LevelEntry(DynamicProcessorLevels lvl)
      : levels(lvl), set(true), stamp(current_millis()) {}
};

class LevelEntryBuffer {
  mutex m;
  static constexpr size_t SIZE = 128;
  static constexpr size_t MASK = SIZE - 1;
  LevelEntry entries[SIZE];
  size_t wr_;

  static size_t prev(size_t n) { return (n + SIZE - 1) & MASK; }

  static size_t next(size_t n) { return (n + 1) & MASK; }

public:
  void put(const DynamicProcessorLevels &levels) {
    unique_lock<mutex> lock(m);
    wr_ = prev(wr_);
    entries[wr_] = LevelEntry(levels);
  }

  void get(long long lastChecked, LevelEntry &target) {
    unique_lock<mutex> lock(m);
    target = entries[wr_];
    if (lastChecked <= 0) {
      return;
    }
    size_t read = wr_;
    read = next(read);
    LevelEntry entry = entries[read];
    while (read != wr_ && entry.set && entry.stamp > lastChecked) {
      target.levels += entry.levels;
      read = next(read);
      entry = entries[read];
    }
  }
};

class web_server : public WebServer {
public:
  static constexpr const char *COOKIE_TIME_STAMP = "levelTimeStamp";
  static constexpr size_t COOKIE_TIME_STAMP_LENGTH =
      tdap::constexpr_string_length(COOKIE_TIME_STAMP);

  web_server(SpeakerManagerControl &speakerManager);

  ~web_server() {
    cout << "Closing web server" << endl;
  }

protected:

  HttpResultHandleResult handle(mg_connection *connection, mg_http_message *httpMessage) override;

private:
  class Response {
    std::string body;
    std::string headers;
    std::string response;
    std::string contentType;
    char contentLength[24];

    static void addHeader(std::string &headers, const char *name,
                          const char *value, const char *extra) {
      headers += name;
      headers += ": ";
      headers += value;
      if (extra) {
        headers += "; ";
        headers += extra;
      }
      headers += "\r\n";
    }

  public:
    void clear() {
      body.clear();
      headers.clear();
      response.clear();
      contentType.clear();
    }

    void addHeader(const char *name, const char *value,
                   const char *extra = nullptr) {
      addHeader(headers, name, value, extra);
    }

    void setContentType(const char *type, bool addUtf8) {
      contentType.clear();
      if (addUtf8) {
        addHeader(contentType, "Content-Type", type, "charset=UTF-8");
      } else {
        addHeader(contentType, "Content-Type", type, nullptr);
      }
    };

    void createReply(mg_connection *connection, int code = 200) {
      if (contentType.length() > 0) {
        headers += contentType;
      }
      snprintf(contentLength, 24, "%zu", body.length());
      addHeader("Content-Length", contentLength);

      mg_http_reply(connection, code, headers.c_str(), body.c_str());
    }

    void write_string(const char *str) {
      body += str;
    }

    void write_string(const std::string &str) {
      body += str;
    }

    void write_json_string(const char *string) {
      org::simple::util::text::JsonEscapeState state = {};
      for (const char *p = string; *p != 0; p++) {
        org::simple::util::text::addJsonStringCharacter(*p, state, [this](char c) {
          body += c;
          return true;
        });
      }
    }

    void write(char c) {
      body += c;
    }
  };


  void handleTimeStampCookie(const char *header, const char *value);
  static void thread_static_function(web_server *);
  void thread_function();
  void handleConfigurationChanges(mg_connection *connection,
                                  const char *configurationJson);
  void writeInputVolumes();
  bool applyConfigAndGetLevels(DynamicProcessorLevels &levels, milliseconds &wait);

  SpeakerManagerControl &manager_;
  LevelEntryBuffer level_buffer;
  std::thread level_fetch_thread;
  long long levelTimeStamp = 0;
  SpeakermanConfig configFileConfig;
  SpeakermanConfig clientFileConfig;
  SpeakermanConfig usedFileConfig;
  std::mutex handlingMutex;
  Response response;
};

} // namespace speakerman

#endif // SPEAKERMAN_M_SPEAKERMAN_WEB_SERVER_HPP
