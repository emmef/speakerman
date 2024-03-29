/*
 * JackProcessor.cpp
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
#include <speakerman/jack/ErrorHandler.hpp>
#include <speakerman/jack/JackProcessor.hpp>
#include <sys/mman.h>
#include <tdap/Guards.hpp>
#include <tdap/MemoryFence.hpp>
#include <thread>
#include <chrono>

namespace speakerman::jack {

using namespace std;
using namespace tdap;

JackProcessor::Reset::Reset(JackProcessor *owner) : owner_(owner) {}

JackProcessor::Reset::~Reset() { owner_->unsafeResetState(); }

void JackProcessor::realtimeInitCallback(void *) {
  static constexpr size_t preAllocStackSize = 102400;
  auto self = pthread_self();
  pthread_attr_t self_attributes;
  if (pthread_getattr_np(self, &self_attributes)) {
    perror("Could not get RT thread attributes");
    return;
  }
  void *self_address;
  size_t self_size;
  if (pthread_attr_getstack(&self_attributes, &self_address, &self_size)) {
    perror("Could not get RT thread address and size");
    return;
  }
//  unsigned long long id = static_cast<unsigned long long>(self);
//  char *start = static_cast<char *>(self_address);
//  char *end_address = start + self_size;
  char mark[preAllocStackSize];
//  void *mark_addr = &mark;
  for (size_t i = 0; i < preAllocStackSize; i++) {
    mark[i] = 0;
  }
}

int JackProcessor::realtimeCallback(jack_nframes_t frames, void *data) {
  if (data) {
    try {
      return static_cast<JackProcessor *>(data)->realtimeProcessWrapper(frames);
    } catch (const std::exception &e) {
      cerr << "Exception in processing thread: " << e.what() << endl;
      return 1;
    }
  }
  return 1;
}

int JackProcessor::realtimeProcessWrapper(jack_nframes_t frames) {
  TryEnter guard(running_);
  auto start = std::chrono::system_clock::now();
  int result = 0;// TODO Find out what the real error-behaviour should be!
  MemoryFence fence;
  if (guard.entered() && ports_) {
    ports_->getBuffers(frames);
    result = process(frames, *ports_) ? 0 : 1;
    auto end = std::chrono::system_clock::now();
    auto processingMicros = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    statistics.updateFrame(frames, processingMicros);
  }
  return result;
}

void JackProcessor::ensurePorts(jack_client_t *client) {
  if (ports_) {
    return;
  }
  ports_ = new Ports(getDefinitions());

  ports_->registerPorts(client);

  ErrorHandler::checkZeroOrThrow(
      jack_set_thread_init_callback(client, realtimeInitCallback, this),
      "Setting real time thread initialization callback");
  ErrorHandler::checkZeroOrThrow(
      jack_set_process_callback(client, realtimeCallback, this),
      "Setting processing callback");
}

void JackProcessor::unsafeResetState() {
  running_.test_and_set();
  metrics_ = {0, 0};
  if (ports_) {
    delete ports_;
  }
}

JackProcessor::JackProcessor() {
  running_.test_and_set();
}

bool JackProcessor::updateMetrics(jack_client_t *client,
                                  ProcessingMetrics update) {
  lock guard(mutex_);
  bool rateConditionMet =
      !needsSampleRate() ||
      (update.sampleRate != 0 && update.sampleRate != metrics_.sampleRate);
  bool bufferSizeConditionMet =
      !needsBufferSize() ||
      (update.bufferSize != 0 && update.bufferSize != metrics_.bufferSize);

  /**
   * If the processor indicates it does not need information
   * on either sample rate or buffer size, we will not make that information
   * available to it.
   */
  ProcessingMetrics relevantMetrics = {
      needsSampleRate() ? update.sampleRate : 0,
      needsBufferSize() ? update.bufferSize : 0};

  if (rateConditionMet && bufferSizeConditionMet) {
    if (onMetricsUpdate(relevantMetrics)) {
      if (metrics_ == ProcessingMetrics::withRate(0).withBufferSize(0)) {
        ensurePorts(client);
      }
      metrics_ = relevantMetrics;
      running_.clear();
      statistics.setSampleRate(metrics_.sampleRate);
      return true;
    }
    return false;
  }
  return true;
}

void JackProcessor::onActivate(jack_client_t *client) {
  lock guard(mutex_);
  if (ports_) {
    onPortsEnabled(client, *ports_);
  }
}

void JackProcessor::reset() {
  lock guard(mutex_);
  Reset reset(this);
  onReset();
}

const ProcessingStatistics JackProcessor::getStatistics() const {
  MemoryFence fence;
  return statistics;
}

JackProcessor::~JackProcessor() {
  if (ports_) {
    delete ports_;
    ports_ = nullptr;
  }
}

} // namespace speakerman
