/*
 * JackProcessor.hpp
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

#ifndef SMS_SPEAKERMAN_JACKPROCESSOR_GUARD_H_
#define SMS_SPEAKERMAN_JACKPROCESSOR_GUARD_H_

#include <jack/jack.h>
#include <jack/types.h>
#include <atomic>
#include <mutex>
#include <tdap/Guards.hpp>
#include <speakerman/jack/Port.hpp>

namespace speakerman {

using namespace std;
using namespace tdap;

struct ProcessingMetrics
{
	jack_nframes_t sampleRate;
	jack_nframes_t bufferSize;

	bool operator ==(const ProcessingMetrics &o) { return sampleRate == o.sampleRate && bufferSize == o.bufferSize; }

	static ProcessingMetrics withRate(jack_nframes_t r) { return { r, 0 }; }

	ProcessingMetrics withBufferSize(jack_nframes_t size) { return { sampleRate, size }; }

	ProcessingMetrics mergeWithUpdate(ProcessingMetrics update)
	{
		return { update.sampleRate ? update.sampleRate : sampleRate, update.bufferSize ? update.bufferSize : bufferSize };
	}
};


class JackProcessor
{
	mutex mutex_;
	Ports *ports_ = nullptr;
	ProcessingMetrics metrics_ = {0, 0};
	using lock = unique_lock<mutex>;

	std::atomic_flag running_ = ATOMIC_FLAG_INIT;

	class Reset {
		JackProcessor *owner_;
	public:
		Reset(JackProcessor *owner) : owner_(owner) { }
		~Reset()
		{
			owner_->unsafeResetState();
		}
	};

	static int realtimeCallback(jack_nframes_t frames, void *data)
	{
		if (data) {
			try {
				return static_cast<JackProcessor *>(data)->realtimeProcessWrapper(frames);
			}
			catch (const std::exception e) {
				return 1;
			}
		}
		return 1;
	}

	int realtimeProcessWrapper(jack_nframes_t frames)
	{
		TryEnter guard(running_);
		if (guard.entered() && ports_) {
			ports_->getBuffers(frames);
			return process(frames, *ports_) ? 0 : 1;
		}
		return 0;
	}

	void ensurePorts(jack_client_t *client)
	{
		if (ports_) {
			return;
		}
		ports_ = new Ports(getDefinitions());

		ports_->registerPorts(client);

		onPortsRegistered(client, *ports_);

		ErrorHandler::checkZeroOrThrow(jack_set_process_callback(client, realtimeCallback, this), "Setting callback");
	}

	void unsafeResetState()
	{
		running_.test_and_set();
		metrics_ = {0, 0};
		if (ports_) {
			delete ports_;
		}
	}


protected:
	virtual const PortDefinitions &getDefinitions() =  0;
	/**
	 * Do whatever needs to happen when the processing metrics need to
	 * be initialized or updated.
	 * Called at configuration time and sometimes during suspended
	 * processing. It is allowed to use blocking operations.
	 * @param metrics The (new) processing metrics
	 * @return true on success (in which case the metrics will be
	 * reflected in later calls to getBufferSize() and getSampleRate(),
	 * false on error in which case processing won't start or will be
	 * terminated.
	 */
	virtual bool onMetricsUpdate(ProcessingMetrics metrics) = 0;
	/**
	 * Do whatever is necessary when the ports are registered with
	 * the Jack server, for instance, change port connections.
	 */
	virtual void onPortsRegistered(jack_client_t * client, const Ports &ports) = 0;
	/**
	 * Do whatever is necessary if the state is reset.
	 * This is not a destructor. But the processor can be reused or
	 * even continue for a new jack client if the old one happened
	 * to die.
	 */
	virtual void onReset() = 0;
	/**
	 * Does the real processing.
	 * This will most likely be called in a real-time context,
	 * which means blocking operations are forbidden.
	 *
	 * @param frames The number of frames to process
	 * @param ports Contains the inputs and outputs with up-to-date buffer locations
	 * @return true on success and false on error, in which case processing ends.
	 */
	virtual bool process(jack_nframes_t frames, const Ports &ports) = 0;

public:
	JackProcessor() { running_.test_and_set(); }
	/**
	 * Returns the sample rate.
	 * The rate is only non-zero if #needsSampleRate() returns true
	 * and after updateMetrics was executed successfully.
	 * @return the sample rate
	 */
	const jack_nframes_t getSampleRate() const { return metrics_.sampleRate; }
	/**
	 * Returns the jack buffer size.
	 * The size is only non-zero if #needsBufferSize() returns true
	 * and after updateMetrics was executed successfully.
	 * @return the buffer size.
	 *
	 */
	const jack_nframes_t getBufferSize() const { return metrics_.bufferSize; }
	/**
	 * returns whether the buffer size is relevant for this processor.
	 * If it is not, the buffer size will not be available to this
	 * processor and #getBufferSize() always returns 0.
	 * @return true is the buffer size is relevant for this processor, false otherwise.
	 */
	virtual bool needsBufferSize() const = 0;
	/**
	 * returns whether the sample rate is relevant for this processor.
	 * If it is not, the sample rate will not be available to this
	 * processor and #getSampleRate() always returns 0.
	 * @return true is the sample rate is relevant for this processor, false otherwise.
	 */
	virtual bool needsSampleRate() const = 0;

	bool updateMetrics(jack_client_t *client, ProcessingMetrics update)
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

	void reset()
	{
		lock guard(mutex_);
		Reset reset(this);
		onReset();
	}

	virtual ~JackProcessor()
	{
		if (ports_) {
			delete ports_;
			ports_ = nullptr;
		}
	}
};

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_JACKPROCESSOR_GUARD_H_ */

