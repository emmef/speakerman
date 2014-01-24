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
#include <mutex>
#include <jack/jack.h>
#include <simpledsp/Guard.hpp>
#include <speakerman/jack/JackProcessor.hpp>
#include <speakerman/jack/ClientState.hpp>
#include <speakerman/jack/Connection.hpp>

namespace speakerman {
namespace jack {

using namespace simpledsp;

class JackClient
{
	recursive_mutex m;
	string name;
	Client client;
	ClientState state = ClientState::INITIAL;
	JackProcessor &processor;

	static int rawProcess(jack_nframes_t nframes, void* arg);
	static void rawShutdown(void* arg);
	static int rawSetSampleRate(jack_nframes_t nframes, void* arg);

	void checkCanAddIO();
	void shutdownByServer();
	static bool staticClose(JackClient &self, jack_client_t *client);
	bool unsafeClose(jack_client_t *client);

	static signed staticClosePorts(JackClient &self, jack_client_t *client);
	signed unsafeClosePorts(jack_client_t *client);

public:
	JackClient(string name, JackProcessor &processor);
	template<typename... Args> void open(JackOptions options, Args... args)
	{
		Guard g(m);

		switch (state) {
		case ClientState::INITIAL:
			if (processor.inputs.size() == 0 && processor.outputs.size() == 0) {
				throw std::runtime_error("Cannot open client: no ports defined");
			}
			std::cout << "Inputs: " << processor.inputs.size() << "; outputs: " << processor.outputs.size() << std::endl;
			/* no break */
		case ClientState::CLOSED:
		case ClientState::DEFINED_PORTS:
			client.connect(name, options, args...);
			state = ClientState::REGISTERED;
			break;
		case ClientState::REGISTERED:
			break;
		default:
			throw std::runtime_error("Cannot open client: invalid state");
		}
	}
	void activate();
	signed connectPorts(bool disconnectPreviousOutputs, bool disconnectPreviousInputs);
	void deactivate();
	void close();

	virtual ~JackClient();
};

} /* End of namespace jack */
} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_JACKCLIENT_GUARD_H_ */
