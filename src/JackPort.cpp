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
#include <speakerman/JackPort.hpp>
#include <simpledsp/Values.hpp>

namespace speakerman {

using namespace std;

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
		string message = "Port not registered (yet): " + name;
		throw std::runtime_error(message);
	}
	return (jack_default_audio_sample_t *)jack_port_get_buffer(port, frames);
}

} /* End of namespace speakerman */
