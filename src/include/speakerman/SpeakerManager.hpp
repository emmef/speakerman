#ifndef SPEAKERMAN_M_SPEAKER_MANAGER_HPP
#define SPEAKERMAN_M_SPEAKER_MANAGER_HPP
/*
 * speakerman/SpeakerManager.hpp
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

#include "DynamicsProcessor.hpp"
#include "LogicalGroupConfig.h"
#include <cmath>
#include <iostream>
#include <memory>
#include <speakerman/SpeakerManagerControl.h>
#include <speakerman/jack/JackClient.hpp>
#include <speakerman/jack/JackProcessor.hpp>
#include <speakerman/jack/Names.hpp>
#include <tdap/Delay.hpp>
#include <tdap/FixedSizeArray.hpp>
#include <tdap/IirButterworth.hpp>
#include <tdap/MemoryFence.hpp>
#include <tdap/Noise.hpp>
#include <tdap/Weighting.hpp>

namespace speakerman {

class AbstractSpeakerManager : public SpeakerManagerControl,
                               public jack::JackProcessor {};

template <typename T, size_t CHANNELS_PER_GROUP, size_t GROUPS,
          size_t CROSSOVERS, size_t LOGICAL_INPUTS>
class SpeakerManager : public AbstractSpeakerManager {
  static_assert(is_floating_point<T>::value,
                "expected floating-point value parameter");

  using Processor =
      DynamicsProcessor<T, CHANNELS_PER_GROUP, GROUPS, CROSSOVERS, LOGICAL_INPUTS>;
  using CrossoverFrequencies = typename Processor::CrossoverFrequencies;
  using ThresholdValues = typename Processor::ThresholdValues;
  using Levels = DynamicProcessorLevels;
  using ConfigData = typename Processor::ConfigData;

  static constexpr size_t OUTPUTS = Processor::OUTPUTS;
  RefArray<jack_default_audio_sample_t> inputs[LOGICAL_INPUTS];
  RefArray<jack_default_audio_sample_t> outputs[OUTPUTS];
  AlignedArray<T, LOGICAL_INPUTS, 32> inFrame;
  FixedSizeArray<T, OUTPUTS> outFrame;

  static CrossoverFrequencies crossovers() {
    CrossoverFrequencies cr;
    cr[0] = 80;
    switch (cr.size()) {
    case 1:
      cr[0] = 120;
      break;
    case 2:
      cr[1] = 120;
      break;
    case 3:
      cr[1] = 160;
      cr[2] = 2500;
      break;
    default:
      throw std::invalid_argument("Too many crossovers");
    }
    return cr;
  };

  jack::PortDefinitions portDefinitions_;
  SpeakermanConfig config_;
  Processor processor;
  std::mutex mutex_;

  bool fewerInputs = false;
  bool fewerOutputs = false;

  struct TransportData {
    ConfigData configData;
    Levels levels;
    bool configChanged;

    TransportData() : levels(GROUPS), configChanged(false) {}
  };

  Transport<TransportData> transport;
  TransportData preparedConfigData;

protected:
  const jack::PortDefinitions &getDefinitions() override {
    return portDefinitions_;
  }

  virtual bool onMetricsUpdate(jack::ProcessingMetrics metrics) override {
    std::cout << "Updated metrics: {rate:" << metrics.sampleRate
              << ", bsize:" << metrics.bufferSize << "}" << std::endl;
    processor.setSampleRate(metrics.sampleRate, crossovers(), config_);
    Levels levels(GROUPS);
    levels.reset();
    preparedConfigData.configData = processor.getConfigData();
    preparedConfigData.configChanged =
        true; // force to reload equalizer filters
    transport.init(preparedConfigData, true);
    preparedConfigData.configChanged = false;
    return true;
  }

  void connectPorts(jack_client_t *client, const char *sourceName,
                    const char *destinationName) {
    if (!jack::Port::try_connect_ports(client, sourceName, destinationName)) {
      std::cout << "Could not connect \"" << sourceName << "\" with \""
                << destinationName << "\"" << std::endl;
    } else {
      std::cout << "Connected \"" << sourceName << "\" with \""
                << destinationName << "\"" << std::endl;
    }
  }

  virtual void onPortsEnabled(jack_client_t *client,
                              const jack::Ports &ports) override {
    const char *unspecified = ".*";
    jack::PortNames playbackPortNames = jack::JackClient::portNames(
        client, "^system", unspecified, JackPortIsPhysical | JackPortIsInput);
    jack::PortNames capturePortNames = jack::JackClient::portNames(
        client, "^system", unspecified, JackPortIsPhysical | JackPortIsOutput);
    jack::NameList inputs = ports.inputNames();
    jack::NameList outputs = ports.outputNames();

    size_t captureCount = capturePortNames.count();
    size_t inputCount = Values::min(inputs.count(), captureCount);
    int subOutPut =
        Values::min(config_.subOutput, playbackPortNames.count()) - 1;
    size_t outputCount =
        Values::min(outputs.count(), playbackPortNames.count());

    size_t groupOutputStart = 0;
    if (subOutPut >= 0) {
      connectPorts(client, outputs.get(0), playbackPortNames.get(subOutPut));
      groupOutputStart = 1;
    }

    std::cout << "Outputs: playback " << playbackPortNames.count() << " out "
              << outputs.count() << std::endl;
    for (size_t out = groupOutputStart, port = 0; out < outputCount; port++) {
      if (int(port) == subOutPut) {
        continue;
      }
      connectPorts(client, outputs.get(out++), playbackPortNames.get(port));
    }

    auto mapping = config_.logicalInputs.createMapping();
    for (const auto entry : mapping) {
      connectPorts(client,
                   capturePortNames.get(entry.wrappedPort(captureCount)),
                   inputs.get(entry.channel));
    }
  }

  virtual void onReset() override {
    std::cout << "No action on reset" << std::endl;
  }

  virtual bool process(jack_nframes_t frames, const jack::Ports &ports) override {
    auto lockFreeData =
        transport
            .getLockFreeNoFence(); // method already called from within a fence
    bool modifiedTransport = lockFreeData.modified();
    ZFPUState state;

    if (modifiedTransport) {
      processor.levels.reset();
      if (lockFreeData.data().configChanged) {
        processor.updateConfig(lockFreeData.data().configData);
      } else {
        processor.updateConfig(processor.getConfigData());
      }
    }
    size_t portNumber = 0;
    int subPort = config_.subOutput - 1;
    if (subPort >= 0) {
      outputs[0] = ports.getBuffer(portNumber++);
      for (size_t output = 1; output < OUTPUTS; output++, portNumber++) {
        outputs[output] = ports.getBuffer(portNumber);
      }
    } else {
      for (size_t output = 0; output < OUTPUTS - 1; output++, portNumber++) {
        outputs[output] = ports.getBuffer(portNumber);
      }
    }
    for (size_t input = 0; input < LOGICAL_INPUTS; input++, portNumber++) {
      inputs[input] = ports.getBuffer(portNumber);
    }

    if (subPort >= 0) {
      for (size_t i = 0; i < frames; i++) {
        for (size_t channel = 0; channel < LOGICAL_INPUTS; channel++) {
          inFrame[channel] = inputs[channel][i];
        }

        processor.process(inFrame, outFrame);

        for (size_t channel = 0; channel < OUTPUTS; channel++) {
          outputs[channel][i] = outFrame[channel];
        }
      }
    } else {
      double scale = 1.0 / sqrt(OUTPUTS - 1);
      for (size_t i = 0; i < frames; i++) {
        for (size_t channel = 0; channel < LOGICAL_INPUTS; channel++) {
          inFrame[channel] = inputs[channel][i];
        }

        processor.process(inFrame, outFrame);

        double subValue = outFrame[0] * scale;

        for (size_t channel = 0; channel < OUTPUTS - 1; channel++) {
          outputs[channel][i] = outFrame[channel + 1] + subValue;
        }
      }
    }

    lockFreeData.data().levels = processor.levels;

    return true;
  }

public:
  virtual bool needsBufferSize() const override { return false; }

  virtual bool needsSampleRate() const override { return true; }

  SpeakerManager(const SpeakermanConfig &config)
      : portDefinitions_(1 + 2 * ProcessingGroupConfig::MAX_CHANNELS),
        config_(config) {
    std::unique_ptr<char> name(new char[1 + jack::Names::get_port_size()]);
    if (config.subOutput > 0) {
      portDefinitions_.addOutput("out_sub");
      cout << "I: added output "
           << "out_sub" << std::endl;
    }
    for (size_t channel = 0; channel < Processor::OUTPUTS - 1; channel++) {
      snprintf(name.get(), 1 + jack::Names::get_port_size(), "out_%zu_%zu",
               1 + channel / CHANNELS_PER_GROUP,
               1 + channel % CHANNELS_PER_GROUP);
      portDefinitions_.addOutput(name.get());
      cout << "I: added output " << name.get() << std::endl;
    }
    auto map = config.logicalInputs.createMapping();
    for (const auto entry : map) {
      snprintf(name.get(), 1 + jack::Names::get_port_size(), "in_%zu_%zu",
               entry.logicalGroup + 1, entry.groupChannel + 1);
      portDefinitions_.addInput(name.get());
      cout << "I: added input " << name.get() << std::endl;
    }
  }

  virtual const SpeakermanConfig &getConfig() const override { return config_; }

  virtual bool getLevels(DynamicProcessorLevels *levels,
                         std::chrono::milliseconds duration) override {
    std::unique_lock<std::mutex> lock(mutex_);
    TransportData result;
    preparedConfigData.levels.reset();
    preparedConfigData.configChanged = false;
    if (transport.getAndSet(preparedConfigData, result, duration)) {
      if (levels) {
        *levels = result.levels;
      }
      return true;
    }
    return false;
  }

  bool
  applyConfigAndGetLevels(const SpeakermanConfig &config,
                          DynamicProcessorLevels *levels,
                          std::chrono::milliseconds duration) override {
    std::unique_lock<std::mutex> lock(mutex_);
    TransportData result;
    config_ = config;
    preparedConfigData.configData = processor.createConfigData(config);
    preparedConfigData.levels.reset();
    preparedConfigData.configChanged = true;
    if (transport.getAndSet(preparedConfigData, result, duration)) {
      if (levels) {
        *levels = result.levels;
      }
      return true;
    }
    return false;
  }

  const jack::ProcessingStatistics getStatistics() const override {
    return JackProcessor::getStatistics();
  }
};

} // namespace speakerman

#endif // SPEAKERMAN_M_SPEAKER_MANAGER_HPP
