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

#include <jack/jack.h>
#include <jack/types.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <string>
#include <iostream>
#include "Port.hpp"
#include <speakerman/jack/JackProcessor.hpp>

namespace speakerman {

using namespace std;
using namespace tdap;
enum class ClientState { NONE, CLOSED, OPEN, CONFIGURED, ACTIVE, SHUTTING_DOWN };

static const char * client_state_name(ClientState state)
{
	switch (state) {
	case ClientState::CLOSED :
		return "CLOSED";
	case ClientState::CONFIGURED :
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

static bool client_state_defined(ClientState state)
{
	switch (state) {
	case ClientState::CLOSED :
	case ClientState::CONFIGURED :
	case ClientState::OPEN:
	case ClientState::ACTIVE:
	case ClientState::SHUTTING_DOWN:
		return true;
	default:
		return false;
	}
}

static bool client_state_is_shutdown_state(ClientState state)
{
	switch (state) {
	case ClientState::CLOSED :
	case ClientState::SHUTTING_DOWN:
		return true;
	default:
		return false;
	}

}


struct ShutDownInfo
{
	jack_status_t status;
	const char *reason;
	bool isSet;

	static constexpr ShutDownInfo empty()
	{
		return { static_cast<jack_status_t>(0), nullptr, false };
	}
	static ShutDownInfo withReason(const char *reason)
	{
		return { static_cast<jack_status_t>(0), reason, true };
	}
	static ShutDownInfo withReasonAndCode(jack_status_t code, const char *reason)
	{
		return { code, reason, true };
	}
	bool isEmpty() { return !isSet; }
};

class JackClient
{
	static thread_local jack_status_t lastState;
	ClientState state_ = ClientState::CLOSED;
	mutex mutex_;
	condition_variable awaitShutdownCondition_;
	thread awaitShutdownThread_;
	bool awaitShutdownThreadRunning_ = false;
	volatile bool shutDownOnSignal = false;

	jack_client_t *client_ = nullptr;
	ShutDownInfo shutdownInfo_ {static_cast<jack_status_t>(0), nullptr, false};
	string name_;
	JackProcessor *processor_ = nullptr;
	ProcessingMetrics metrics_;

	static void awaitShutdownCaller(JackClient *client)
	{
		client->awaitShutDownAndClose();
	}

	static void jackShutdownCallback(void * client)
	{
		if (client) {
			static_cast<JackClient*>(client)->onShutdown(
					ShutDownInfo::withReason("jack_on_shutdown"));
		}
	}

	static void jackInfoShutdownCallback(jack_status_t code, const char * reason, void * client)
	{
		if (client) {
			static_cast<JackClient*>(client)->onShutdown({code, reason, true});
		}
	}

	static int jackBufferSizeCallback(jack_nframes_t frames, void * client)
	{
		return client ? static_cast<JackClient*>(client)->onBufferSizeChange(frames) : 1;
	}

	static int jackSampleRateCallback(jack_nframes_t rate, void * client)
	{
		return client ? static_cast<JackClient*>(client)->onSampleRateChange(rate) : 1;
	}

	void registerCallbacks()
	{
		jack_on_shutdown(client_, jackShutdownCallback, this);
		jack_on_info_shutdown(client_, jackInfoShutdownCallback, this);
	}

	void awaitShutdownAndCloseUnsafe(unique_lock<mutex> &lock)
	{
		cout << "Await shutdown..." << endl;
		while (!client_state_is_shutdown_state(state_)) {
			awaitShutdownCondition_.wait(lock);
			cout << "Wakeup!..." << endl;
		}
		closeUnsafe();
	}

	void awaitShutDownAndClose()
	{
		cout << "Start closer thread" << endl;
		unique_lock<mutex> lock(mutex_);
		try {
			awaitShutdownThreadRunning_ = true;
			cout << "Closer thread awaits..." << endl;
			awaitShutdownAndCloseUnsafe(lock);
			awaitShutdownThreadRunning_ = false;
			awaitShutdownCondition_.notify_all();
		}
		catch (const std::exception &e) {
			cerr << "Encountered exception in shutdown-thread of \"" << name_ << "\": " << e.what() << endl;
		}
		catch (...) {
			awaitShutdownThreadRunning_ = false;
			awaitShutdownCondition_.notify_all();
			throw;
		}
	}

	bool notifyShutdownUnsafe(ShutDownInfo info, unique_lock<mutex> &lock)
	{
		if (!client_state_is_shutdown_state(state_)) {
			shutdownInfo_ = info;
			state_ = ClientState::SHUTTING_DOWN;
			awaitShutdownCondition_.notify_all();
			return true;
		}
		cout << "Ignore notify: already closed" << endl;
		return false;
	}

	void onShutdown(ShutDownInfo info)
	{
		unique_lock<mutex> lock(mutex_);
		notifyShutdownUnsafe(info, lock);
	}

	void closeUnsafe()
	{
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
	}

protected:
	virtual void registerAdditionalCallbacks(jack_client_t *client) {}

	JackClient(jack_client_t *client) :
		client_(client)
	{
		awaitShutdownThread_ = thread(awaitShutdownCaller, this);
		awaitShutdownThread_.detach();
		name_ = jack_get_client_name(client_);
		state_ = ClientState::OPEN;
		registerCallbacks();
		registerAdditionalCallbacks(client_);
	}

	int onMetricsUpdate(ProcessingMetrics m)
	{
		unique_lock<mutex> lock(mutex_);
		try {
			ProcessingMetrics update = metrics_.mergeWithUpdate(m);
			if (processor_->updateMetrics(client_, update)) {
				metrics_ = update;
				return 0;
			}
		}
		catch (const std::exception &e) {
			std:cerr << "Exception in onBufferSizeChange: " << e.what() << std::endl;
		}
		return 1;
	}

	int onSampleRateChange(jack_nframes_t rate)
	{
		return onMetricsUpdate({rate, 0});
	}

	int onBufferSizeChange(jack_nframes_t size)
	{
		return onMetricsUpdate({0, size});
	}


public:

	template<typename ...A>
	static JackClient * create(const char *serverName, jack_options_t options, A... args)
	{
		lastState = (jack_status_t)0;
		ErrorHandler::clear_ensure();
		return new JackClient(
				ErrorHandler::checkNotNullOrThrow(
						jack_client_open(serverName, options, &lastState, args...),
						"Create Jack Client"));
	}

	static JackClient * createDefault(const char *serverName)
	{
		lastState = (jack_status_t)0;
		ErrorHandler::clear_ensure();
		return new JackClient(
				ErrorHandler::checkNotNullOrThrow(
						jack_client_open(serverName, JackOptions::JackNullOption, &lastState),
						"Create Jack client"));
	}

	static jack_status_t getLastStatus()
	{
		jack_status_t result = lastState;
		lastState = (jack_status_t)0;
		return result;
	}

	const string &name() const
	{
		return name_;
	}

	bool setProcessor(JackProcessor &processor)
	{
		{
			unique_lock<mutex> lock(mutex_);
			if (state_ != ClientState::OPEN) {
				throw runtime_error("setProcessor: Not in OPEN state");
			}
			processor_ = &processor;
		}
		try {
			if (processor_->needBufferSizeCallback()) {
				ErrorHandler::checkZeroOrThrow(
						jack_set_buffer_size_callback(client_, jackBufferSizeCallback, this),
						"Set buffer size callback");
			}
			if (processor_->needSampleRateCallback()) {
				ErrorHandler::checkZeroOrThrow(
						jack_set_sample_rate_callback(client_, jackSampleRateCallback, this),
						"Set sample rate callback");
			}
			{
				unique_lock<mutex> lock(mutex_);
				state_ = ClientState::CONFIGURED;
				return true;
			}
		}
		catch(const std::exception &e) {
			std::cout << "Wrong" << e.what() << std::endl;
			{
				unique_lock<mutex> lock(mutex_);
				processor_ = nullptr;
			}
			throw e;
		}
	}

	void setActive()
	{
		unique_lock<mutex> lock(mutex_);
		if (state_ != ClientState::CONFIGURED) {
			throw runtime_error("setProcessor: Not in CONFIGURED state");
		}

		ErrorHandler::checkZeroOrThrow(jack_activate(client_), "Activating");
		state_ = ClientState::ACTIVE;
	}

	ClientState getState()
	{
		unique_lock<mutex> lock(mutex_);
		return state_;
	}

	void notifyShutdown(const char * reason)
	{
		onShutdown(ShutDownInfo::withReason(reason));
	}

	ShutDownInfo awaitClose()
	{
		unique_lock<mutex> lock(mutex_);
		if (state_ != ClientState::ACTIVE) {
			throw runtime_error("setProcessor: Not in ACTIVE state");
		}
		awaitShutdownAndCloseUnsafe(lock);
		return shutdownInfo_;
	}

	ShutDownInfo close()
	{
		unique_lock<mutex> lock(mutex_);
		if (notifyShutdownUnsafe(ShutDownInfo::withReason("Explicit close"), lock)) {
			closeUnsafe();
			while (awaitShutdownThreadRunning_) {
				cout << "Wait for terminate thread to end..."<< endl;
				awaitShutdownCondition_.wait(lock);
				awaitShutdownCondition_.notify_all();
			}
			cout << "Wait for terminate thread to end -- done!"<< endl;
			return shutdownInfo_;
		}
		else {
			return ShutDownInfo::withReason("Already closing");
		}
	}

	~JackClient()
	{
		cout << "destructor" << endl;
		close();
		cout << "destructor -- done!" << endl;
	}
};


} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_JACKCLIENT_GUARD_H_ */

