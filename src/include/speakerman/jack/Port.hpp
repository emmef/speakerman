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
#include <iostream>

#include <jack/jack.h>
#include <jack/types.h>

#include <tdap/Array.hpp>
#include <tdap/Value.hpp>
#include "ErrorHandler.hpp"
#include "Names.hpp"
#include "PortDefinition.hpp"

namespace speakerman {

using namespace tdap;

class PortNames;

class Port
{
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
	create_port(
			jack_client_t *client,
			PortDefinition::Data definition)
	{
		ErrorHandler::clear_ensure();
		jack_port_t * port = jack_port_register(client, definition.name, definition.type(), definition.flags(), 0);
		return ErrorHandler::checkNotNullOrThrow(port, "Failed to register port");
	}

	static jack_port_t *
	create_port(
			jack_client_t *client,
			PortDefinition definition)
	{
		return create_port(client, definition.data);
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

class Ports;

class Ports
{
	struct PortData
	{
		jack_port_t *port = nullptr;
		RefArray<jack_default_audio_sample_t> buffer;
	};

	PortDefinitions definitions_;
	Array<PortData> ports_;
	volatile bool registered_;

	static NameListPolicy &nameListPolicy()
	{
		static NameListPolicy policy;
		return policy;
	}

	void unregister(jack_client_t * client, size_t limit)
	{
		size_t bound = Value<size_t>::min(limit, ports_.size());

		int i;
		try {
			for (i = 0; i < bound; i++) {
				ErrorHandler::setForceLogNext();
				jack_port_t *port = ports_[i].port;
				ports_[i].port = nullptr;
				Port::try_unregister_port(client, port);
			}
		}
		catch (...) {
			for (; i < bound; i++) {
				ports_[i].port = nullptr;
				ports_[i].buffer.reset();
			}
			throw;
		}
	}

	size_t portCountInDirection(PortDirection dir) const
	{
		size_t count = 0;
		for (size_t i = 0; i < portCount(); i++) {
			if (definitions_[i].direction == dir) {
				count++;
			}
		}
		return count;

	}

	size_t totalPortNameLengthInDirection(PortDirection dir) const
	{
		size_t count = 0;
		for (size_t i = 0; i < portCount(); i++) {
			if (definitions_[i].direction == dir) {
				count += strlen(portName(i));
			}
		}

		return count;
	}

	NameList portsInDirection(PortDirection dir) const
	{
		size_t nameLength = totalPortNameLengthInDirection(dir);
		size_t count = portCountInDirection(dir);
		NameList list(nameListPolicy(), count, nameLength + count);
		for (size_t i = 0; i < portCount(); i++) {
			if (definitions_[i].direction == dir) {
				list.add(portName(i));
			}
		}
		return list;
	}

public:

	Ports(const PortDefinitions &definitions) :
		definitions_(definitions),
		ports_(Value<size_t>::max(1, definitions_.portCount()), definitions_.portCount()), registered_(false)
	{
	}

	const PortDefinitions &definitions() const
	{
		return definitions_;
	}

	size_t portCount() const
	{
		return ports_.size();
	}

	size_t inputCount() const
	{
		return portCountInDirection(PortDirection::IN);
	}

	size_t outputCount() const
	{
		return portCountInDirection(PortDirection::OUT);
	}

	NameList inputNames() const
	{
		return portsInDirection(PortDirection::IN);
	}

	NameList outputNames() const
	{
		return portsInDirection(PortDirection::OUT);
	}

	const char * portName(size_t i) const
	{
		if (i < portCount()) {
			if (registered_) {
				return jack_port_name(ports_[i].port);
			}
			else {
				return definitions_[i].name;
			}
		}
		throw std::invalid_argument("Port name index too high");
	}

	void getBuffers(jack_nframes_t frames)
	{
		for (size_t i = 0; i < ports_.size(); i++) {
			ports_[i].buffer = Port::get_buffer(ports_[i].port, frames);
		}
	}

	RefArray<jack_default_audio_sample_t> getBuffer(size_t i) const
	{
		return ports_[i].buffer;
	}

	void registerPorts(jack_client_t *client)
	{
		size_t i;
		try {
			for (i = 0; i < ports_.size(); i++) {
				PortDefinition::Data data = definitions_[i];
				ports_[i].port = Port::create_port(client, data);
			}
			registered_ = true;
		}
		catch (const std::exception &e) {
			unregister(client, i);
			throw e;
		}
	}

	void unregisterPorts(jack_client_t *client)
	{
		registered_ = false;
		unregister(client, ports_.size());
	}
};


} /* End of namespace speakerman */


#endif /* SMS_SPEAKERMAN_PORT_GUARD_H_ */

