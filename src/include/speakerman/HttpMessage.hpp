/*
 * HttpMessage.hpp
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

#ifndef SMS_SPEAKERMAN_HTTP_MESSAGE_GUARD_H_
#define SMS_SPEAKERMAN_HTTP_MESSAGE_GUARD_H_

#include <speakerman/SocketStream.hpp>

namespace speakerman
{
	struct http_status
	{
		static constexpr unsigned OK = 200;
		static constexpr unsigned BAD_REQUEST = 400;
		static constexpr unsigned METHOD_NOT_ALLOWED = 405;
		static constexpr unsigned REQUEST_URI_TOO_LONG = 414;
		static constexpr unsigned INTERNAL_SERVER_ERROR = 500;

		static const char* status_name(unsigned status);

		static int format_message(char* buffer, size_t max_length, unsigned status);
		static int format_message_extra(char* buffer, size_t max_length, unsigned status, const char * extraMessage);
	};

	class http_message
	{
		bool read_method(socket_stream& stream);
		int read_url(socket_stream& stream);
	protected:
		/**
		 * Invoked when method was read from request.
		 *
		 * @param method The request method
		 * @return nullptr on success or a list of allowed methods on failure
		 */
		virtual const char * on_method(const char* method);

		/**
		 * Invoked when URL was read from request.
		 *
		 * @param url The request URL
		 * @return nullptr on success or additional message on failure
		 */
		const virtual char* on_url(const char* url);
	public:
		static constexpr size_t LINE_BUFFER = 2048;

		void handle(socket_stream &stream);

		virtual ~http_message() = default;

	private:
		char line_[LINE_BUFFER];
	};

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_HTTP_MESSAGE_GUARD_H_ */
