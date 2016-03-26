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
#include <tdap/Guards.hpp>
#include <tdap/MemoryFence.hpp>
#include <speakerman/jack/ErrorHandler.hpp>
#include <speakerman/jack/JackProcessor.hpp>

namespace speakerman {

using namespace std;
using namespace tdap;


JackProcessor::Reset::Reset(JackProcessor *owner) : owner_(owner) { }
JackProcessor::Reset::~Reset()
{
	owner_->unsafeResetState();
}

	int JackProcessor::realtimeCallback(jack_nframes_t frames, void *data)
	{
		if (data) {
			try {
				return static_cast<JackProcessor *>(data)->realtimeProcessWrapper(frames);
			}
			catch (const std::exception &e) {
				cerr << "Exception in processing thread: " << e.what() << endl;
				return 1;
			}
		}
		return 1;
	}

	int JackProcessor::realtimeProcessWrapper(jack_nframes_t frames)
	{
		TryEnter guard(running_);
		MemoryFence fence;
		processingCycles++;
		if (guard.entered() && ports_) {
			ports_->getBuffers(frames);
			return process(frames, *ports_) ? 0 : 1;
		}
		return 0;
	}

	void JackProcessor::ensurePorts(jack_client_t *client)
	{
		if (ports_) {
			return;
		}
		ports_ = new Ports(getDefinitions());

		ports_->registerPorts(client);

		ErrorHandler::checkZeroOrThrow(jack_set_process_callback(client, realtimeCallback, this), "Setting callback");
	}

	void JackProcessor::unsafeResetState()
	{
		running_.test_and_set();
		metrics_ = {0, 0};
		if (ports_) {
			delete ports_;
		}
	}

	JackProcessor::JackProcessor() : processingCycles(0) { running_.test_and_set(); }
	bool JackProcessor::updateMetrics(jack_client_t *client, ProcessingMetrics update)
	{
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
				needsBufferSize() ? update.bufferSize : 0
		};

		if (rateConditionMet && bufferSizeConditionMet) {
			if (onMetricsUpdate(relevantMetrics)) {
				if (metrics_ == ProcessingMetrics::withRate(0).withBufferSize(0)) {
					ensurePorts(client);
				}
				metrics_ = relevantMetrics;
				running_.clear();
				return true;
			}
			return false;
		}
		return true;
	}

	void JackProcessor::onActivate(jack_client_t *client)
	{
		lock guard(mutex_);
		if (ports_) {
			onPortsEnabled(client, *ports_);
		}
	}

	void JackProcessor::reset()
	{
		lock guard(mutex_);
		Reset reset(this);
		onReset();
	}

	long long JackProcessor::getProcessingCycles() const
	{
		MemoryFence fence;
		return processingCycles;
	}

	JackProcessor::~JackProcessor()
	{
		if (ports_) {
			delete ports_;
			ports_ = nullptr;
		}
	}


} /* End of namespace speakerman */
