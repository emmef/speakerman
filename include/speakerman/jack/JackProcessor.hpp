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
	jack_nframes_t rate;
	jack_nframes_t bufferSize;

	bool operator ==(const ProcessingMetrics &o) { return rate == o.rate && bufferSize == o.bufferSize; }

	static ProcessingMetrics withRate(jack_nframes_t r) { return { r, 0 }; }

	ProcessingMetrics withBufferSize(jack_nframes_t size) { return { rate, size }; }

	ProcessingMetrics mergeWithUpdate(ProcessingMetrics update)
	{
		return { update.rate ? update.rate : rate, update.bufferSize ? update.bufferSize : bufferSize };
	}
};


class JackProcessor
{
	Ports *ports_ = nullptr;
	ProcessingMetrics metrics;

	std::atomic_flag running_ = ATOMIC_FLAG_INIT;

	static int callback(jack_nframes_t frames, void *data)
	{
		if (data) {
			try {
				return static_cast<JackProcessor *>(data)->processWrapper(frames);
			}
			catch (const std::exception e) {
				return 1;
			}
		}
		return 1;
	}

	int processWrapper(jack_nframes_t frames)
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

		onPortsRegistered();

		ErrorHandler::checkZeroOrThrow(jack_set_process_callback(client, callback, this), "Setting callback");
	}

protected:
	virtual const PortDefinitions &getDefinitions() =  0;
	virtual bool onMetricsUpdate(ProcessingMetrics metrics) = 0;
	virtual void onPortsRegistered() = 0;
	virtual bool process(jack_nframes_t frames, const Ports &ports) = 0;

public:
	const jack_nframes_t getRate() const { return metrics.rate; }
	const jack_nframes_t getBufferSize() const { return metrics.bufferSize; }
	virtual bool needBufferSizeCallback() const = 0;
	virtual bool needSampleRateCallback() const = 0;

	const Ports &ports() const
	{
		return *ErrorHandler::checkNotNullOrThrow(ports_, "Ports initialized");
	}

	bool updateMetrics(jack_client_t *client, ProcessingMetrics update)
	{
		bool rateConditionMet =
				!needSampleRateCallback() ||
				(update.rate != 0 && update.rate != metrics.rate);
		bool bufferSizeConditionMet =
				!needBufferSizeCallback() ||
				(update.bufferSize != 0 && update.bufferSize != metrics.bufferSize);

		if (rateConditionMet && bufferSizeConditionMet) {
			if (onMetricsUpdate(update)) {
				if (metrics == ProcessingMetrics::withRate(0).withBufferSize(0)) {
					ensurePorts(client);
				}
				metrics = update;
				return true;
			}
			return false;
		}
		return true;
	}

	~JackProcessor()
	{
		if (ports_) {
			delete ports_;
			ports_ = nullptr;
		}
	}
};

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_JACKPROCESSOR_GUARD_H_ */

