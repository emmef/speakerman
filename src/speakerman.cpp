/*
 * Speakerman.cpp
 *
 * Part of 'Speaker management system'
 *
 * Copyright (C) 2013 Michel Fleur.
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

#include <atomic>
#include <cmath>
#include <iostream>
#include <malloc.h>
#include <thread>

//#define DYNAMICS_PROCESSOR_LIMITER_ANALYSIS 1

#include <speakerman/SpeakerManager.hpp>
#include <speakerman/SpeakermanConfig.hpp>
#include <speakerman/SpeakermanWebServer.hpp>
#include <speakerman/jack/JackClient.hpp>
#include <speakerman/jack/SignalHandler.hpp>
#include <speakerman/Webserver.h>
#include <speakerman/SpeakerManagerGenerator.h>
#include <tdap/Allocation.hpp>

using namespace speakerman;
using namespace tdap;

typedef double sample_t;
typedef double accurate_t;

template <class T> class Owner : public ConsecutiveAllocationOwner {
  atomic<T *> __client;
  mutex mutex_;

public:
  Owner(size_t block_size) : ConsecutiveAllocationOwner(block_size) {
    __client.store(0);
    lock_memory();
  }

  template <class A> void set(T *(*function)(const A &), const A &argument) {
    unique_lock<mutex> lock(mutex_);
    set_null(true);

    consecutive_alloc::Enable conseq = enable();

    __client.exchange(function(argument));
  }

  template <class A> void set(T *(*function)(const A *), const A *argument) {
    unique_lock<mutex> lock(mutex_);
    set_null(true);

    consecutive_alloc::Enable conseq = enable();

    __client.exchange(function(argument));
  }

  void set_null(bool reset) {
    T *previous = __client.exchange(nullptr);
    if (previous == nullptr) {
      return;
    }
    {
      consecutive_alloc::Enable conseq = enable();
      cout << this << ": Delete client!!!" << endl;
      delete previous;
      cout << this << ": Delete client!!! -- done!!!" << endl;
    }
    if (reset && !reset_allocation()) {
      throw std::runtime_error("Could not free up memory");
    }
  }

  T &get() const { return *__client.load(); }

  ~Owner() {
    unique_lock<mutex> lock(mutex_);
    set_null(false);
  }
};

ConsecutiveAllocatedObjectOwner<AbstractSpeakerManager> manager(100 * 1000 *
                                                                1000);
SpeakermanConfig configFileConfig;

static void webServer() {
  jack::CountedThreadGuard guard("Web server listening thread");


  mg_log_set("0");
  web_server server(manager.get());

  try {
    server.run("http://localhost:8088", 1000);
    cout << "Web server exit" << endl;
  } catch (const std::exception &e) {
    std::cerr << "Web server error: " << e.what() << std::endl;
  } catch (const jack::signal_exception &e) {
    cerr << "Web server thread stopped" << endl;
    e.handle();
  }
}

int mainLoop(ConsecutiveAllocatedObjectOwner<jack::JackClient> &) {
  std::thread webServerThread(webServer);
  webServerThread.detach();

  const std::chrono::milliseconds sleep_time(100);
  try {
    while (true) {
      this_thread::sleep_for(sleep_time);
      jack::SignalHandler::check_raised();
    }
  } catch (const jack::signal_exception &e) {
    e.handle();
    return e.signal();
  }
}

jack::JackClient *create_client(const char *name) {
  auto result = jack::JackClient::createDefault(name);
  if (!result.success()) {
    cerr << "Could not create jack client \"" << name << "\"" << endl;
  }
  return result.getClient();
}

void display_owner_info(ConsecutiveAllocationOwner &owner,
                        const char *message) {
  cout << message;
  cout << ": consecutive allocation stats: block_size="
       << owner.get_block_size();
  cout << "; allocated=" << owner.get_allocated_bytes();
  cout << "; consecutive=" << owner.is_consecutive();
  cout << "; (owner=" << &owner << ")";
  cout << endl;
}

int main(int count, char *arguments[]) {

  cout << "Executing " << arguments[0] << endl;
  configFileConfig = readSpeakermanConfig();

  for (int arg = 1; arg < count; arg++) {
    if (strncmp(arguments[arg], "--dump-config", 20) == 0) {
      return 0;
    } else {
      cerr << "Invalid argument:" << arguments[arg] << endl;
      return 1;
    }
  }

  jack::AwaitThreadFinishedAfterExit await(5000, "Await thread shutdown...");
  MemoryFence::release();

  manager.generate<AbstractSpeakerManager, const SpeakermanConfig &>(
      createManager, configFileConfig);

  display_owner_info(manager, "Processor");

  ConsecutiveAllocatedObjectOwner<jack::JackClient> clientOwner(4048576);

  clientOwner.generate(create_client, "Speaker manager");

  display_owner_info(clientOwner, "Jack client");

  clientOwner.get().setProcessor(manager.get());

  std::cout << "activate..." << std::endl;
  clientOwner.get().setActive();

  std::cout << "activated..." << std::endl;
  return mainLoop(clientOwner);
}
