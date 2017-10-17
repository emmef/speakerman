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
#include <iostream>
#include <cstdio>
#include <sys/types.h>
#include <unistd.h>
#include <speakerman/jack/SignalHandler.hpp>
#include <speakerman/SpeakermanWebServer.hpp>
#include <speakerman/utils/Config.hpp>
#include <tdap/MemoryFence.hpp>

namespace speakerman {
    bool web_server::open(const char *service, int timeoutSeconds, int backLog, int *errorCode)
    {
        return socket_.open(service, timeoutSeconds, backLog, errorCode);
    }


    bool web_server::work(int *errorCode)
    {
        return socket_.work(errorCode, web_server::worker_function, this);
    }

    void web_server::thread_static_function(web_server *server)
    {
        CountedThreadGuard guard;

        try {
            server->thread_function();
        }
        catch (const signal_exception &e) {
            e.handle("Web server configuration update and level fetching");
        }
    }

    static void create_command_and_file(string &rangeFile, string &command_line)
    {
        char number[33];
        snprintf(number, 33, "%x", rand());

        rangeFile = "/tmp/";
        rangeFile += number;
        snprintf(number, 33, "%llx", (long long int)getpid());
        rangeFile += number;
        rangeFile += ".ranges";

        command_line = getWatchDogScript();
        command_line += " ";
        command_line += rangeFile;
    }

    struct TemporaryFile
    {
        ifstream file_;
        string name_;

        TemporaryFile(const char *name) :
                name_(name)
        {
            file_.open(name_);
        }

        bool is_open() const
        {
            return file_.is_open();
        }

        const char * name() const
        {
            return name_.c_str();
        }

        istream &stream()
        {
            return file_;
        }

        ~TemporaryFile()
        {
            if (file_.is_open()) {
                file_.close();
            }
            int result = std::remove(name_.c_str());
            if (result != 0) {
                cerr << "Could not remove " << name_ << ": " << strerror(errno) << endl;
            }
        }
    };

    static void approach_threshold_scaling(double &value, int new_value) {
        if (new_value > value) {
            value *= 1.02;
            if (value > new_value) {
                value = new_value;
            }
        }
        else if (new_value < value) {
            value /= 1.02;
            if (value < new_value) {
                value = new_value;
            }
        }
    }

    void web_server::thread_function()
    {
        static std::chrono::milliseconds wait(1000);
        static std::chrono::milliseconds sleep(50);
        int count = 1;

        SpeakermanConfig configFileConfig;
        {
            tdap::MemoryFence fence;
            configFileConfig = manager_.getConfig();
        }
        DynamicProcessorLevels levels;
        string range_file;
        string command_line;
        int threshold_scaling_setting = 1;
        double threshold_scaling = threshold_scaling_setting;
        double new_threshold_scaling = threshold_scaling;

        while (!SignalHandler::check_raised()) {
            count++;
            bool got_levels = false;
            if ((count % 10) == 0) {
                approach_threshold_scaling(new_threshold_scaling, threshold_scaling_setting);
                bool read = false;
                auto stamp = getConfigFileTimeStamp();
                if (stamp != configFileConfig.timeStamp) {
                    cout << "read config!" << std::endl;
                    try {
                        configFileConfig = readSpeakermanConfig(
                                configFileConfig, false);
                        read = true;
                    }
                    catch (const runtime_error &e) {
                        cerr << "Error reading configuration: " << e.what()
                             << endl;
                        configFileConfig.timeStamp = stamp;
                    }
                }
                if (new_threshold_scaling != threshold_scaling) {
                    threshold_scaling = new_threshold_scaling;
                    configFileConfig.threshold_scaling = threshold_scaling;
                    read = true;
                }
                if (read && manager_.applyConfigAndGetLevels(configFileConfig, &levels, wait)) {
                    level_buffer.put(levels);
                    got_levels = true;
                }
            }
            if (!got_levels && manager_.getLevels(&levels, wait)) {
                level_buffer.put(levels);
            }
            if (count == 100) {
                count = 0;
                create_command_and_file(range_file, command_line);
                int old_setting = threshold_scaling_setting;
                threshold_scaling_setting = 1;
                if (system(command_line.c_str()) == 0) {
                    TemporaryFile file{range_file.c_str()};
                    if (file.is_open()) {
                        istream &stream = file.stream();
                        while (!stream.eof()) {
                            char chr = stream.get();
                            if (chr >= '1' && chr <= '5') {
                                threshold_scaling_setting = chr - '0';
                                break;
                            }
                            else if (!config::isWhiteSpace(chr)) {
                                break;
                            }
                        }
                    }
                }
                if (old_setting != threshold_scaling_setting) {
                    cout << "Threshold scaling set from " << old_setting << " to " << threshold_scaling_setting << endl;
                }
            }
            this_thread::sleep_for(sleep);
        }
    }

