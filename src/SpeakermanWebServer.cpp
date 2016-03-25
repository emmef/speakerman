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

	void web_server::thread_static_function(web_server *server)
	{
		server->thread_function();
	}

	void web_server::thread_function()
	{
		static std::chrono::milliseconds wait(1000);
		static std::chrono::milliseconds sleep(100);
		int count = 0;
		SpeakermanConfig configFileConfig = readSpeakermanConfig(configFileConfig, true);
		DynamicProcessorLevels levels;
		while (true) {
			count++;
			if (count == 10) {
				count = 0;
				auto stamp = getConfigFileTimeStamp();
				if (stamp != configFileConfig.timeStamp) {
					cout << "read config!" << std::endl;
					configFileConfig = readSpeakermanConfig(configFileConfig, false);
					if (manager_.applyConfigAndGetLevels(configFileConfig, &levels, wait)) {
						level_buffer.put(levels);
					}
				}
			}
			else if (manager_.getLevels(&levels, wait)) {
				level_buffer.put(levels);
			}
			this_thread::sleep_for(sleep);
		}
	}

	web_server::web_server(SpeakerManagerControl& speakerManager) :
			http_message(10240),
			manager_(speakerManager)
	{
		thread t(thread_static_function, this);
		level_fetch_thread.swap(t);
		level_fetch_thread.detach();
	}

	web_server::Result web_server::worker_function(
			server_socket::Stream &stream, const struct sockaddr& address,
			const server_socket& socket, void* data)
	{
		return static_cast<web_server*>(data)->accept_work(stream, address, socket);
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

	static const char *toString(char *buffer, size_t len, double value)
	{
		snprintf(buffer, len, "%lf", value);
		return buffer;
	}

	void web_server::handle_request()
	{
		if (strncmp("/", url_, 32) == 0) {
			strncpy(url_, "/index.html", 32);
		}

		if (strncasecmp("/levels.json", url_, 32) == 0) {
			char floats[30];
			LevelEntry entry;
			level_buffer.get(0, entry);
			if (entry.set) {
				DynamicProcessorLevels levels = entry.levels;
				std::cout << "Groups: " << levels.groups() << std::endl;

				set_content_type("text/json");
				response().write_string("\"levels\": {\r\n");
				response().write_string("\t\"subGain\": \"");
				response().write_string(toString(floats, 30, levels.getSubGain()));
				response().write_string("\", \r\n");
				response().write_string("\t\"periods\": \"");
				response().write_string(toString(floats, 30, levels.count()));
				response().write_string("\", \r\n");
					response().write_string("\t\"group\" : [\r\n");
						for (size_t i = 0; i < levels.groups(); i++) {
							response().write_string("\t\t{\r\n");
							response().write_string("\t\t\t\"gain\": \"");
							response().write_string(toString(floats, 30, levels.getGroupGain(i)));
							response().write_string("\", \r\n");
							response().write_string("\t\t\t\"gain_avg\": \"");
							response().write_string(toString(floats, 30, levels.getAverageGroupGain(i)));
							response().write_string("\", \r\n");
							response().write_string("\t\t\t\"level\": \"");
							response().write_string(toString(floats, 30, levels.getSignal(i)));
							response().write_string("\"\r\n");
							if (i < levels.groups() - 1) {
								response().write(',');
							}
							response().write_string("\t\t}\r\n");
						}
					response().write_string("\t]\r\n");
				response().write_string("}\r\n");
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
