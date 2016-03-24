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

#include <cstring>
#include <speakerman/SpeakermanWebServer.hpp>

namespace speakerman
{
	bool web_server::open(const char* service, int timeoutSeconds, int backLog, int* errorCode)
	{
		return socket_.open(service, timeoutSeconds, backLog, errorCode);
	}


	bool web_server::work(int *errorCode)
	{
		return socket_.work(errorCode, web_server::worker_function, this);
	}

	web_server::web_server(SpeakerManagerControl& speakerManager) :
			http_message(10240),
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
		handle(stream);
		return Result::CONTINUE;
	}

	const char * web_server::on_url(const char* url)
	{
		size_t i;
		for (i = 0; i < URL_LENGTH; i++) {
			char c = url[i];
			if (c != 0) {
				url_[i] = c;
			}
			else {
				break;
			}

		}
		if (i < URL_LENGTH) {
			url_[i] = 0;
			std::cout << "D: URL = " << url_ << std::endl;
			return nullptr;
		}
		return "URL too long";
	}

	void web_server::handle_request()
	{
		if (strncmp("/", url_, 32) == 0) {
			strncpy(url_, "/index.html", 32);
		}

		if (strncasecmp("/levels.json", url_, 32) == 0) {
			DynamicProcessorLevels levels(1);
			std::chrono::milliseconds wait(1000);
			if (manager_.getLevels(&levels, wait)) {
				set_content_type("text/json");
				response().write_string("\"levels\" : {}", 1024);
				set_success();
			}
			else {
				set_error(http_status::SERVICE_UNAVAILABLE);
			}
		}
		else if (strncasecmp("/favicon.ico", url_, 32) == 0) {
			set_content_type("text/plain");
			response().write_string("X", 1);
			set_success();
		}
		else if (strncasecmp(url_, "/index.html", 32) == 0) {
			set_content_type("text/html");
			response().write_string("<html>\n", 32);
			response().write_string("<head>\n", 32);
			response().write_string("<title>\n", 32);
			response().write_string("Speakerman\n", 32);
			response().write_string("</title>\n", 32);
			response().write_string("</head>\n", 32);
			response().write_string("<body>\n", 32);
			response().write_string("Speakerman\n", 32);
			response().write_string("<a href=\"/levels.json\">Levels</a>\r\n", 1024);
			response().write_string("</body>\n", 32);
			response().write_string("</html>\n", 32);
//			set_success();
		}
		else {
			set_error(404);
		}
	}



} /* End of namespace speakerman */
