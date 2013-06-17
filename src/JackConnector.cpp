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
#include <speakerman/JackConnector.hpp>

namespace speakerman {

static void printError(const char *message) {
	std::cerr << "Error: " << message << std::endl;
}

JackPort::JackPort(std::string n, Direction d) :
		name(n), direction(d)
{
}
JackPort::~JackPort()
{
}

void JackPort::registerPort(jack_client_t *client)
{
	if (port) {
		throw std::runtime_error("Cannot connect while already connected");
	}
	switch(direction) {
	case Direction::IN:
		port = jack_port_register(client, name.c_str(), "input", JackPortIsInput, 0);
		std::cout << "Port registered: " << name.c_str() << port << std::endl;
		break;
	case Direction::OUT:
		port = jack_port_register(client, name.c_str(), "output", JackPortIsOutput, 0);
		std::cout << "Port registered: " << name.c_str() << port << std::endl;
		break;
	default:
		throw invalid_argument("Invalid port direction");
	}
	if (!port) {
		string message = "Could not open port " + name;
		std::cout << "Failure for " << name.c_str() << std::endl;
		throw std::runtime_error(message);
	}
}

void JackPort::deRegisterPort()
{
	if (port) {
		std::cout << "Port de-registered: " << jack_port_name(port) << std::endl;
		port = nullptr;
	}
}

jack_default_audio_sample_t * JackPort::getBuffer(jack_nframes_t frames)
{
	if (!port) {
		string message = "Port not registered yet: " + name;
		throw std::runtime_error(message);
	}
	return (jack_default_audio_sample_t *)jack_port_get_buffer(port, frames);
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
		if (jack_set_process_callback(client, rawProcess, this) != 0) {
			throw std::runtime_error("Couldn't set call-backs");
		}
		std::cout << "Installed call-backs " << jack_get_client_name(client) << std::endl;
		for (size_t i = 0 ; i < inputs.size(); i++) {
			inputs.get(i).registerPort(client);
		}
		for (size_t i = 0 ; i < outputs.size(); i++) {
			outputs.get(i).registerPort(client);
		}
		std::cout << "Registered ports " << jack_get_client_name(client) << std::endl;
		state = ClientState::REGISTERED;
	}
	catch (...) {
		close();
	}
}
const jack_default_audio_sample_t *JackClient::getInput(size_t number, jack_nframes_t frameCount) const
{
	return inputs.get(number).getBuffer(frameCount);
}

jack_default_audio_sample_t *JackClient::getOutput(size_t number, jack_nframes_t frameCount) const
{
	return outputs.get(number).getBuffer(frameCount);
}


JackClient::JackClient(string n) : name(n), inputs(10), outputs(10)
{
}

void JackClient::addInput(string name)
{
	Guard g = m.guard();
	checkCanAddIO();
	inputs.add(name, Direction::IN);
	state = ClientState::DEFINED_PORTS;
}

void JackClient::addOutput(string name)
{
	Guard g = m.guard();
	checkCanAddIO();
	outputs.add(name, Direction::OUT);
	state = ClientState::DEFINED_PORTS;
}

void JackClient::open()
{
	Guard g = m.guard();
	switch (state) {
	case ClientState::INITIAL:
		throw std::runtime_error("Cannot open client: no ports defined");
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

	prepareActivate();
}

void JackClient::deactivate()
{
	Guard g = m.guard();
	if (state != ClientState::ACTIVE) {
		throw std::runtime_error("Cannot deactivate: not activated");
	}
	for (int attempt = 0; attempt < 10; attempt++) {
		if (jack_deactivate(client) == 0) {
			prepareDeactivate();
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
	for (size_t i = 0 ; i < inputs.size(); i++) {
		inputs.get(i).deRegisterPort();
	}
	for (size_t i = 0 ; i < inputs.size(); i++) {
		inputs.get(i).deRegisterPort();
	}
	for (int attempt = 0; attempt < 10; attempt++) {
		if (jack_client_close(client) != 0) {
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
