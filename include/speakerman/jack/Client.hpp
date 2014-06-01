/*
 * Client.hpp
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

#ifndef SMS_SPEAKERMAN_CLIENT_GUARD_H_
#define SMS_SPEAKERMAN_CLIENT_GUARD_H_

#include <speakerman/jack/Messages.hpp>
#include <string>
#include <mutex>
#include <stdexcept>

namespace speakerman {
namespace jack {

	using namespace std;
	using namespace simpledsp;

	static void ensureJackErrorMessageHandler();
	static const char * getAndResetErrorMessage();

	void throwOnErrorMessage(const char* description = nullptr);

	enum class PortDirection
	{
		IN, OUT
	};
	enum class ClientState
	{
		DISCONNECTED, CLIENT, ACTIVE
	};
	class CriticalScope
	{
		mutex &m;
	public:
		CriticalScope(mutex &mutex) : m(mutex)
		{
			m.lock();
		}
		~CriticalScope()
		{
			m.unlock();
		}
	};
	class PortNames
	{
		const char** const portNames;
		size_t count;
		friend class Client;

		PortNames(jack_client_t* client, const char* namePattern,
				const char* typePattern, unsigned long flags);

		size_t rangeCheck(size_t index) const;

	public:
		size_t length() const;
		const char* get(size_t idx) const;
		const char* operator [](size_t idx) const;
		~PortNames();
	};


	class Client {
		struct PortEntry
		{
			string givenName;
			PortDirection dir;
			string actualName;
			jack_port_t * port;
			jack_default_audio_sample_t * buffer;

			PortEntry();
		};

		PortEntry * const port;
		const size_t portCapacity;
		size_t ports = 0;
		mutex m;
		jack_client_t * client = nullptr;
		ClientState state = ClientState::DISCONNECTED;
		bool portsDefined = false;
		volatile size_t exceptionCount = 0;
		volatile bool shutdownByJack = false;

		virtual bool process(jack_nframes_t frames) = 0;
		virtual bool setSamplerate(jack_nframes_t frames) = 0;
		virtual void beforeShutdown() { }
		virtual void afterShutdown() { }
		virtual void connectPortsOnActivate() { }

		bool shutdown();
		bool handleOpen(jack_status_t clientOpenStatus);

		inline const char * getName(size_t id) const
		{
			return port[id].givenName.c_str();
		}
		inline const char * getActualName(size_t id) const
		{
			return port[id].actualName.c_str();
		}
		PortDirection getDirection(size_t id) const
		{
			return port[id].dir;
		}
		jack_default_audio_sample_t * getBuffer(size_t id) const
		{
			return port[id].buffer;
		}
		bool connectPort(size_t id, string otherPort);

		void unsafeCheckPortsDefined();
		void unsafeCheckPortsNotDefined();
		void unsafeCheckActivated();
		void unsafeCheckNotActivated();
		bool unsafeNameAlreadyUsed(string name);
		void unsafeCheckPortNumberInrange(size_t number);
		void unsafeRegisterPorts();
		bool unsafeUnRegisterPorts(ssize_t count);
		void unsafeDeactivate();
		bool unsafeClose();

		bool setSamplerateFenced(jack_nframes_t frames);
		bool prepareAndprocess(jack_nframes_t nframes);
		static int rawProcess(jack_nframes_t nframes, void* arg);
		static int rawSetSamplerate(jack_nframes_t nframes, void* arg);
		static void rawShutdown(void* arg);

	public:

		class Port
		{
			Client * const a;
			const size_t id;

			friend class Client;

			Port(Client * const aPtr, size_t _id) : a(aPtr), id(_id) {}

		public:
			Port(const Port &source) : a(source.a), id(source.id) {}
			inline const char * getName() const { return a->getName(id); }
			inline const char * getActualName() const { return a->getActualName(id); }
			inline jack_default_audio_sample_t * getBuffer() const { return a->getBuffer(id); }
			inline PortDirection getDirection() const { return a->getDirection(id); }
			inline bool connect(string otherPortname) const { return a->connectPort(id, otherPortname); }
		};

	protected:

		/**
		 * Subclasses can define their ports here and retrieve Port objects to access them!
		 */
		const Port addPort(PortDirection direction, string name);
		/**
		 * Done with defining ports. Must be called before all functions that actually
		 * activate the jack client or connect ports.
		 */
		void finishDefiningPorts();
		/**
		 * Checks if this port name is already in use.
		 */
		bool nameAlreadyUsed(string name);

	public:

		static bool isValidPortName(string name);

		Client(size_t maximumNumberOfPorts);

		size_t getNumberOfPorts();
		Port getPort(size_t number);
		size_t hasProcessExceptions() const;
		bool isShutdown() const;
		void activate();
		void deactivate();
		PortNames * getPortNames(const char * namePattern, const char * typePattern, unsigned long flags);

		template<typename... Args> bool open(string client_name, jack_options_t options, Args... args)
		{
			CriticalScope g(mutex);

			if (state != ClientState::DISCONNECTED) {
				throw runtime_error("Already connected or not yet fully disconnected");
			}
			jack_status_t clientOpenStatus;

			client = jack_client_open(client_name.c_str(), options, &clientOpenStatus, args...);

			handleOpen(clientOpenStatus);
			cout << "Opened jack client: " << client_name << endl;
		}
		void close()
		{
			CriticalScope g(mutex);

			if (state == ClientState::ACTIVE) {
				unsafeDeactivate();
			}

			unsafeClose();
		}
	};

	typedef Client::Port ClientPort;


} /* End of namespace jack */
} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_CLIENT_GUARD_H_ */


/*
	class Port {
		const string portName;
		string actualPortName;
		const PortDirection dir;
		jack_default_audio_sample_t *bufferPtr = nullptr;
		jack_port_t *port = nullptr;

		Port(string name, PortDirection direction) :
			portName(name),
			actualPortName(""),
			dir(direction)
		{
			if (name.length() < 1) {
				throw invalid_argument("Port must have a non-empty name");
			}
		}

		bool registerPort(jack_client_t *client)
		{
			port = jack_port_register(client, portName.c_str(),
					JACK_DEFAULT_AUDIO_TYPE,
					dir == PortDirection::IN ? JackPortIsInput : JackPortIsOutput, 0);

			if (port) {
				actualPortName = jack_port_name(port);
				return true;
			}

			actualPortName = "";
			return false;
		}

		bool unregisterPort(jack_client_t *client)
		{
			if (port) {
				jack_port_unregister(client, port);
				port = nullptr;
			}
			actualPortName = "";
		}

		bool connectWith(jack_client_t *client, const char * portName) // privately accessed by ClientHandler
		{
			int connected;
			if (dir == PortDirection::IN) {
				connected = jack_connect(client, portName, actualPortName.c_str());
			}
			else {
				connected = jack_connect(client, actualPortName.c_str(), portName);
			}
			if (connected == 0) {
				return true;
			}
		}

		bool getBuffer(jack_nframes_t samples) // privately accessed by ClientHandler
		{
			if (port) {
				bufferPtr = (jack_default_audio_sample_t *)jack_port_get_buffer(port, samples);

				return bufferPtr != nullptr;
			}

			return false;
		}

	public:
		const string givenName() const { return portName; }
		const string actualName() const { return actualPortName; }
		PortDirection direction() const { return dir; }
		jack_default_audio_sample_t * buffer() const { return bufferPtr; }

		~Port() {
		}

		friend class ClientHandler;
	};

	class ClientHandler {

	};
*/
