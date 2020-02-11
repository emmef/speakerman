/*
 * JackClient.cpp
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

#include <iostream>
#include <jack/jack.h>
#include <signal.h>
#include <string>

#include <condition_variable>
#include <speakerman/jack/ErrorHandler.hpp>
#include <speakerman/jack/JackClient.hpp>
#include <speakerman/jack/SignalHandler.hpp>

namespace speakerman {

using namespace std;
using namespace tdap;

const char *client_state_name(ClientState state) {
  switch (state) {
  case ClientState::CLOSED:
    return "CLOSED";
  case ClientState::CONFIGURED:
    return "CONFIGURED";
  case ClientState::OPEN:
    return "OPEN";
  case ClientState::ACTIVE:
    return "ACTIVE";
  case ClientState::SHUTTING_DOWN:
    return "SHUTTING_DOWN";
  default:
    return "NONE";
  }
}

bool client_state_defined(ClientState state) {
  switch (state) {
  case ClientState::CLOSED:
  case ClientState::CONFIGURED:
  case ClientState::OPEN:
  case ClientState::ACTIVE:
  case ClientState::SHUTTING_DOWN:
    return true;
  default:
    return false;
  }
}

bool client_state_is_shutdown_state(ClientState state) {
  switch (state) {
  case ClientState::CLOSED:
  case ClientState::SHUTTING_DOWN:
    return true;
  default:
    return false;
  }
}

void JackClient::awaitShutdownCaller(JackClient *client) {
  CountedThreadGuard guard("Jack client - await shutdown");
  client->awaitShutDownAndClose();
}

void JackClient::jackShutdownCallback(void *client) {
  if (client) {
    static_cast<JackClient *>(client)->onShutdown(
        ShutDownInfo::withReason("jack_on_shutdown"));
  }
}

void JackClient::jackInfoShutdownCallback(jack_status_t code,
                                          const char *reason, void *client) {
  if (client) {
    static_cast<JackClient *>(client)->onShutdown({code, reason, true});
  }
}

int JackClient::jackBufferSizeCallback(jack_nframes_t frames, void *client) {
  return client ? static_cast<JackClient *>(client)->onBufferSizeChange(frames)
                : 1;
}

int JackClient::jackSampleRateCallback(jack_nframes_t rate, void *client) {
  return client ? static_cast<JackClient *>(client)->onSampleRateChange(rate)
                : 1;
}

int JackClient::jackXrunCallback(void *client) {
  return client ? static_cast<JackClient *>(client)->onXRun() : 1;
}

void JackClient::registerCallbacks() {
  jack_on_shutdown(client_, jackShutdownCallback, this);
  jack_on_info_shutdown(client_, jackInfoShutdownCallback, this);
}

void JackClient::awaitShutdownAndCloseUnsafe(unique_lock<mutex> &lock) {
  cout << "Await shutdown..." << endl;
  while (!client_state_is_shutdown_state(state_)) {
    awaitShutdownCondition_.wait(lock);
    cout << "Wakeup!..." << endl;
  }
  closeUnsafe();
}

void JackClient::awaitShutDownAndClose() {
  cout << "Start closer thread" << endl;
  unique_lock<mutex> lock(mutex_);
  try {
    awaitShutdownThreadRunning_ = true;
    cout << "Closer thread awaits..." << endl;
    awaitShutdownAndCloseUnsafe(lock);
    awaitShutdownThreadRunning_ = false;
    awaitShutdownCondition_.notify_all();
  } catch (const std::exception &e) {
    cerr << "Encountered exception in shutdown-thread of \"" << name_
         << "\": " << e.what() << endl;
  } catch (...) {
    awaitShutdownThreadRunning_ = false;
    awaitShutdownCondition_.notify_all();
    throw;
  }
}

bool JackClient::notifyShutdownUnsafe(ShutDownInfo info,
                                      unique_lock<mutex> &lock) {
  if (!client_state_is_shutdown_state(state_)) {
    shutdownInfo_ = info;
    state_ = ClientState::SHUTTING_DOWN;
    awaitShutdownCondition_.notify_all();
    return true;
  }
  cout << "Ignore notify: already closed" << endl;
  return false;
}

void JackClient::onShutdown(ShutDownInfo info) {
  unique_lock<mutex> lock(mutex_);
  notifyShutdownUnsafe(info, lock);
}

void JackClient::closeUnsafe() {
  if (state_ == ClientState::CLOSED) {
    cout << "closeUnsafe(): already closed" << endl;
    return;
  }
  cout << "closeUnsafe()" << endl;
  if (client_) {
    std::cout << "Closing client: '" << name() << "'" << std::endl;
    jack_client_t *c = client_;
    client_ = nullptr;
    ErrorHandler::clear_ensure();
    ErrorHandler::setForceLogNext();
    jack_client_close(c);
  }
  processor_ = nullptr;
  ProcessingMetrics m;
  metrics_ = m;
  state_ = ClientState::CLOSED;
  cout << "closeUnsafe(): end" << endl;
  SignalHandler::instance().raise_signal(SIGABRT);
}

void JackClient::jack_portnames_free(const char **names) { jack_free(names); }

void JackClient::registerAdditionalCallbacks(jack_client_t *client) {}

JackClient::JackClient(jack_client_t *client)
    : client_(client), xRuns(0), lastXrunProcessingCycle(0) {
  awaitShutdownThread_ = thread(awaitShutdownCaller, this);
  awaitShutdownThread_.detach();
  name_ = jack_get_client_name(client_);
  state_ = ClientState::OPEN;
  registerCallbacks();
  registerAdditionalCallbacks(client_);
}

int JackClient::onMetricsUpdate(ProcessingMetrics m) {
  unique_lock<mutex> lock(mutex_);
  try {
    ProcessingMetrics update = metrics_.mergeWithUpdate(m);
    if (processor_->updateMetrics(client_, update)) {
      metrics_ = update;
      return 0;
    }
  } catch (const std::exception &e) {
    std::cerr << "Exception in onMetricsUpdate: " << e.what() << std::endl;
  }
  return 1;
}

int JackClient::onSampleRateChange(jack_nframes_t rate) {
  return onMetricsUpdate({rate, 0});
}

int JackClient::onBufferSizeChange(jack_nframes_t size) {
  return onMetricsUpdate({0, size});
}

int JackClient::onXRun() {
  static constexpr long long XRUNS_FOR_MEASUREMENT = 200;
  static constexpr long long MAX_ONE_OUT_OF = 50;
  if (!processor_) {
    return 0;
  }
  long long cycles = processor_->getProcessingCycles();
  if (xRuns == 0) {
    lastXrunProcessingCycle = cycles;
  }
  xRuns++;
  long long cyclesSinceFirstXrun = Value<long long>::max(
      XRUNS_FOR_MEASUREMENT, cycles - lastXrunProcessingCycle);
  cerr << "Xrun(" << xRuns << ") out of " << cyclesSinceFirstXrun
       << " processing cycles" << endl;
  if (xRuns < XRUNS_FOR_MEASUREMENT) {
    return 0;
  }
  if (XRUNS_FOR_MEASUREMENT * MAX_ONE_OUT_OF / cyclesSinceFirstXrun) {
    cerr << "More than 1 out of " << MAX_ONE_OUT_OF << " cycles Xrun: aborting"
         << endl;
    notifyShutdown("XRUN occured!");
    return 1;
  }
  xRuns = 0;
  return 0;
}

const std::string &JackClient::name() const { return name_; }

bool JackClient::setProcessor(JackProcessor &processor) {
  {
    unique_lock<mutex> lock(mutex_);
    if (state_ != ClientState::OPEN) {
      throw runtime_error("setProcessor: Not in OPEN state");
    }
    processor_ = &processor;
  }
  try {
    if (processor_->needsBufferSize()) {
      ErrorHandler::checkZeroOrThrow(
          jack_set_buffer_size_callback(client_, jackBufferSizeCallback, this),
          "Set buffer size callback");
    }
    if (processor_->needsSampleRate()) {
      ErrorHandler::checkZeroOrThrow(
          jack_set_sample_rate_callback(client_, jackSampleRateCallback, this),
          "Set sample rate callback");
    }
    ErrorHandler::checkZeroOrThrow(
        jack_set_xrun_callback(client_, jackXrunCallback, this),
        "Set sample rate callback");
    {
      unique_lock<mutex> lock(mutex_);
      state_ = ClientState::CONFIGURED;
      return true;
    }
  } catch (const std::exception &e) {
    std::cout << "Wrong" << e.what() << std::endl;
    {
      unique_lock<mutex> lock(mutex_);
      processor_ = nullptr;
    }
    throw e;
  }
}

void JackClient::setActive() {
  unique_lock<mutex> lock(mutex_);
  if (state_ != ClientState::CONFIGURED) {
    throw runtime_error("setProcessor: Not in CONFIGURED state");
  }
  ErrorHandler::checkZeroOrThrow(jack_activate(client_), "Activating");
  state_ = ClientState::ACTIVE;
  processor_->onActivate(client_);
}

speakerman::ClientState JackClient::getState() {
  unique_lock<mutex> lock(mutex_);
  return state_;
}

void JackClient::notifyShutdown(const char *reason) {
  onShutdown(ShutDownInfo::withReason(reason));
}

speakerman::ShutDownInfo JackClient::awaitClose() {
  unique_lock<mutex> lock(mutex_);
  if (state_ != ClientState::ACTIVE) {
    throw runtime_error("setProcessor: Not in ACTIVE state");
  }
  awaitShutdownAndCloseUnsafe(lock);
  return shutdownInfo_;
}

speakerman::PortNames JackClient::portNames(jack_client_t *client,
                                            const char *namePattern,
                                            const char *typePattern,
                                            unsigned long flags) {
  const char **names = jack_get_ports(client, namePattern, typePattern, flags);
  return PortNames(names, jack_portnames_free, 1024);
}

speakerman::PortNames JackClient::portNames(const char *namePattern,
                                            const char *typePattern,
                                            unsigned long flags) {
  unique_lock<mutex> lock(mutex_);
  if (state_ != ClientState::OPEN && state_ != ClientState::CONFIGURED) {
    throw runtime_error("setProcessor: Not in OPEN or CONFIGURED state");
  }
  return portNames(client_, namePattern, typePattern, flags);
}

speakerman::ShutDownInfo JackClient::close() {
  unique_lock<mutex> lock(mutex_);
  if (notifyShutdownUnsafe(ShutDownInfo::withReason("Explicit close"), lock)) {
    closeUnsafe();
    while (awaitShutdownThreadRunning_) {
      cout << "Wait for terminate thread to end..." << endl;
      awaitShutdownCondition_.wait(lock);
      awaitShutdownCondition_.notify_all();
    }
    cout << "Wait for terminate thread to end -- done!" << endl;
    return shutdownInfo_;
  } else {
    return ShutDownInfo::withReason("Already closing");
  }
}

JackClient::~JackClient() {
  cout << "destructor" << endl;
  close();
  cout << "destructor -- done!" << endl;
}

} /* End of namespace speakerman */
