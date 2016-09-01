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

#include <jack/types.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include "Port.hpp"
#include <speakerman/jack/JackProcessor.hpp>

namespace speakerman {

using namespace std;
using namespace tdap;
enum class ClientState { NONE, CLOSED, OPEN, CONFIGURED, ACTIVE, SHUTTING_DOWN };

const char * client_state_name(ClientState state);
bool client_state_defined(ClientState state);
bool client_state_is_shutdown_state(ClientState state);

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

class JackClient;

struct CreateClientResult
{
	JackClient * client;
	jack_status_t status;
	const char * name;

	bool success() { return (client); }

	JackClient * getClient()
	{
		if (success()) {
			return client;
		}
		throw std::runtime_error("No jack client created");
	}
};

class JackClient
{
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
	long xRuns;
	long long lastXrunProcessingCycle;

	static void awaitShutdownCaller(JackClient* client);

	static void jackShutdownCallback(void* client);

	static void jackInfoShutdownCallback(jack_status_t code, const char* reason, void* client);

	static int jackBufferSizeCallback(jack_nframes_t frames, void* client);

	static int jackSampleRateCallback(jack_nframes_t rate, void* client);

	static int jackXrunCallback(void* client);

	void registerCallbacks();

	void awaitShutdownAndCloseUnsafe(unique_lock<mutex>& lock);

	void awaitShutDownAndClose();

	bool notifyShutdownUnsafe(ShutDownInfo info, unique_lock<mutex>& lock);

	void onShutdown(ShutDownInfo info);

	void closeUnsafe();

	static void jack_portnames_free(const char** names);

protected:
	virtual void registerAdditionalCallbacks(jack_client_t* client);

	JackClient(jack_client_t* client);

	int onMetricsUpdate(ProcessingMetrics m);

	int onSampleRateChange(jack_nframes_t rate);

	int onBufferSizeChange(jack_nframes_t size);


public:

	virtual int onXRun();

	template<typename ...A>
	static CreateClientResult create(const char *serverName, jack_options_t options, A... args)
	{
		jack_status_t lastState = static_cast<JackStatus>(0);
		for (int i = 1; i <= 10; i ++) {
			jack_client_t *c = jack_client_open(serverName, options, &lastState, args...);
			if (c) {
				return { new JackClient(c), static_cast<JackStatus>(0), serverName };
			}
			std::cerr << "JackClient::create() attempt " << i << " failed with status " << lastState << std::endl;
		}
		return { nullptr, lastState, serverName};
	}

	static CreateClientResult createDefault(const char *serverName)
	{
		jack_status_t lastState = static_cast<JackStatus>(0);
		for (int i = 1; i <= 10; i ++) {
			jack_client_t *c = jack_client_open(serverName, JackOptions::JackNullOption, &lastState);
			if (c) {
				return { new JackClient(c), static_cast<JackStatus>(0), serverName };
			}
			std::cerr << "JackClient::createDefault() attempt " << i << " failed with status " << lastState << std::endl;
		}
		return { nullptr, lastState, serverName};
	}

	const string& name() const;

	bool setProcessor(JackProcessor& processor);

	void setActive();

	ClientState getState();

	void notifyShutdown(const char* reason);

	ShutDownInfo awaitClose();

	static PortNames portNames(jack_client_t* client, const char* namePattern,
			const char* typePattern, unsigned long flags);

	PortNames portNames(const char* namePattern, const char* typePattern,
			unsigned long flags);

	ShutDownInfo close();

	virtual ~JackClient();
};


} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_JACKCLIENT_GUARD_H_ */

