/*
 * Client.cpp
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

#include <regex>
#include <iostream>
#include <locale>
#include <chrono>

#include <simpledsp/Precondition.hpp>
#include <simpledsp/Alloc.hpp>
#include <simpledsp/MemoryFence.hpp>
#include <speakerman/jack/Client.hpp>

namespace speakerman {
namespace jack {

using namespace std::chrono;

static string NONAME = "";

static thread_local const char * lastErrorMessage = nullptr;
static bool errorCallBackSet = false;

void Client::errorMessageHandler(const char * message)
{
	lastErrorMessage = message;
	cout << "Received error message from jack: " << message << endl;
}

void Client::ensureJackErrorMessageHandler()
{
	static recursive_mutex m;

	if (errorCallBackSet) {
		return;
	}
	CriticalScope g(m);
	if (!errorCallBackSet) {
		cerr << "Setting jack error handler" << endl;
		jack_set_error_function(errorMessageHandler);
		errorCallBackSet = true;
	}
}

const char * Client::getAndResetErrorMessage()
{
	const char * message = lastErrorMessage;
	lastErrorMessage = nullptr;
	return message;
}

void Client::throwOnErrorMessage(const char* description) {
	const char* message = Client::getAndResetErrorMessage();
	if (message == nullptr) {
		return;
	}
	if (description == nullptr || *description == '\0') {
		throw runtime_error(message);
	}
	string exceptionMessage = description;
	exceptionMessage += ": ";
	exceptionMessage += message;
	throw runtime_error(exceptionMessage);
}


Client::PortEntry::PortEntry() :
					givenName(NONAME),
					dir(PortDirection::IN),
					actualName(NONAME),
					port(nullptr),
					buffer(nullptr) {}

inline PortNames::PortNames(jack_client_t* client, const char* namePattern,
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

size_t PortNames::rangeCheck(size_t index) const
{
	if (index < count) {
		return index;
	}
	throw out_of_range("Port name index out of range");
}

size_t PortNames::length() const
{
	return count;
}

const char* PortNames::get(size_t idx) const
{
	return portNames[rangeCheck(idx)];
}

const char* PortNames::operator [](size_t idx) const
{
	return get(idx);
}
PortNames::~PortNames()
{
	if (portNames != nullptr) {
		jack_free(portNames);
	}
}

//class MessageServer
//{
//	std::mutex lock;
//	std::condition_variable condition;
//	std::queue<Client::ClientMessage> queue;
//	volatile bool shutdown = false;
//
//	void serveMessages();
//
//public:
//	MessageServer();
//
//	void sendClientMessage(ClientMessage message);
//	~MessageServer();
//};


PortNames * Client::getPortNames(const char * namePattern, const char * typePattern, unsigned long flags)
{
	return new PortNames(client, namePattern, typePattern, flags);
}

void Client::serveMessagesForClient(Client *client)
{
	client->serveMessages();
}

void Client::serveMessages()
{
	while (!messageShutdown) {
		ClientMessageType type = ClientMessageType::NONE;
		{
			unique_lock<mutex> guard(messageMutex);
			if (messageQueue.empty()) {
				messageCondition.wait_for(guard, std::chrono::milliseconds{200});
			}
			else {
				type = messageQueue.front();
				messageQueue.pop();
			}
		}
		if (type != ClientMessageType::NONE) {
			executeMessage(type);
		}
	}
}

void Client::sendMessage(ClientMessageType type)
{
	if (type == ClientMessageType::DESTRUCTION) {
		messageShutdown = true;
	}
	unique_lock<mutex> guard(messageMutex);
	messageQueue.push(type);
	messageCondition.notify_all();
}

void Client::executeMessage(ClientMessageType type)
{
	if (type == ClientMessageType::SERVER_SHUTDOWN) {
		CriticalScope g(mutex);

		unsafeClose();
	}
}

Client::~Client()
{
	sendMessage(ClientMessageType::DESTRUCTION);
	cout << "Awaiting end of message loop..." << endl;
	messageThread.join();
	cout << "Done waiting" << endl;
}


Client::Client(size_t maximumNumberOfPorts) :
		port(Alloc::allocatePositive<PortEntry>(maximumNumberOfPorts)),
		portCapacity(maximumNumberOfPorts)
{
	thread t { Client::serveMessagesForClient, this };
	messageThread.swap(t);

	ensureJackErrorMessageHandler();
}

bool Client::handleOpen(jack_status_t clientOpenStatus)
{
	if (client != nullptr) {
		if (clientOpenStatus != 0) {
			cerr << "Jack client open message: "
					<< statusMessage(clientOpenStatus) << endl;
		}
		shutdownByJack = false;
		if ((jack_set_process_callback(client, rawProcess, this) == 0)
				&& (jack_set_sample_rate_callback(client, rawSetSamplerate,
						this) == 0)) {
			jack_on_info_shutdown(client, rawShutdown, this);
			state = ClientState::CLIENT;
			return true;
		}
		unsafeClose();
		throw runtime_error("Unable to register necessary call-backs");
	}
	throwOnErrorMessage("Could not open");
	string message = "Jack client start failed: ";
	throw runtime_error(message + statusMessage(clientOpenStatus));
}
bool Client::shutdown()
{
	beforeShutdown();
	bool result;
	{
		CriticalScope g(m);
		result = unsafeClose();
		shutdownByJack = true;
	}
	afterShutdown();
	return result;
}
bool Client::unsafeClose()
{
	jack_client_t * c = client;
	client = nullptr;
	state = ClientState::DISCONNECTED;

	if (c == nullptr) {
		cerr << "Already closed" << endl;
		return false;
	}
	if (jack_client_close(c) != 0) {
		cerr << "Error status when closing client";

		return false;
	}
	return true;
}

bool Client::setSamplerateFenced(jack_nframes_t frames)
{
	return setSamplerate(frames);
}


bool Client::prepareAndprocess(jack_nframes_t nframes)
{
	try {
		for (size_t id = 0; id < ports; id++) {
			port[id].buffer =
					(jack_default_audio_sample_t*) (jack_port_get_buffer(
							port[id].port, nframes));
		}
		return process(nframes);
	} catch (std::exception &e) {
		cerr << "Exception: " << e.what() << endl;
		exceptionCount++;
	} catch (...) {
		exceptionCount++;
	}
	return false;
}

int Client::rawProcess(jack_nframes_t nframes, void* arg)
{
	return ((Client*) ((arg)))->prepareAndprocess(nframes) ? 0 : 1;
}

int Client::rawSetSamplerate(jack_nframes_t nframes, void* arg)
{
	return ((Client*) ((arg)))->setSamplerateFenced(nframes) ? 0 : 1;
}

void Client::rawShutdown(jack_status_t status, const char *message, void* arg)
{
	cout << "Jack server shut down (expect further Server is not running messages):" << endl << "\t" << statusMessage(status) << endl << "\t" << message << endl;

	((Client*)arg)->sendMessage(ClientMessageType::SERVER_SHUTDOWN); //	messageServer.sendClientMessage(ClientMessage(ClientMessageType::SHUTDOWN, (Client*)arg));
}

bool Client::isValidPortName(string name)
{
//	static const regex portExpression("[_0-9A-Za-z]+");
	if (name.length() > jack_port_name_size()) {
		return false;
	}
	for (size_t i = 0; i < name.length(); i++) {
		char c = name[i];
		if (!(isalnum(c) || c == '-' || c == '_')) {
			cerr << "Port name contains invalid character '" << c << "': " << name << endl;
			return false;
		}
	}
	return true;
}

void Client::unsafeCheckPortsNotDefined()
{
	if (portsDefined) {
		throw runtime_error("Ports already defined (finished)");
	}
}
void Client::unsafeCheckActivated()
{
	if (state != ClientState::ACTIVE) {
		throw runtime_error("Client not activated");
	}
}
void Client::unsafeCheckNotActivated()
{
	if (state == ClientState::ACTIVE) {
		throw runtime_error("Client already activated");
	}
}
void Client::unsafeCheckPortsDefined()
{
	if (!portsDefined) {
		throw runtime_error("Ports not defined yet");
	}
}
bool Client::unsafeNameAlreadyUsed(string name)
{
	for (size_t i = 0; i < ports; i++) {
		if (port[i].givenName == name) {
			return true;
		}
	}
	return false;
}

void Client::unsafeCheckPortNumberInrange(size_t number)
{
	unsafeCheckPortsDefined();
	if (number >= ports) {
		throw out_of_range("Port number out of range");
	}
}

const Client::Port Client::addPort(PortDirection direction, string name)
{
	if (!isValidPortName(name)) {
		string message = "Port name is invalid: ";
		throw invalid_argument(message + name);
	}
	cerr << "Adding port " << name << endl;
	CriticalScope g(m);
	unsafeCheckPortsNotDefined();
	if (ports >= portCapacity) {
		throw runtime_error("Maximum number of ports cannot be exceeded");
	}
	if (unsafeNameAlreadyUsed(name)) {
		string message = "Port name is already in use: ";
		throw invalid_argument(message + name);
	}
	size_t index = ports++;
	PortEntry& entry = port[index];
	entry.givenName = name;
	entry.dir = direction;
	return Port(this, index);
}

void Client::finishDefiningPorts()
{
	cout << "Finished defining ports" << endl;
	CriticalScope g(m);
	unsafeCheckPortsNotDefined();
	if (ports == 0) {
		throw runtime_error("Must at least define one port");
	}
	portsDefined = true;
}
void Client::unsafeRegisterPorts()
{
	for (size_t i = 0; i < ports; i++) {
		PortEntry &entry = port[i];
		entry.port = jack_port_register(
				client,
				entry.givenName.c_str(),
				JACK_DEFAULT_AUDIO_TYPE,
				entry.dir == PortDirection::IN ? JackPortIsInput : JackPortIsOutput,
				0);
		if (entry.port == 0) {
			unsafeUnRegisterPorts(i);
			string message = "Could not register port: ";
			throw runtime_error(message + entry.givenName);
		}
		entry.actualName = jack_port_name(entry.port);
		entry.buffer = nullptr;
		cout << "Registered port: " << entry.givenName << " -> " << entry.actualName << endl;
	}
}
bool Client::unsafeUnRegisterPorts(ssize_t count)
{
	bool success = true;
	size_t cnt = count >= 0 ? count : ports;
	for (size_t i = 0; i < cnt; i++) {
		PortEntry &entry = port[i];
		if (jack_port_unregister(client, entry.port) != 0) {
			cerr << "Couldn't unregister port: " << entry.givenName << endl;
			success = false;
		}
		entry.port = nullptr;
		entry.actualName = NONAME;
	}
	return success;
}

bool Client::connectPort(size_t id, string otherPort)
{
	CriticalScope g(m);
	unsafeCheckActivated();
	PortEntry entry  = port[id];
	int result;
	if (entry.dir == PortDirection::IN) {
		result = jack_connect(client, otherPort.c_str(), entry.actualName.c_str());
	}
	else {
		result = jack_connect(client, entry.actualName.c_str(), otherPort.c_str());
	}
	return result == 0;
}

bool Client::nameAlreadyUsed(string name)
{
	CriticalScope g(m);
	return unsafeNameAlreadyUsed(name);
}

size_t Client::getNumberOfPorts()
{
	CriticalScope g(m);
	unsafeCheckPortsDefined();
	return ports;
}


Client::Port Client::getPort(size_t number)
{
	CriticalScope g(m);
	unsafeCheckPortNumberInrange(number);
	return Port(this, number);
}

size_t Client::hasProcessExceptions() const
{
	return exceptionCount;
}

bool Client::isShutdown() const
{
	return shutdownByJack;
}

void Client::activate() {
	{
		CriticalScope g(mutex);
		unsafeCheckNotActivated();
		unsafeCheckPortsDefined();
		if (state != ClientState::CLIENT) {
			throw runtime_error("Client not open");
		}
		unsafeRegisterPorts();
		if (jack_activate(client) != 0) {
			unsafeUnRegisterPorts(-1);
			throw runtime_error("Could not activate client");
		}
		cout << "Activated: " << jack_get_client_name(client) << endl;
		state = ClientState::ACTIVE;
	}
	connectPortsOnActivate();
}


void Client::unsafeDeactivate() {
	unsafeCheckActivated();
	if (jack_deactivate(client) != 0) {
		cerr << "Could not deactivate jack client" << endl;
	}
	unsafeUnRegisterPorts(-1);
	state = ClientState::CLIENT;
}
void Client::deactivate() {
	CriticalScope g(mutex);
	unsafeDeactivate();
}

} /* End of namespace jack */
} /* End of namespace speakerman */
