/*
 * JackPort.cpp
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

#include <stdexcept>
#include <iostream>
#include <speakerman/jack/JackPort.hpp>
#include <simpledsp/Values.hpp>

namespace speakerman {
namespace jack {

using namespace std;

JackPort::JackPort(Direction d, std::string n, std::string namePattern, std::string typePattern, unsigned long flags) :
		name(n), direction(d), connectNamePattern(namePattern), connectTypePattern(typePattern), connectFlags(flags)
{
}
JackPort::JackPort(Direction d, std::string n) :
		name(n), direction(d), connectNamePattern(""), connectTypePattern(""), connectFlags(0)
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
		port = jack_port_register(client, name.c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
		std::cout << "Port registered: " << name.c_str() << port << std::endl;
		break;
	case Direction::OUT:
		port = jack_port_register(client, name.c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
		std::cout << "Port registered: " << name.c_str() << port << std::endl;
		break;
	default:
		throw invalid_argument("Invalid port direction");
	}
	if (!port) {
		string message = "Could not open port " + name;
		std::cout << "Failure for " << name.c_str() << std::endl;
		throw runtime_error(message);
	}
}

void JackPort::deRegisterPort(jack_client_t *client)
{
	if (port) {
		if (jack_port_unregister(client, port) == 0) {
			std::cout << "Port de-registered: " << jack_port_name(port) << std::endl;
		}
		else {
			std::cerr<< "Port NOT de-registered: " << jack_port_name(port) << std::endl;
		}
		port = nullptr;
	}
}

signed JackPort::connect(jack_client_t *client, bool disconnectPrevious)
{
	bool hasNamePattern = connectNamePattern.length() > 0;
	bool hasTypePattern = connectTypePattern.length() > 0;
	if (hasNamePattern || hasTypePattern) {
		std:cout << "Connect port: " << name << std::endl;
		long flags = connectFlags;
		if (direction == Direction::IN) {
			flags &= -1 ^ JackPortIsInput;
		}
		else {
			flags &= -1 ^ JackPortIsOutput;
		}
		const char ** ports = jack_get_ports(client, hasNamePattern ? connectNamePattern.c_str() : "*", hasTypePattern ? connectTypePattern.c_str() : "*", connectFlags);
		if (!ports) {
			std::cerr << "No ports to match name=\"" << connectNamePattern << "\"; type=\"" << connectTypePattern << "\"; flags=" << flags << std::endl;
			return -1;
		}
		const char ** walk = ports;
		while (*walk) {
			std::cout << "Port candidate \"" << *walk << "\"" << std::endl;
			jack_free(const_cast<char *>(*walk));
			walk++;
		}
		jack_free(walk);
	}

	return 0;
}

jack_default_audio_sample_t * JackPort::getBuffer(jack_nframes_t frames)
{
	if (!port) {
		string message = "Port not registered (yet): " + name;
		throw std::runtime_error(message);
	}
	return (jack_default_audio_sample_t *)jack_port_get_buffer(port, frames);
}

} /* End of namespace jack */
} /* End of namespace speakerman */
