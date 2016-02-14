/*
 * Port.hpp
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

#ifndef SMS_SPEAKERMAN_PORT_GUARD_H_
#define SMS_SPEAKERMAN_PORT_GUARD_H_

#include <string>

#include <jack/jack.h>
#include <jack/types.h>

#include <tdap/Array.hpp>

#include "ErrorHandler.hpp"

namespace speakerman {
namespace jack {

using namespace tdap;

enum class PortDirection
{
	IN, OUT
};

static const char * const port_direction_name(PortDirection direction)
{
	switch (direction) {
		case PortDirection::IN:
			return "IN";
		case PortDirection::OUT:
			return "OUT";
		default:
			return "<INVALID>";
	}
}

class PortNames;

class Ports
{
	static constexpr unsigned long FLAGS_INPUT = JackPortFlags::JackPortIsInput;
	static constexpr unsigned long FLAGS_INPUT_TERMINAL = JackPortFlags::JackPortIsInput | JackPortFlags::JackPortIsTerminal;
	static constexpr unsigned long FLAGS_OUTPUT = JackPortFlags::JackPortIsOutput;
	static constexpr unsigned long FLAGS_OUTPUT_TERMINAL = JackPortFlags::JackPortIsOutput | JackPortFlags::JackPortIsTerminal;

	static jack_port_t *
	create_port(
			jack_client_t *client,
			const char * const name,
			unsigned long int flags)
	{
		ErrorHandler::clear_ensure();
		jack_port_t * port = jack_port_register(client, name, JACK_DEFAULT_AUDIO_TYPE, flags, 0);
		return ErrorHandler::checkNotNullOrThrow(port, "Failed to register port");
	}

	static int disconnect_port_internal(jack_client_t *client, jack_port_t *port, const char *target, bool throw_if_disconnect_fails)
	{
		ErrorHandler::clear_ensure();
		const char * name = jack_port_name(port);
		ErrorHandler::checkNotNullOrThrow(name, "Could not obtain port name");
		unsigned long flags = jack_port_flags(port);
		if (flags & JackPortFlags::JackPortIsInput) {
			return jack_disconnect(client, target, name);
		}
		else if (flags & JackPortFlags::JackPortIsInput) {
			return jack_disconnect(client, name, target);
		}
		else {
			throw std::runtime_error("Port must be input or output");
		}
	}

	static int connect_port_internal(jack_client_t *client, jack_port_t *port, const char *target, bool throw_if_disconnect_fails)
	{
		ErrorHandler::clear_ensure();
		const char * name = jack_port_name(port);
		ErrorHandler::checkNotNullOrThrow(name, "Could not obtain port name");
		unsigned long flags = jack_port_flags(port);
		if (flags & JackPortFlags::JackPortIsInput) {
			return jack_connect(client, target, name);
		}
		else if (flags & JackPortFlags::JackPortIsInput) {
			return jack_connect(client, name, target);
		}
		else {
			throw std::runtime_error("Port must be input or output");
		}
	}

public:
	struct BufferFaultResult
	{
		jack_port_t *port;
		jack_nframes_t frames;
	};

	static size_t max_port_name_length()
	{
		static size_t value = jack_port_name_size();
		return value;
	}

	static RefArray<jack_default_audio_sample_t> get_buffer(jack_port_t * port, jack_nframes_t frames)
	{
		if (port) {
			jack_default_audio_sample_t *buffer = (jack_default_audio_sample_t *)jack_port_get_buffer(port, frames);
			if (buffer) {
				return RefArray<jack_default_audio_sample_t>(buffer, frames);
			}
		}
		throw BufferFaultResult{port, frames};
	}

	static jack_port_t *
	create_input_port(
			jack_client_t *client,
			const char * const name,
			bool is_terminal = false)
	{
		return create_port(client, name, is_terminal ? FLAGS_INPUT_TERMINAL : FLAGS_INPUT);
	}

	static jack_port_t *
	create_output_port(
			jack_client_t *client,
			const char * const name,
			bool is_terminal = false)
	{
		return create_port(client, name, is_terminal ? FLAGS_OUTPUT_TERMINAL : FLAGS_OUTPUT);
	}

	static void connect_port(jack_client_t *client, jack_port_t * port, const char * target) {
		ErrorHandler::clear_ensure();
		ErrorHandler::checkZeroOrThrow(connect_port_internal(client, port, target, true), "Could not connect ports");
	}

	static bool try_connect_port(jack_client_t *client, jack_port_t * port, const char * target, int *result = nullptr) {
		ErrorHandler::get_message_clear();

		return ErrorHandler::returnIfZero(connect_port_internal(client, port, target, false), result);
	}

	static void connect_ports(jack_client_t *client, const char * output, const char * input) {
		ErrorHandler::clear_ensure();
		ErrorHandler::checkZeroOrThrow(jack_connect(client, output, input), "Could not connect ports");
	}

	static bool try_connect_ports(jack_client_t *client, const char * output, const char * input, int *result = nullptr) {
		ErrorHandler::get_message_clear();

		return ErrorHandler::returnIfZero(jack_connect(client, output, input), result);
	}

	static void disconnect_port_all(jack_client_t *client, jack_port_t *port)
	{
		ErrorHandler::get_message_clear();
		ErrorHandler::checkZeroOrThrow(jack_port_disconnect(client, port), "Failed to disconnect port");
	}

	static bool try_disconnect_port_all(jack_client_t *client, jack_port_t *port, int *result = nullptr)
	{
		ErrorHandler::get_message_clear();
		return ErrorHandler::returnIfZero(jack_port_disconnect(client, port), result);
	}

	static void disconnect_port(jack_client_t *client, jack_port_t *port, const char *target)
	{
		ErrorHandler::checkZeroOrThrow(disconnect_port_internal(client, port, target, true), "Could not disconnect port");
	}

	static bool try_disconnect_port(jack_client_t *client, jack_port_t *port, const char *target, int *result = nullptr)
	{
		return ErrorHandler::returnIfZero(disconnect_port_internal(client, port, target, false), result);
	}

	static void unregister_port(jack_client_t *client, jack_port_t *port)
	{
		ErrorHandler::checkZeroOrThrow(jack_port_unregister(client, port), "Could not unregister port");
	}

	static bool try_unregister_port(jack_client_t *client, jack_port_t *port, int * result = nullptr)
	{
		ErrorHandler::returnIfZero(jack_port_unregister(client, port), result);
	}
};

class Port
{
	jack_client_t *client_ = 0;
	jack_port_t *port_ = 0;
	PortDirection direction_ = PortDirection::IN;

	Port(jack_client_t *client, jack_port_t *port, PortDirection direction) : client_(client), port_(port), direction_(direction) {}

public:
	Port createInput(jack_client_t *client,
			const char * const name,
			bool is_terminal = false)
	{
		return Port(client, Ports::create_input_port(client, name, is_terminal), PortDirection::IN);
	}

	Port createOutput(jack_client_t *client,
			const char * const name,
			bool is_terminal = false)
	{
		return Port(client, Ports::create_output_port(client, name, is_terminal), PortDirection::OUT);
	}

	const char * getName()
	{
		return port_ ? jack_port_name(port_) : nullptr;
	}

	void rename(const char * name, bool is_terminal) {
		jack_port_t *newPort;
		if (direction_ == PortDirection::IN) {
			newPort = ErrorHandler::checkNotNullOrThrow(Ports::create_input_port(client_, name, is_terminal), "Cannot register port");
		}
		else {
			newPort = ErrorHandler::checkNotNullOrThrow(Ports::create_output_port(client_, name, is_terminal), "Cannot register port");
		}
		if (port_) {
			ErrorHandler::setForceLogNext();
			Ports::try_unregister_port(client_, port_);
		}
		port_ = newPort;
	}

	void connect(const char *target)
	{
		Ports::connect_port(client_, port_, target);
	}

	void tryConnect(const char *target, int * result = nullptr)
	{
		Ports::try_connect_port(client_, port_, target, result);
	}

	RefArray<jack_default_audio_sample_t> getBuffer(jack_nframes_t frames) const
	{
		return Ports::get_buffer(port_, frames);
	}

	bool connected() const
	{
		return port_ && jack_port_connected(port_) > 0;
	}

	bool connectedWith(const char * target) const
	{
		return port_ && jack_port_connected_to(port_, target) > 0;
	}

	int connectCount() const
	{
		return port_ ? jack_port_connected(port_) : 0;
	}

	void disconnectAll()
	{
		Ports::disconnect_port_all(client_, port_);
	}

	bool tryDisconnectAll(int *result = nullptr)
	{
		Ports::try_disconnect_port_all(client_, port_, result);
	}

	void disconnect(const char * target)
	{
		Ports::disconnect_port(client_, port_, target);
	}

	bool tryDisconnect(const char * target, int *result = nullptr)
	{
		Ports::try_disconnect_port(client_, port_, target, result);
	}

	void unregister()
	{
		Ports::unregister_port(client_, port_);
		port_ = nullptr;
	}

	bool tryUnregister(int * result = nullptr)
	{
		if (Ports::try_unregister_port(client_, port_, result)) {
			port_ = nullptr;
			return true;
		}
		return false;
	}

	~Port()
	{
		if (client_) {
			ErrorHandler::setForceLogNext();
			tryUnregister();
			client_ = nullptr;
		}
	}
};

class PortNames
{
	const char** const portNames;
	size_t count;
	friend class Ports;

	size_t rangeCheck(size_t index) const {
		if (index < count) {
			return index;
		}
		throw out_of_range("Port name index out of range");
	}

public:
	PortNames(jack_client_t* client, const char* namePattern,
			const char* typePattern, unsigned long flags) :
			portNames(jack_get_ports(client, namePattern, typePattern, flags))
	{
		count = 0;
		if (portNames != nullptr) {
			const char** name = portNames;
			while (name[count] != nullptr) {
				count++;
			}
		}
	}

	size_t length() const {
		return count;
	}
	const char* get(size_t idx) const {
		return portNames[rangeCheck(idx)];
	}
	const char* operator [](size_t idx) const {
		return get(idx);
	}
	~PortNames() {
		if (portNames != nullptr) {
			jack_free(portNames);
		}
	}
};


} /* End of namespace jack */
} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_PORT_GUARD_H_ */

