/*
 * JackClient.hpp
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

#ifndef SMS_SPEAKERMAN_JACKCLIENT_GUARD_H_
#define SMS_SPEAKERMAN_JACKCLIENT_GUARD_H_

#include "Port.hpp"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <jack/types.h>
#include <mutex>
#include <speakerman/jack/JackProcessor.hpp>
#include <speakerman/jack/SignalHandler.hpp>
#include <string>
#include <thread>
#include <unordered_set>

namespace speakerman {

class ClientOpenRetryPolicy {
  static constexpr long minWaitMillis = 100;
  static constexpr long maxWaitMillis = 2000;
  static constexpr long maxPrintInterVal = 3600000 / maxWaitMillis;
  static constexpr double waitExponent = 1.1;
  static thread_local long suppressed_errors_;

  static void no_error_function(const char *err) {
    if (getenv("SPEAKERMAN_LOG_OPEN_CLIENT_ERRORS")) {
      std::cerr << "jack_open_client() error: " << err << std::endl;
    } else {
      suppressed_errors_++;
    }
  }

  long attempt_ = 0;
  long waitMillis_ = minWaitMillis;
  long printInterval_ = 8;
  char milliseconds[16];
  std::chrono::time_point<std::chrono::steady_clock> start =
      std::chrono::steady_clock::now();

  void write_milliseconds() {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    sprintf(milliseconds, "%12.3f", 0.001f * ms.count());
  }

public:
  ClientOpenRetryPolicy() {
    jack_set_error_function(no_error_function);
    suppressed_errors_ = 0;
  }

  ~ClientOpenRetryPolicy() { jack_set_error_function(NULL); }
  long attempt() const { return attempt_; }

  bool must_print() const { return (attempt_ % printInterval_) == 0; }

  long errors() const { return suppressed_errors_; }

  long waitMillis() const { return waitMillis_; }

  void printFailureAndWait(jack_status_t status) {
    if (must_print()) {
      write_milliseconds();
      std::cerr << milliseconds << " JackClient::create() attempt "
                << (attempt() + 1) << " failed with status " << status
                << " (sleep " << waitMillis() << "msec.";
      if (errors() > 0) {
        std::cerr << " (" << errors() << " errors suppressed)";
      }
      std::cerr << std::endl;
      if (waitMillis_ >= maxWaitMillis) {
        printInterval_ = std::min(printInterval_ * 2, maxPrintInterVal);
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(waitMillis()));
    attempt_++;
    waitMillis_ = std::min(long(waitMillis_ * waitExponent), maxWaitMillis);
  }

  void printSuccess(const char *name) {
    write_milliseconds();
    const char *serverName = name ? name : "default";
    std::cout << milliseconds << " Created jack client \"" << name << "\"!"
              << std::endl;
  }
};

using namespace std;
using namespace tdap;
enum class ClientState {
  NONE,
  CLOSED,
  OPEN,
  CONFIGURED,
  ACTIVE,
  SHUTTING_DOWN
};

const char *client_state_name(ClientState state);

bool client_state_defined(ClientState state);

bool client_state_is_shutdown_state(ClientState state);

struct ShutDownInfo {
  jack_status_t status;
  const char *reason;
  bool isSet;

  static constexpr ShutDownInfo empty() {
    return {static_cast<jack_status_t>(0), nullptr, false};
  }

  static ShutDownInfo withReason(const char *reason) {
    return {static_cast<jack_status_t>(0), reason, true};
  }

  static ShutDownInfo withReasonAndCode(jack_status_t code,
                                        const char *reason) {
    return {code, reason, true};
  }

  bool isEmpty() { return !isSet; }
};

class JackClient;

struct CreateClientResult {
  JackClient *client;
  jack_status_t status;
  const char *name;

  bool success() { return (client); }

  JackClient *getClient() {
    if (success()) {
      return client;
    }
    throw std::runtime_error("No jack client created");
  }
};

class JackClient {
  ClientState state_ = ClientState::CLOSED;
  mutex mutex_;
  condition_variable awaitShutdownCondition_;
  thread awaitShutdownThread_;
  bool awaitShutdownThreadRunning_ = false;
  volatile bool shutDownOnSignal = false;

  jack_client_t *client_ = nullptr;
  ShutDownInfo shutdownInfo_{static_cast<jack_status_t>(0), nullptr, false};
  string name_;
  JackProcessor *processor_ = nullptr;
  ProcessingMetrics metrics_;
  long xRuns;
  long long lastXrunProcessingCycle;

  static void awaitShutdownCaller(JackClient *client);

  static void jackShutdownCallback(void *client);

  static void jackInfoShutdownCallback(jack_status_t code, const char *reason,
                                       void *client);

  static int jackBufferSizeCallback(jack_nframes_t frames, void *client);

  static int jackSampleRateCallback(jack_nframes_t rate, void *client);

  static int jackXrunCallback(void *client);

  void registerCallbacks();

  void awaitShutdownAndCloseUnsafe(unique_lock<mutex> &lock);

  void awaitShutDownAndClose();

  bool notifyShutdownUnsafe(ShutDownInfo info, unique_lock<mutex> &lock);

  void onShutdown(ShutDownInfo info);

  void closeUnsafe();

  static void jack_portnames_free(const char **names);

protected:
  virtual void registerAdditionalCallbacks(jack_client_t *client);

  JackClient(jack_client_t *client);

  int onMetricsUpdate(ProcessingMetrics m);

  int onSampleRateChange(jack_nframes_t rate);

  int onBufferSizeChange(jack_nframes_t size);

public:
  virtual int onXRun();

  static CreateClientResult createDefault(const char *serverName) {
    jack_status_t lastState = static_cast<JackStatus>(0);
    ClientOpenRetryPolicy policy;

    while (!SignalHandler::check_raised()) {
      jack_client_t *c = jack_client_open(
          serverName, JackOptions::JackNoStartServer, &lastState);
      if (c) {
        policy.printSuccess(serverName);
        return {new JackClient(c), static_cast<JackStatus>(0), serverName};
      }
      policy.printFailureAndWait(lastState);
    }
    return {nullptr, lastState, serverName};
  }

  template <typename... A>
  static CreateClientResult create(const char *serverName,
                                   jack_options_t options, A... args) {
    jack_status_t lastState = static_cast<JackStatus>(0);
    ClientOpenRetryPolicy policy;

    while (!SignalHandler::check_raised()) {
      jack_client_t *c =
          jack_client_open(serverName, options | JackOptions::JackNoStartServer,
                           &lastState, args...);
      if (c) {
        policy.printSuccess(serverName);
        return {new JackClient(c), static_cast<JackStatus>(0), serverName};
      }
      policy.printFailureAndWait(lastState);
    }
    return {nullptr, lastState, serverName};
  }

  const string &name() const;

  bool setProcessor(JackProcessor &processor);

  void setActive();

  ClientState getState();

  void notifyShutdown(const char *reason);

  ShutDownInfo awaitClose();

  static PortNames portNames(jack_client_t *client, const char *namePattern,
                             const char *typePattern, unsigned long flags);

  PortNames portNames(const char *namePattern, const char *typePattern,
                      unsigned long flags);

  ShutDownInfo close();

  virtual ~JackClient();
};

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_JACKCLIENT_GUARD_H_ */
