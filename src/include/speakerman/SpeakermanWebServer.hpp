/*
 * SpeakermanWebServer.hpp
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

#ifndef SMS_SPEAKERMAN_WEBSERVER_GUARD_H_
#define SMS_SPEAKERMAN_WEBSERVER_GUARD_H_

#include <speakerman/ServerSocket.hpp>
#include <speakerman/HttpMessage.hpp>
#include <speakerman/SpeakermanConfig.hpp>

namespace speakerman
{
	class web_server : protected http_message
	{
	public:
		using Result = server_worker_result;
		using State = server_socket_state;
		using Stream = server_socket::Stream;
		static constexpr size_t URL_LENGTH = 1023;

		web_server(SpeakerManagerControl& speakerManager);

		bool open(const char* service, int timeoutSeconds, int backLog, int* errorCode);

		const char * const service() const { return socket_.service(); }

		State state() const { return socket_.state(); }

		bool isOpen() const { return state() != State::CLOSED; }

		bool isWorking() const { return state() == State::WORKING; }

		bool work(int *errorCode);

		void close();

		~web_server(){}

	protected:
		virtual bool content_stream_delete() const override { return false; }
		virtual const char * on_url(const char* url) override;
		virtual void handle_request() override;
	private:
		SpeakerManagerControl &manager_;
		server_socket socket_;
		char url_[URL_LENGTH + 1];

		Result accept_work(Stream &stream, const struct sockaddr& address, const server_socket& socket);

		static Result worker_function(
				Stream &stream, const struct sockaddr& address,
				const server_socket& socket, void* data);
	};

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_WEBSERVER_GUARD_H_ */
