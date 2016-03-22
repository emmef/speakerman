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

#include <cstring>
#include <speakerman/HttpMessage.hpp>

namespace speakerman
{
	static const char * UNKNOWN_STATUS = "Unknown status";

	const char* http_status::status_name(unsigned status)
	{
		switch (status) {
		case OK:
			// 200
			return "OK";
		case BAD_REQUEST:
			// 400
			return "Bad Request";
		case METHOD_NOT_ALLOWED:
			// 405
			return "Method not allowed";
		case REQUEST_URI_TOO_LONG:
			// 414
			return "Request URI too long";
		case INTERNAL_SERVER_ERROR:
			//500
			return "Internal server error";
		default:
			return UNKNOWN_STATUS;
		}
	}

	int http_status::format_message(char* buffer, size_t max_length, unsigned status)
	{
		return snprintf(buffer, max_length, "HTTP/1.1 %u %s\r\n", status, status_name(status));
	}

	int http_status::format_message_extra(char* buffer, size_t max_length, unsigned status, const char *extraMessage)
	{
		return snprintf(buffer, max_length, "HTTP/1.1 %u %s\r\n\r\n%s\r\n", status, status_name(status), extraMessage);
	}

	enum class State
	{
		METHOD
	};


	const char * http_message::on_method(const char* method)
	{
		if (strncmp("GET", method, 10)) {
			return nullptr;
		}
		return "GET";
	}

	inline const char* http_message::on_url(const char* url)
	{
	}


	void http_message::handle(socket_stream &stream)
	{
		if (!read_method(stream)) {
			return;
		}
		if (!read_url(stream)) {
			return;
		}
	}

	bool http_message::read_method(socket_stream& stream)
	{
		int c;
		int length = 0;
		for (length = 0; length < (LINE_BUFFER-1) && ((c = stream.read()) >= 0); length++) {
			if (c < 'A' && c > 'Z') {
				break;
			}
			line_[length] = c;
		}
		if (c < 0) {
			http_status::format_message_extra(line_, LINE_BUFFER, http_status::BAD_REQUEST, "Unexpected end of stream");
			stream.write_string(line_, LINE_BUFFER, 0);
			stream.flush();
			return false;
		}
		else if (c != ' ') {
			http_status::format_message_extra(line_, LINE_BUFFER, http_status::BAD_REQUEST, "Invalid method");
			stream.write_string(line_, LINE_BUFFER, 0);
			stream.flush();
			return false;
		}
		line_[length] = 0;
		const char *onMethod = on_method(line_);
		if (onMethod) {
			http_status::format_message(line_, LINE_BUFFER, http_status::METHOD_NOT_ALLOWED);
			stream.write_string(line_, LINE_BUFFER, 0);
			snprintf(line_, LINE_BUFFER, "Accept:%s\r\n", onMethod);
			stream.flush();
			return false;
		}

		return true;
	}

	inline int http_message::read_url(socket_stream& stream)
	{
		static constexpr int SP = 0;
		static constexpr int HEX1 = 1;
		static constexpr int HEX2 = 2;

		int c;
		const int maxLength = LINE_BUFFER - 1;
		int length = 0;

		for (length = 0; length < maxLength && ((c = stream.read()) >= 0); length++) {

		}
	}



} /* End of namespace speakerman */