    web_server::web_server(SpeakerManagerControl &speakerManager) :
            http_message(10240, 2048),
            manager_(speakerManager),
            indexHtmlFile("index.html"),
            cssFile("speakerman.css"),
            javaScriptFile("speakerman.js"),
            faviconFile("favicon.png")
    {
        thread t(thread_static_function, this);
        level_fetch_thread.swap(t);
        level_fetch_thread.detach();
    }

    web_server::Result web_server::worker_function(
            server_socket::Stream &stream, const struct sockaddr &address,
            const server_socket &socket, void *data)
    {
        return static_cast<web_server *>(data)->accept_work(stream, address, socket);
    }

    void web_server::close()
    {
        socket_.close();
    }

    web_server::Result
    web_server::accept_work(Stream &stream, const struct sockaddr &address, const server_socket &socket)
    {
        levelTimeStamp = 0;
        handle(stream);
        return Result::CONTINUE;
    }

    const char *web_server::on_url(const char *url)
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
//			std::cout << "D: URL = " << url_ << std::endl;
            return nullptr;
        }
        return "URL too long";
    }

    void web_server::on_header(const char *header, const char *value)
    {
        static constexpr int ASSIGN = 1;
        static constexpr int VALUE = 2;
        static constexpr int NUM = 3;
        static constexpr int DONE = 4;
        unsigned long long number = 0;
        unsigned long long previousNumber = 0;
        if (strcasecmp("cookie", header) == 0) {
            const char *pos = strstr(value, COOKIE_TIME_STAMP);
            if (pos) {
                pos += COOKIE_TIME_STAMP_LENGTH;
                int status = ASSIGN;
                char c;
                while (status != DONE && (c = *pos++) != 0) {
                    switch (status) {
                        case ASSIGN:
                            if (c == '=') {
                                status = VALUE;
                            }
                            else if (c != ' ') {
                                return;
                            }
                            break;
                        case VALUE:
                            if (c == ' ') {
                                continue;
                            }
                            if (c == ';') {
                                return;
                            }
                            if (c >= '0' && c <= '9') {
                                number = c - '0';
                                status = NUM;
                            }
                            else {
                                return;
                            }
                            break;
                        case NUM:
                            if (c >= '0' && c <= '9') {
                                previousNumber = number;
                                number *= 10;
                                number += c - '0';
                                if (number < previousNumber) {
                                    status = DONE;
                                }
                            }
                            else if (c == ';') {
                                status = DONE;
                            }
                            break;
                    }
                }
                levelTimeStamp = number;
            }
        }
    }

    static const char *ftostr(char *buffer, size_t len, double value)
    {
        snprintf(buffer, len, "%lf", value);
        return buffer;
    }

    static const char *itostr(char *buffer, size_t len, long long value)
    {
        snprintf(buffer, len, "%lli", value);
        return buffer;
    }

    void web_server::handle_request()
    {
        if (strncmp("/", url_, 32) == 0) {
            strncpy(url_, "/index.html", 32);
        }
        for (size_t i = 0; i < URL_LENGTH && url_[i] != '\0'; i++) {
            if (url_[i] == '?') {
                url_[i] = '\0';
                break;
            }
        }
        if (strncasecmp("/levels.json", url_, 32) == 0) {
            char numbers[60];
            LevelEntry entry;
            level_buffer.get(levelTimeStamp, entry);
            if (entry.set) {
                DynamicProcessorLevels levels = entry.levels;
                snprintf(numbers, 60, "%s=%lli", COOKIE_TIME_STAMP, entry.stamp);
                set_header("Set-Cookie", numbers);
                set_header("Access-Control-Allow-Origin", "*");
                set_content_type("application/json");
                response().write_string("{\r\n");
                response().write_string("\t\"elapsedMillis\": \"");
                response().write_string(itostr(numbers, 30, entry.stamp - levelTimeStamp));
                response().write_string("\", \r\n");
                response().write_string("\t\"subGain\": \"");
                response().write_string(ftostr(numbers, 30, levels.getGain(0)));
                response().write_string("\", \r\n");
                response().write_string("\t\"thresholdScale\": \"");
                response().write_string(ftostr(numbers, 30, manager_.getConfig().threshold_scaling));
                response().write_string("\", \r\n");
                response().write_string("\t\"subAverageGain\": \"");
                response().write_string(ftostr(numbers, 30, levels.getAverageGain(0)));
                response().write_string("\", \r\n");
                response().write_string("\t\"subLevel\": \"");
                response().write_string(ftostr(numbers, 30, levels.getSignal(0)));
                response().write_string("\", \r\n");
                response().write_string("\t\"subAverageLevel\": \"");
                response().write_string(ftostr(numbers, 30, levels.getAverageSignal(0)));
                response().write_string("\", \r\n");
                response().write_string("\t\"periods\": \"");
                response().write_string(itostr(numbers, 30, levels.count()));
                response().write_string("\", \r\n");
                response().write_string("\t\"group\" : [\r\n");
                for (size_t i = 0; i < levels.groups(); i++) {
                    response().write_string("\t\t{\r\n");
                    response().write_string("\t\t\t\"gain\": \"");
                    response().write_string(ftostr(numbers, 30, levels.getGain(i + 1)));
                    response().write_string("\", \r\n");
                    response().write_string("\t\t\t\"averageGain\": \"");
                    response().write_string(ftostr(numbers, 30, levels.getAverageGain(i + 1)));
                    response().write_string("\",\r\n");
                    response().write_string("\t\t\t\"level\": \"");
                    response().write_string(ftostr(numbers, 30, levels.getSignal(i + 1)));
                    response().write_string("\",\r\n");
                    response().write_string("\t\t\t\"averageLevel\": \"");
                    response().write_string(ftostr(numbers, 30, levels.getAverageSignal(i + 1)));
                    response().write_string("\"\r\n");
                    response().write_string("\t\t}");
                    if (i < levels.groups() - 1) {
                        response().write(',');
                    }
                    response().write_string("\r\n");
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
            indexHtmlFile.reset();
            handle_content(indexHtmlFile.size(), &indexHtmlFile);
        }
        else if (strncasecmp(url_, "/speakerman.css", 32) == 0) {
            set_content_type("text/css");
            cssFile.reset();
            handle_content(cssFile.size(), &cssFile);
        }
        else if (strncasecmp(url_, "/speakerman.js", 32) == 0) {
            set_content_type("text/javascript");
            javaScriptFile.reset();
            handle_content(javaScriptFile.size(), &javaScriptFile);
        }
        else if (strncasecmp(url_, "/favicon.ico", 32) == 0) {
            set_content_type("image/png");
            faviconFile.reset();
            handle_content(faviconFile.size(), &faviconFile);
        }
        else {
            set_error(404);
        }
    }
} /* End of namespace speakerman */
