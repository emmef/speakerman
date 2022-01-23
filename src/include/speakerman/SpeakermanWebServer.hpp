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
#include <speakerman/HttpMessage.hpp>
#include <speakerman/ServerSocket.hpp>
#include <speakerman/SingleThreadFileCache.hpp>
#include <speakerman/DynamicProcessorLevels.h>
#include <speakerman/SpeakerManagerControl.h>
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

class web_server : protected http_message {
public:
  using Result = server_worker_result;
  using State = server_socket_state;
  using Stream = server_socket::Stream;
  static constexpr size_t URL_LENGTH = 1023;
  static constexpr const char *COOKIE_TIME_STAMP = "levelTimeStamp";
  static constexpr size_t COOKIE_TIME_STAMP_LENGTH =
      tdap::constexpr_string_length(COOKIE_TIME_STAMP);

  web_server(SpeakerManagerControl &speakerManager);

  bool open(const char *service, int timeoutSeconds, int backLog,
            int *errorCode);

  const char *service() const { return socket_.service(); }

  State state() const { return socket_.state(); }

  bool isOpen() const { return state() != State::CLOSED; }

  bool isWorking() const { return state() == State::WORKING; }

  bool work(int *errorCode);

  void close();

  ~web_server() {
    cout << "Closing web server" << endl;
    close();
  }

protected:
  virtual bool content_stream_delete() const override { return false; }

  virtual const char *on_url(const char *url) override;

  virtual void on_header(const char *header, const char *value) override;

  virtual void handle_request(input_stream *pStream) override;

  virtual const char *on_method(const char *method_name) override;

private:
  enum class Method { GET, PUT };

  SpeakerManagerControl &manager_;
  file_entry indexHtmlFile;
  file_entry cssFile;
  file_entry javaScriptFile;
  file_entry faviconFile;
  server_socket socket_;
  LevelEntryBuffer level_buffer;
  char url_[URL_LENGTH + 1];
  std::thread level_fetch_thread;
  long long levelTimeStamp = 0;
  Method method = Method::GET;
  SpeakermanConfig configFileConfig;

  static void thread_static_function(web_server *);

  void thread_function();

  Result accept_work(Stream &stream, const server_socket &socket);

  static Result worker_function(Stream &stream, const server_socket &socket,
                                void *data);
  void handleConfigurationChanges(char *configurationJson);
  void writeInputVolumes();
};

} // namespace speakerman

#endif // SPEAKERMAN_M_SPEAKERMAN_WEB_SERVER_HPP
