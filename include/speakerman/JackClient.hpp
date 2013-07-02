/*
 * JackClient.hpp
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

#ifndef SMS_SPEAKERMAN_JACKCLIENT_GUARD_H_
#define SMS_SPEAKERMAN_JACKCLIENT_GUARD_H_

#include <string>
#include <atomic>
#include <jack/jack.h>
#include <simpledsp/List.hpp>
#include <speakerman/utils/Mutex.hpp>
#include <speakerman/JackProcessor.hpp>

namespace speakerman {

using namespace simpledsp;

enum class ClientState
{
	INITIAL,
	CLOSED,
	DEFINED_PORTS,
	REGISTERED,
	ACTIVE
};

class JackClient
{
	speakerman::Mutex m;
	string name;
	jack_client_t *client = nullptr;
	ClientState state = ClientState::INITIAL;
	JackProcessor &processor;

	static int rawProcess(jack_nframes_t nframes, void* arg);
	static void rawShutdown(void* arg);
	static int rawSetSampleRate(jack_nframes_t nframes, void* arg);

	void checkCanAddIO();
	void shutdownByServer();
	void unsafeOpen();

protected:

public:
	JackClient(string name, JackProcessor &processor);
	void open();
	void activate();
	signed connectPorts(bool disconnectPreviousOutputs, bool disconnectPreviousInputs);
	void deactivate();
	void close();

	virtual ~JackClient();
};

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_JACKCLIENT_GUARD_H_ */
