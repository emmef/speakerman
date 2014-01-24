/*
 * Connection.hpp
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

#ifndef SMS_SPEAKERMAN_CONNECTION_GUARD_H_
#define SMS_SPEAKERMAN_CONNECTION_GUARD_H_

#include <stdexcept>
#include <mutex>
#include <thread>
#include <iostream>

#include <jack/jack.h>
#include <simpledsp/Guard.hpp>
#include <speakerman/jack/Messages.hpp>

namespace speakerman {
namespace jack {


enum class ConnectionState {
	DISCONNECTED,
	DISCONNECTING,
	CONNECTED
};

class Client
{
	typedef simpledsp::Guard Guard;

	std::recursive_mutex mutex;
	jack_client_t *client = 0;
	ConnectionState status = ConnectionState::DISCONNECTED;

public:
	template<typename... Args> void connect(string client_name, jack_options_t options, Args... args)
	{
		Guard g(mutex);

		if (status != ConnectionState::DISCONNECTED) {
			throw runtime_error("Already connected or not yet fully disconnected");
		}
		jack_status_t clientOpenStatus;

		client = jack_client_open(client_name.c_str(), options, &clientOpenStatus, args...);

		string message = "";

		if (clientOpenStatus == 0) {
			if (client != nullptr) {
				status = ConnectionState::CONNECTED;
				return;
			}
			message = "Couldn't open connection to jack";
		}
		else if ((clientOpenStatus & JackStatus::JackServerStarted) == JackStatus::JackServerStarted) {
			cout << statusMessage(clientOpenStatus) << endl;
			status = ConnectionState::CONNECTED;
			return;
		}
		else {
			message = statusMessage(clientOpenStatus);
			if (client != nullptr) {
				message += "/ Closing client because of unknown state";
				jack_client_close(client);
			}
		}
		throw runtime_error(message);
	}

	template<typename T>void disconnect(bool (*canClose)(T &context, jack_client_t *), T &context)
	{
		{
			Guard g(mutex);
			if (status != ConnectionState::CONNECTED) {
				throw runtime_error("Cannot disconnect: already disconnected or disconnecting");
			}
			status = ConnectionState::DISCONNECTING;
		}

		jack_client_t *clientToClose = nullptr;

		while (clientToClose) {
			Guard g(mutex);
			if (canClose(context, client)) {
				clientToClose = client;
				client = nullptr;
			}
		}

		jack_client_close(clientToClose);
		{
			Guard g(mutex);
			status = ConnectionState::DISCONNECTED;
		}
	}
	template<typename R, typename T> R useClient(T &context, R (user)(T &context, jack_client_t *client))
	{
		Guard g(mutex);
		switch (status) {
		case ConnectionState::CONNECTED:
			return user(context, client);
		default:
			throw runtime_error("Cannot get jack client when closed or closing");
		}
	}
	template<typename R> R useClient(R (user)(jack_client_t *client))
	{
		Guard g(mutex);
		switch (status) {
		case ConnectionState::CONNECTED:
			return user(client);
		default:
			throw runtime_error("Cannot get jack client when closed or closing");
		}
	}
};


} /* End of namespace jack */
} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_CONNECTION_GUARD_H_ */
