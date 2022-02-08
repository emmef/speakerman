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
#include <org-simple/util/text/Json.h>
#include <speakerman/DynamicProcessorLevels.h>
#include <speakerman/SpeakerManagerControl.h>
#include <speakerman/Webserver.h>
#include <tdap/Count.hpp>
#include <tdap/Power2.hpp>
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

  ~web_server() { cout << "Closing web server" << endl; }

protected:
  HttpResultHandleResult handle(mg_connection *connection,
                                mg_http_message *httpMessage) override;

private:
  class Response {
    static constexpr size_t LENGTH = 30;
    static constexpr const char *const NEWLINE = "\r\n";
    char numberPad[LENGTH + 1] = {0};
    std::string body;
    std::string headers;
    std::string response;
    std::string contentType;

    template <typename V>
    requires(std::is_integral_v<V> || std::is_floating_point_v<V>) //
        void write_number(std::string &string, V number) {
      if constexpr (std::is_integral_v<V>) {
        signed long long v = number;
        snprintf(numberPad, LENGTH, "%lld", v);
      } else if constexpr (std::is_floating_point_v<V>) {
        double v = number;
        snprintf(numberPad, LENGTH, "%lf", v);
      }
      numberPad[LENGTH] = 0;
      string += numberPad;
    }

    template <typename V>
    requires(std::is_integral_v<V> || std::is_floating_point_v<V>) //
        void addHeader(std::string &headers, const char *name,
                              V value, const char *extra) {
      headers += name;
      headers += ": ";
      write_number(headers, value);
      headers += value;
      if (extra) {
        headers += "; ";
        headers += extra;
      }
      headers += NEWLINE;

    }
    static void addHeader(std::string &headers, const char *name,
                          const char *value, const char *extra) {
      headers += name;
      headers += ": ";
      headers += value;
      if (extra) {
        headers += "; ";
        headers += extra;
      }
      headers += NEWLINE;
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

    template <typename V>
    requires(std::is_integral_v<V> || std::is_floating_point_v<V>) //
    void addHeader(const char *name, V value,
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
      snprintf(numberPad, 24, "%zu", body.length());
      addHeader("Content-Length", numberPad);

      mg_http_reply(connection, code, headers.c_str(), body.c_str());
    }

    void write_string(const char *str) { body += str; }

    template <typename V>
    requires(std::is_integral_v<V> || std::is_floating_point_v<V>) //
        void write_number(V number) {
      write_number(body, number);
    }

    void write_json_string(const char *string) {
      for (const char *p = string; *p != 0; p++) {
        org::simple::util::text::addCharacterToJsonString(*p, [this](char c) {
          body += c;
          return true;
        });
      }
    }

    void write(char c) { body += c; }

    template <typename V>
    void addCookie(const char *const name, V value,
                   const char *extra) {
      headers += "Set-Cookie";
      headers += ": ";
      headers += name;
      headers += '=';
      if constexpr (std::is_same_v<V,const char *>) {
        headers += value;
      }
      else {
        write_number(headers, value);
      }
      if (extra) {
        headers += "; ";
        headers += extra;
      }
      headers += NEWLINE;
    }
  };

  class Json {
    Response *response;
    char scopeEnd;
    bool first = true;

    void startValue() {
      if (first) {
        first = false;
      } else {
        response->write(',');
      }
    }
    bool addName(const char *name) {
      if (!response) {
        return false;
      }
      startValue();
      response->write('"');
      response->write_json_string(name);
      response->write_string("\":");
      return true;
    }

    Json(Json &j, char scope, char endScope)
        : response(j.response), scopeEnd(endScope), first(true) {
      response->write(scope);
    }
    Json() : response(nullptr), scopeEnd('}') { response->write('{'); }

  public:
    Json(Response &r) : response(&r), scopeEnd('}') { response->write('{'); }

    Json(Json &&j)
        : response(j.response), scopeEnd(j.scopeEnd), first(j.first) {
      j.response = nullptr;
    }

    void setString(const char *name, const char *value) {
      if (addName(name)) {
        response->write('"');
        response->write_json_string(value);
        response->write('"');
      }
    }

    template <typename V>
    requires(std::is_integral_v<V> || std::is_floating_point_v<V>) //
        void setNumber(const char *name, V value) {
      if (addName(name)) {
        response->write_number(value);
      }
    }

    void setBoolean(const char *name, bool value) {
      if (addName(name)) {
        response->write_string(value ? "true" : "false");
      }
    }

    void setNull(const char *name) {
      if (addName(name)) {
        response->write_string("null");
      }
    }

    Json addObject(const char *name) {
      if (addName(name)) {
        return Json(*this, '{', '}');
      }
      return Json();
    }

    Json addArrayObject() {
      startValue();
      return Json(*this, '{', '}');
    }

    Json addArray(const char *name) {
      if (addName(name)) {
        return Json(*this, '[', ']');
      }
      return Json();
    }

    ~Json() {
      if (response) {
        response->write(scopeEnd);
      }
    }
  };

  void handleTimeStampCookie(const char *header, const char *value);
  static void thread_static_function(web_server *);
  void thread_function();
  void handleConfigurationChanges(mg_connection *connection,
                                  const char *configurationJson);
  void writeInputVolumes(Json &json);
  bool applyConfigAndGetLevels(DynamicProcessorLevels &levels,
                               milliseconds &wait);

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
