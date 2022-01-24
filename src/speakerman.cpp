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
  web_server server(manager.get());

  try {
    server.open("8088", 60, 10, nullptr);
    server.work(nullptr);
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

// speakerman::utils::config::Reader configReader;
speakerman::server_socket webserver;

template <typename F, size_t GROUPS, size_t CROSSOVERS, size_t LOGICAL_INPUTS,
          size_t CHANNELS_PER_GROUP = ProcessingGroupConfig::MAX_CHANNELS>
AbstractSpeakerManager *
createManagerSampleType(const SpeakermanConfig &config) {
  static_assert(is_floating_point<F>::value,
                "Sample type must be floating point");
  const size_t channelsPerGroup = config.processingGroups.channels;

  if (channelsPerGroup > ProcessingGroupConfig::MAX_CHANNELS) {
    throw invalid_argument("Maximum number of channels per group exceeded.");
  }
  if constexpr (CHANNELS_PER_GROUP < 1) {
    throw invalid_argument("Must have at least one channel per group.");
  } else if (channelsPerGroup == CHANNELS_PER_GROUP) {
    return new SpeakerManager<F, CHANNELS_PER_GROUP, GROUPS, CROSSOVERS,
                              LOGICAL_INPUTS>(config);
  } else {
    return createManagerSampleType<F, GROUPS, CROSSOVERS, LOGICAL_INPUTS,
                                   CHANNELS_PER_GROUP - 1>(config);
  }
}

template <typename F, size_t CROSSOVERS, size_t LOGICAL_INPUTS,
          size_t PROCESSING_GROUPS = ProcessingGroupsConfig::MAX_GROUPS>
static AbstractSpeakerManager *
createManagerGroup(const SpeakermanConfig &config) {

  size_t processingGroups = config.processingGroups.groups;
  if (processingGroups > ProcessingGroupsConfig::MAX_GROUPS) {
    throw std::invalid_argument(
        "Maximum number of processing groups exceeded.");
  }
  if constexpr (PROCESSING_GROUPS < 1) {
    throw std::invalid_argument("Need at least one processing group.");
  } else if (processingGroups == PROCESSING_GROUPS) {
    return createManagerSampleType<F, PROCESSING_GROUPS, CROSSOVERS,
                                   LOGICAL_INPUTS>(config);
  } else {
    return createManagerGroup<F, CROSSOVERS, LOGICAL_INPUTS,
                              PROCESSING_GROUPS - 1>(config);
  }
}

template <typename F, size_t LOGICAL_INPUTS,
          size_t CROSSOVERS = SpeakermanConfig::MAX_CROSSOVERS>
static AbstractSpeakerManager *
createManagerCrossovers(const SpeakermanConfig &config) {
  size_t crossovers = config.crossovers;
  if (crossovers > SpeakermanConfig::MAX_CROSSOVERS) {
    throw std::invalid_argument("Maximum number of crossovers exceeded.");
  }
  if constexpr (CROSSOVERS < 1) {
    throw std::invalid_argument("Need at least one crossover.");
  } else if (crossovers == CROSSOVERS) {
    return createManagerGroup<double, CROSSOVERS, LOGICAL_INPUTS>(config);
  } else {
    return createManagerCrossovers<F, LOGICAL_INPUTS, CROSSOVERS - 1>(config);
  }
}

using namespace std;

template <size_t TOTAL_CHANNELS = LogicalGroupConfig::MAX_CHANNELS>
AbstractSpeakerManager *create_manager(const SpeakermanConfig &config) {
  size_t channels = config.logicalInputs.getTotalChannels();
  if (channels > LogicalGroupConfig::MAX_CHANNELS) {
    throw std::invalid_argument("Maximum total number logical input channels exceeded.");
  }
  if constexpr (TOTAL_CHANNELS < 1) {
    throw std::invalid_argument("Need at least one logical input channel.");
  }
  else if (channels == TOTAL_CHANNELS) {
    return createManagerCrossovers<double, TOTAL_CHANNELS>(config);
  }
  else {
    return create_manager<TOTAL_CHANNELS - 1>(config);
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
      create_manager, configFileConfig);

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
