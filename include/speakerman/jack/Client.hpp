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
#include <speakerman/jack/JackClient.hpp>
#include <string>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <thread>
#include <stdexcept>


namespace speakerman {
namespace jack {

	using namespace std;

	enum class ClientState
	{
		DISCONNECTED, CLIENT, ACTIVE
	};
	class CriticalScope
	{
		recursive_mutex &m;
	public:
		CriticalScope(recursive_mutex &mutex) : m(mutex)
		{
			m.lock();
		}
		~CriticalScope()
		{
			m.unlock();
		}
	};

	class Client
	{
		// Error message from jack (which are not tailored to a specific client
		static void errorMessageHandler(const char * message);
		static void ensureJackErrorMessageHandler();
		static const char * getAndResetErrorMessage();
		static void throwOnErrorMessage(const char* description = nullptr);
		// Handling of messages in the that cannot be dealt with in the
		// callback by jack (for instance: you cannot call close while
		// in the call-back that jack is shutting doen)
		enum class ClientMessageType
		{
			NONE, SERVER_SHUTDOWN, DESTRUCTION
		};
		static void serveMessagesForClient(Client *owner);
		std::mutex messageMutex;
		std::condition_variable messageCondition;
		std::queue<ClientMessageType> messageQueue;
		volatile bool messageShutdown = false;;
		std::thread messageThread;
		void serveMessages();
		void sendMessage(ClientMessageType type);
		void executeMessage(ClientMessageType type);

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
		recursive_mutex m;
		jack_client_t * client = nullptr;
		ClientState state = ClientState::DISCONNECTED;
		bool portsDefined = false;
		volatile size_t exceptionCount = 0;
		volatile bool shutdownByJack = false;
		jack_nframes_t sampleRateProposal = 0;
		jack_nframes_t bufferSizeProposal = 0;
		jack_nframes_t sampleRate_ = 0;
		jack_nframes_t bufferSize_ = 0;

		bool updateSampleRate(jack_nframes_t sampleRate);
		bool updateBufferSize(jack_nframes_t bufferSize);

		virtual bool process(jack_nframes_t frames) = 0;
		virtual bool setContext(jack_nframes_t bufferSize, jack_nframes_t sampleRate) = 0;
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

		bool prepareAndprocess(jack_nframes_t nframes);
		static int rawProcess(jack_nframes_t nframes, void* arg);
		static int rawSetSamplerate(jack_nframes_t nframes, void* arg);
		static int rawSetBufferSize(jack_nframes_t bufferSize, void *arg);
		static void rawShutdown(jack_status_t status, const char *message, void* arg);

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

		jack_nframes_t sampleRate() { return sampleRate_; }
		jack_nframes_t bufferSize() { return bufferSize_; }
		ClientState clientState() { return state; }

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
		bool disconnectPort(string readPort, string writePort);

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
		virtual ~Client();
	};

	typedef Client::Port ClientPort;


} /* End of namespace jack */
} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_CLIENT_GUARD_H_ */

