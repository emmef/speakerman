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
#include <speakerman/JackClient.hpp>

namespace speakerman {

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
void JackClient::unsafeOpen()
{
	jack_status_t returnStatus;
	client = jack_client_open("speaker-management", JackNullOption, &returnStatus);
	if (client == 0) {
		throw std::runtime_error("Couldn't open connection to jack");
	}
	if (returnStatus != 0) {
		throw std::runtime_error("Invalid return state when opening client");
	}
	std::cout << "Opened client " << jack_get_client_name(client) << std::endl;
	jack_on_shutdown(client, rawShutdown, this);
	jack_set_error_function(printError);
	try {
		if (jack_set_sample_rate_callback(client, rawSetSampleRate, this) != 0) {
			throw std::runtime_error("Couldn't set sample-rate call-back");
		}
		if (jack_set_process_callback(client, rawProcess, this) != 0) {
			throw std::runtime_error("Couldn't set processing call-back");
		}
		std::cout << "Installed processing call-backs " << jack_get_client_name(client) << std::endl;
		processor.registerPorts(client);
		std::cout << "Registered ports " << jack_get_client_name(client) << std::endl;
		state = ClientState::REGISTERED;
	}
	catch (...) {
		close();
	}
}


JackClient::JackClient(string n, JackProcessor &p) : name(n), processor(p)
{
}

void JackClient::open()
{
	Guard g = m.guard();
	switch (state) {
	case ClientState::INITIAL:
		if (processor.inputs.size() == 0 && processor.outputs.size() == 0) {
			throw std::runtime_error("Cannot open client: no ports defined");
		}
		std::cout << "Inputs: " << processor.inputs.size() << "; outputs: " << processor.outputs.size() << std::endl;
		/* no break */
	case ClientState::CLOSED:
	case ClientState::DEFINED_PORTS:
		unsafeOpen();
		break;
	case ClientState::REGISTERED:
		// Nothing to do
		return;
	default:
		throw std::runtime_error("Cannot open client: invalid state");
	}
}

void JackClient::activate()
{
	Guard g = m.guard();
	if (state != ClientState::REGISTERED) {
		throw std::runtime_error("Cannot activate: invalid state");
	}

	if (jack_activate(client) != 0) {
		close();
		throw std::runtime_error("Couldn't activate jack client: an error occurred");
	}

	state = ClientState::ACTIVE;

	processor.prepareActivate();
}

signed JackClient::connectPorts(bool disconnectPreviousOutputs, bool disconnectPreviousInputs)
{
	Guard g = m.guard();
	if (state != ClientState::ACTIVE) {
		throw std::runtime_error("Cannot connect ports: not activated");
	}
	return processor.connectPorts(client, disconnectPreviousOutputs, disconnectPreviousInputs);
}

void JackClient::deactivate()
{
	Guard g = m.guard();
	if (state != ClientState::ACTIVE) {
		throw std::runtime_error("Cannot deactivate: not activated");
	}
	for (int attempt = 0; attempt < 10; attempt++) {
		if (jack_deactivate(client) == 0) {
			processor.prepareDeactivate();
			return ;
		}
		std::this_thread::yield();
	}
	close();
	throw std::runtime_error("Couldn't deactivate jack client: an error occurred");
}

void JackClient::close()
{
	Guard g = m.guard();
	if (client) {
		std::cout << "Unregister ports" << std::endl;
		processor.unRegisterPorts(client);
		std::cout << "Closing jack client " << name << std::endl;
		for (int attempt = 0; attempt < 10; attempt++) {
			if (jack_client_close(client) == 0) {
				break;
			}
			std::this_thread::yield();
		}
	}
	client = nullptr;
	state = ClientState::CLOSED;
}
JackClient::~JackClient()
{
	Guard g = m.guard();
	if (state == ClientState::ACTIVE) {
		deactivate();
	}
	close();
}



} /* End of namespace speakerman */
