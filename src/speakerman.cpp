/*
 * Speakerman.cpp
 *
 * Part of 'Speaker management system'
 *
 * Copyright (C) 2013 Michel Fleur.
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

#include <atomic>
#include <cmath>
#include <iostream>
#include <cstdlib>
#include <thread>

#include <signal.h>
#include <speakerman/jack/JackClient.hpp>
#include <speakerman/SpeakermanConfig.hpp>
#include <speakerman/SpeakerManager.hpp>
#include <speakerman/SpeakermanWebServer.hpp>

using namespace speakerman;
using namespace tdap;

typedef double sample_t;
typedef double accurate_t;

template<class T>
class Owner
{
	atomic<T *> __client;

public:
	Owner() {
		__client.store(0);
	}

	void set(T * client)
	{
		T* previous = __client.exchange(client);
		if (previous != nullptr) {
			cout << "Delete client" << endl;

			delete previous;
		}
	}

	T &get() const
	{
		return *__client.load();
	}

	~Owner()
	{
		set(nullptr);
	}
};

Owner<AbstractSpeakerManager> manager;
SpeakermanConfig configFileConfig;
static volatile int signalNumber = -1;
static volatile int userInput;

extern "C" {
	void signal_callback_handler(int signum)
	{
		std::cerr << std::endl << "Caught signal " << strsignal(signum) << std::endl;

		signalNumber = signum;
	}
}

inline static accurate_t frequencyWeight(accurate_t f, accurate_t shelve1, accurate_t shelve2, accurate_t power)
{
		accurate_t fRel = pow(f / shelve1, power);
		accurate_t fShelve2Corr = pow(1.0 / shelve2, power);
		return (1 + fRel * fShelve2Corr) / (1.0 + fRel);
}

static void charFetcher()
{
	std::chrono::milliseconds duration(101);
	while (signalNumber == -1) {
		char c;
		cin >> c;
		userInput = c;
		cout << "User input " << c << endl;
		while (userInput != EOF) {
			std::this_thread::sleep_for(duration);
		}
	}
	cout << "Ended user input" << endl;
}

static int getChar() {
	int chr = userInput;
	userInput = EOF;
	return chr;
}

static void configUpdater()
{
	std::chrono::milliseconds sleeper(1001);
	while (signalNumber == -1) {
		this_thread::sleep_for(sleeper);
		auto stamp = getConfigFileTimeStamp();
		DynamicProcessorLevels levels(1);
		if (stamp != configFileConfig.timeStamp) {
			configFileConfig = readSpeakermanConfig(configFileConfig, false);
			manager.get().applyConfigAndGetLevels(configFileConfig, &levels, sleeper);
		}
	}
}

static void webServer()
{
	web_server server(manager.get());

	try {
		server.open("8088", 10, 10, nullptr);
		server.work(nullptr);
	}
	catch (const std::exception &e) {
		std::cerr << "Web server error: " << e.what() << std::endl;
	}
//	else {
//		manager.get().getLevels(&levels, sleeper);
//	}
//	for (size_t i = 0; i < levels.groups(); i++) {
//		std::cout << "GROUP " << i << " SIGNAL=" << levels.getSignal(i) << "; GAIN=" << levels.getGroupGain(i) << std::endl;
//	}
//	std::cout << "SUB-GAIN: " << levels.getSubGain() << std::endl;

}

int mainLoop(Owner<JackClient> &owner)
{
	std::thread fetchChars(charFetcher);
	fetchChars.detach();
	std::thread checkConfigUpdates(configUpdater);
	checkConfigUpdates.detach();
	std::thread webServerThread(webServer);
	webServerThread.detach();
	std::chrono::milliseconds duration(100);

	try {
		bool running = true;
		while (running && signalNumber == -1) {
			int cmnd = getChar();
			if (cmnd == EOF) {
				std:this_thread::sleep_for(duration);
				continue;
			}
			std::cout << "User input" << cmnd << std::endl;
			switch (cmnd) {
			case 'a' :
			case 'A' :
				std::cout << "Activating..." << std::endl;
				owner.get().setActive();
				break;
			case 'c' :
			case 'C' :
				std::cout << "Closing..." << std::endl;
				owner.get().close();
				running = false;
				break;
			case 'q' :
			case 'Q' :
				std::cout << "Quiting..." << std::endl;
				running=false;
				break;
			case '+' :
				break;
			case '-' :
				break;
			default:
				std::cerr << "Unknown command " << cmnd << std::endl;
				break;
			}
		}
	}
	catch (const std::exception &e) {
		std::cerr << "Exception caught: " << e.what();
		return SIGABRT;
	}
	if (signalNumber == -1) {
		signalNumber = 0;
	}
	cout << "Bye!";
	if (signalNumber > 0) {
		cout << " CODE " << signalNumber;
	}
	cout << endl;
	return signalNumber;
}


speakerman::config::Reader configReader;
speakerman::server_socket webserver;

template<typename F, size_t GROUPS>
AbstractSpeakerManager *createManagerGr(const SpeakermanConfig & config)
{
	static_assert(is_floating_point<F>::value, "Sample type must be floating point");

	switch (config.groupChannels) {
	case 1:
		return new SpeakerManager<F, 1, GROUPS>(config);
	case 2:
		return new SpeakerManager<F, 2, GROUPS>(config);
	case 3:
		return new SpeakerManager<F, 3, GROUPS>(config);
	case 4:
		return new SpeakerManager<F, 4, GROUPS>(config);
	case 5:
		return new SpeakerManager<F, 5, GROUPS>(config);
	}
	throw invalid_argument("Number of channels per group must be between 1 and 5");
}

template <typename F>
static AbstractSpeakerManager *createManager(const SpeakermanConfig & config)
{

	switch (config.groups) {
	case 1:
		return createManagerGr<F, 1>(config);
	case 2:
		return createManagerGr<F, 2>(config);
	case 3:
		return createManagerGr<F, 3>(config);
	case 4:
		return createManagerGr<F, 4>(config);
	}
	throw invalid_argument("Number of groups must be between 1 and 4");
}

using namespace std;
int main(int count, char * arguments[]) {
	configFileConfig = readSpeakermanConfig(true);

	dumpSpeakermanConfig(configFileConfig, cout);
	manager.set(createManager<double>(configFileConfig));

	Owner<JackClient> clientOwner;
	auto result = JackClient::createDefault("Speaker manager");
	clientOwner.set(result.getClient());

	int error;
	if (!webserver.open("8080", 10, 10, &error)) {
		std::cerr << "Could not open webserver: " << strerror(error) << std::endl;
	}

	const char * all = ".*";
	PortNames inputs = clientOwner.get().portNames(all, all, JackPortIsPhysical|JackPortIsOutput);
	PortNames outputs = clientOwner.get().portNames(all, all, JackPortIsPhysical|JackPortIsOutput);

//	if (!clientOwner.get().setProcessor(manager.get())) {
//		std::cerr << "Failed to set processor" << std::endl;
//		return 1;
//	}

	signal(SIGINT, signal_callback_handler);
	signal(SIGTERM, signal_callback_handler);
	signal(SIGABRT, signal_callback_handler);

//	std::cout << "activate..." << std::endl;
//	clientOwner.get().setActive();

	std::cout << "activated..." << std::endl;
	mainLoop(clientOwner);
	return 0;
}

