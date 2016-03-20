/*
 * SpeakermanWebServer.cpp
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

#include <speakerman/SpeakermanWebServer.hpp>

namespace speakerman
{
	bool web_server::enter_function(void* data) {
		return static_cast<web_server*>(data)->work_enter();
	}

	void web_server::leave_function(void* data) {
		static_cast<web_server*>(data)->work_leave();
	}

	bool web_server::work_enter()
	{
	}

	void web_server::work_leave()
	{
	}

	bool web_server::open(const char* service, int timeoutSeconds, int backLog, int* errorCode)
	{
		return socket_.open(service, timeoutSeconds, backLog, errorCode);
	}


	bool web_server::work(int *errorCode)
	{
		return socket_.work(errorCode, web_server::worker_function, web_server::enter_function, web_server::leave_function, this);
	}

	web_server::web_server(SpeakerManagerControl& speakerManager) :
			manager_(speakerManager)
	{
	}

	web_server::Result web_server::worker_function(
			server_socket::Stream &stream, const struct sockaddr& address,
			const server_socket& socket, void* data)
	{
		static_cast<web_server*>(data)->accept_work(stream, address, socket);
	}

	void web_server::close()
	{
		socket_.close();
	}

	web_server::Result web_server::accept_work(Stream &stream, const struct sockaddr& address, const server_socket& socket)
	{
		char c;
		while (!stream.eof()) {
			stream >> c;
			std::cout << c;
		}
		stream << "HTTP/1.1 200 OK\n\r" << std::endl;
	}


} /* End of namespace speakerman */
