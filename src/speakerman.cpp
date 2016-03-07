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

	void setClient(T * client)
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
		setClient(nullptr);
	}
};

Owner<SpeakerManager<2, 1>> manager;
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

int mainLoop(Owner<JackClient> &owner)
{
	std::thread fetchChars(charFetcher);
	fetchChars.detach();
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

int main(int count, char * arguments[]) {
	SpeakermanConfig config = readSpeakermanConfig(true);

	cout << "Dump config" << endl;
	dumpSpeakermanConfig(config, cout);

	manager.setClient(new SpeakerManager<2, 1>(config));

	Owner<JackClient> clientOwner;
	auto result = JackClient::createDefault("Speaker manager");
	clientOwner.setClient(result.getClient());


	const char * all = ".*";
	PortNames inputs = clientOwner.get().portNames(all, all, JackPortIsPhysical|JackPortIsOutput);
	PortNames outputs = clientOwner.get().portNames(all, all, JackPortIsPhysical|JackPortIsOutput);

	if (!clientOwner.get().setProcessor(manager.get())) {
		std::cerr << "Failed to set processor" << std::endl;
		return 1;
	}

	signal(SIGINT, signal_callback_handler);
	signal(SIGTERM, signal_callback_handler);
	signal(SIGABRT, signal_callback_handler);

	std::cout << "activate..." << std::endl;
	clientOwner.get().setActive();

	std::cout << "activated..." << std::endl;
	mainLoop(clientOwner);
	return 0;
}

