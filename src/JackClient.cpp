/*
 * JackConnector.cpp
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

#include <regex>
#include <thread>
#include <iostream>
#include <simpledsp/Precondition.hpp>
#include <simpledsp/Guard.hpp>
#include <speakerman/jack/JackClient.hpp>

namespace speakerman {
namespace jack {

static void printError(const char *message) {
	std::cerr << "Error: " << message << std::endl;
}


int JackClient::rawProcess(jack_nframes_t nframes, void* arg) {
	try {
		JackClient& me = *((JackClient*) (arg));
		if (me.processor.process(nframes)) {
			return 0;
		}
	}
	catch(const char *msg) {
		std::cerr << "rawProcess: Exception: " << msg << std::endl;
	}
	catch (const std::exception &e) {
		std::cerr << "rawProcess: Exception: " << e.what() << std::endl;
	}
	catch (...) {
		std::cerr << "rawProcess: Exception (last-resort-catch)" << std::endl;
	}
	return 1;
}

void JackClient::rawShutdown(void* arg) {
	try {
		JackClient& me = *((JackClient*) (arg));
		me.processor.shutdownByServer();
		me.shutdownByServer();
	}
	catch(const char *msg) {
		std::cerr << "rawShutdown: Exception: " << msg << std::endl;
	}
	catch (const std::exception &e) {
		std::cerr << "rawShutdown: Exception: " << e.what() << std::endl;
	}
	catch (...) {
		std::cerr << "rawShutdown: Exception (last-resort-catch)" << std::endl;
	}
}

int JackClient::rawSetSampleRate(jack_nframes_t nframes, void* arg) {
	try {
		JackClient& me = *((JackClient*) (arg));
		if (me.processor.setSampleRate(nframes)) {
			return 0;
		}
	}
	catch(const char *msg) {
		std::cerr << "rawSetSampleRate: Exception: " << msg << std::endl;
	}
	catch (const std::exception &e) {
		std::cerr << "rawSetSampleRate: Exception: " << e.what() << std::endl;
	}
	catch (...) {
		std::cerr << "rawSetSampleRate: Exception (last-resort-catch)" << std::endl;
	}
	return 1;
}



void JackClient::shutdownByServer()
{
	close();
}

void JackClient::checkCanAddIO()
{
	if (state != ClientState::INITIAL && state != ClientState::DEFINED_PORTS) {
		throw std::runtime_error("Cannot only define ports in initial state");
	}
}

JackClient::JackClient(string n, JackProcessor &p) : name(n), processor(p)
{
}

void JackClient::activate()
{
	Guard g(m);
	if (state != ClientState::REGISTERED) {
		throw std::runtime_error("Cannot activate: invalid state");
	}
	if (client.useClient<bool>([](jack_client_t *c) {
		return jack_activate(c) != 0 ;
	})) {
		close();
		throw std::runtime_error("Couldn't activate jack client: an error occurred");
	}

	state = ClientState::ACTIVE;

	processor.prepareActivate();
}
signed JackClient::staticClosePorts(JackClient &self, jack_client_t *client)
{
	return self.unsafeClosePorts(client);
}
signed JackClient::unsafeClosePorts(jack_client_t *client)
{
	processor.connectPorts(client, true, true);
}
signed JackClient::connectPorts(bool disconnectPreviousOutputs, bool disconnectPreviousInputs)
{
	Guard g(m);
	if (state != ClientState::ACTIVE) {
		throw std::runtime_error("Cannot connect ports: not activated");
	}
	return client.useClient<signed,JackClient>(*this, staticClosePorts);
}

void JackClient::deactivate()
{
	Guard g(m);
	if (state != ClientState::ACTIVE) {
		throw std::runtime_error("Cannot deactivate: not activated");
	}
	for (int attempt = 0; attempt < 10; attempt++) {
		if (client.useClient<bool>([](jack_client_t *c) {return jack_deactivate(c) == 0;}))
		{
			processor.prepareDeactivate();
			return ;
		}
		std::this_thread::yield();
	}
	close();
	throw std::runtime_error("Couldn't deactivate jack client: an error occurred");
}
bool JackClient::staticClose(JackClient &self, jack_client_t *client)
{
	self.unsafeClose(client);
}
bool JackClient::unsafeClose(jack_client_t *jackClient)
{
	processor.unRegisterPorts(jackClient);
	state = ClientState::CLOSED;
	return true;
}
void JackClient::close()
{
	Guard g(m);
	client.disconnect<JackClient>(staticClose, *this);
}
JackClient::~JackClient()
{
	Guard g(m);
	if (state == ClientState::ACTIVE) {
		deactivate();
	}
	close();
}

} /* End of namespace jack */
} /* End of namespace speakerman */
