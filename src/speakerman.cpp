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

#include <chrono>
#include <thread>
#include <iostream>
#include <signal.h>
#include <string.h>
#include <exception>
#include <math.h>
#include <stdio.h>
#include <signal.h>
#include <simpledsp/Precondition.hpp>
#include <simpledsp/Butterworth.hpp>
#include <simpledsp/Noise.hpp>
#include <simpledsp/SingleReadDelay.hpp>
#include <speakerman/utils/Mutex.hpp>
#include <speakerman/JackClient.hpp>
#include <speakerman/SpeakerMan.hpp>
#include <speakerman/Matrix.hpp>
#include <speakerman/Limiter.hpp>
#include <speakerman/BandSplitter.hpp>

using namespace speakerman;

enum class Modus { FILTER, BYPASS, ZERO, HIGH, LOW, DOUBLE };

Modus modus = Modus::FILTER;
jack_default_audio_sample_t lowOutputOne = 0;
jack_default_audio_sample_t lowOutputOneNew;
typedef SingleReadDelay<jack_default_audio_sample_t> Delay;

static void printCoefficients(CoefficientBuilder &builder, string message) {
	std::cout << message << ":" << std::endl;
	std::cout << "\tOrder: " << builder.order();

	std::cout << std::endl;
	for (size_t i = 0; i < builder.order(); i++) {
		std::cout << " \tC[" << i << "]=" << builder.getC()[i];
	}
	std::cout << std::endl;
	for (size_t i = 0; i < builder.order(); i++) {
		std::cout << " \tD[" << i << "]=" << builder.getD()[i];
	}
	std::cout << std::endl;

}

class SumToAll : public JackProcessor
{
	SpeakerManager * manager = nullptr;
	LimiterSettings settings1;
	LimiterSettings settings2;

protected:
	virtual bool process(jack_nframes_t frameCount)
	{
		if (!manager) {
			return false;
		}
		const jack_default_audio_sample_t* inputLeft1 = getInput(0, frameCount);
		const jack_default_audio_sample_t* inputRight1 = getInput(1, frameCount);
		const jack_default_audio_sample_t* inputLeft2 = getInput(2, frameCount);
		const jack_default_audio_sample_t* inputRight2 = getInput(3, frameCount);

		jack_default_audio_sample_t* outputLeft1 = getOutput(0, frameCount);
		jack_default_audio_sample_t* outputRight1 = getOutput(1, frameCount);
		jack_default_audio_sample_t* outputLeft2 = getOutput(2, frameCount);
		jack_default_audio_sample_t* outputRight2 = getOutput(3, frameCount);
		jack_default_audio_sample_t* subOut = getOutput(4, frameCount);

		for (size_t frame = 0; frame < frameCount; frame++) {
			manager->setInputValue(0, *inputLeft1++);
			manager->setInputValue(1, *inputRight1++);
			manager->setInputValue(2, *inputLeft2++);
			manager->setInputValue(3, *inputRight2++);

			manager->process();

			*outputLeft1++ = manager->getOutputValue(0);
			*outputRight1++ = manager->getOutputValue(1);
			*outputLeft2++ = manager->getOutputValue(2);
			*outputRight2++ = manager->getOutputValue(3);
			*subOut++ = manager->getSubWooferValue(0);
		}

		return true;
	}
	virtual bool setSampleRate(jack_nframes_t sampleRate)
	{
		if (manager) {
			manager->configure(sampleRate);
			return true;
		}
		return false;
	}

	virtual void shutdownByServer()
	{

	}
	virtual void prepareActivate()
	{
		std::cout << "Activate!" << std::endl;
	}
	virtual void prepareDeactivate()
	{
		std::cout << "De-activate!" << std::endl;
	}

public:
	SumToAll() {
		addInput("left_in1", "^PulseAudio JACK Sink.*left$", ".*", 0);
		addInput("right_in1", "^PulseAudio JACK Sink.*right$", ".*", 0);
		addInput("left_in2");
		addInput("right_in2");

		addOutput("left_out1", ".*OUTPUT_1", "firewire_pcm", 0);
		addOutput("right_out1", ".*OUTPUT_1", "firewire_pcm", 0);
		addOutput("left_out2");
		addOutput("right_out2");
		addOutput("sub_out",  ".*OUTPUT*", "firewire_pcm", 0);

		MultibandLimiterConfigList builder;

		builder
				.addCrossover(160).addCrossover(640).addCrossover(2560).setChannels(2).setLimiterConfig(&settings1).add()
				.addCrossover(160).addCrossover(640).addCrossover(2560).setChannels(2).setLimiterConfig(&settings2).add();

		manager = new SpeakerManager(4, builder, 4, 1, 80.0);
	};

	~SumToAll()
	{
		if (manager) {
			delete manager;
			manager = nullptr;
		}
	}
};

class JackClientOwner
{
	Mutex m;
	JackClient * client = nullptr;

	void unsafeSet(JackClient *newClient) {
		Guard guard = m.guard();
		if (client) {
			client->close();
			delete client;
		}
		client = newClient;
	}
public:
	void set(JackClient *newClient) {
		if (newClient) {
			unsafeSet(newClient);
		}
		else {
			throw invalid_argument("new client must not be NIL");
		}
	}

	JackClient &get() {
		Guard guard = m.guard();
		if (client) {
			return *client;
		}
		throw nullptr_error();
	}

	~JackClientOwner() {
		std::cerr << "Closing jack client" << std::endl;
		unsafeSet(nullptr);
	}
};

static JackClientOwner client;

extern "C" {
	void signal_callback_handler(int signum)
	{
	   std::cerr << std::endl << "Caught signal " << strsignal(signum) << std::endl;

	   exit(signum);
	}
}

int main(int count, char * arguments[]) {
	signal(SIGINT, signal_callback_handler);
	signal(SIGTERM, signal_callback_handler);
	signal(SIGABRT, signal_callback_handler);


	SumToAll processor;
	client.set(new JackClient("Speakerman", processor));

	client.get().open();
	client.get().activate();

	std::chrono::milliseconds duration(1000);
	while (true) {
		char cmnd;
		std::this_thread::sleep_for( duration );
		std::cin >> cmnd;
		switch (cmnd) {
		case 'b':
		case 'B':
			std::cout << "Bypass" << std::endl;
			modus = Modus::BYPASS;
			break;
		case 'f':
		case 'F':
			std::cout << "Filter" << std::endl;
			modus = Modus::FILTER;
			break;
		case 'z':
		case 'Z':
			std::cout << "Zero" << std::endl;
			modus = Modus::ZERO;
			break;
		case 'l':
		case 'L':
			std::cout << "Low-pass" << std::endl;
			modus = Modus::LOW;
			break;
		case 'h':
		case 'H':
			std::cout << "High-pass" << std::endl;
			modus = Modus::HIGH;
			break;
		case 'd' :
		case 'D' :
			std::cout << "Double filtering" << std::endl;
			modus = Modus::DOUBLE;
			break;
		default:
			std::cerr << "Unknown command " << cmnd << std::endl;
			break;
		}
		std::cout << "Magnitude of DC is " <<  lowOutputOneNew << std::endl;
	}

	return 0;
}

