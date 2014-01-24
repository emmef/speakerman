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
#include <mutex>
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
#include <speakerman/jack/JackClient.hpp>
#include <speakerman/SpeakerMan.hpp>
#include <speakerman/Matrix.hpp>

using namespace speakerman;
using namespace speakerman::jack;

enum class Modus { FILTER, BYPASS, ZERO, HIGH, LOW, DOUBLE };

Modus modus = Modus::FILTER;
jack_default_audio_sample_t lowOutputOne = 0;
jack_default_audio_sample_t lowOutputOneNew;
typedef SingleReadDelay<jack_default_audio_sample_t> Delay;

template<size_t CROSSOVERS> class SumToAll : public JackProcessor
{
	std::recursive_mutex mutex;
	SpeakerManager<2,CROSSOVERS,8,4,4,1> manager;
	array<freq_t,CROSSOVERS> frequencies;

protected:
	virtual bool process(jack_nframes_t frameCount)
	{
		const jack_default_audio_sample_t* inputLeft1 = getInput(0, frameCount);
		const jack_default_audio_sample_t* inputRight1 = getInput(1, frameCount);
		const jack_default_audio_sample_t* inputLeft2 = getInput(2, frameCount);
		const jack_default_audio_sample_t* inputRight2 = getInput(3, frameCount);
		const jack_default_audio_sample_t* inputLeft3 = getInput(4, frameCount);
		const jack_default_audio_sample_t* inputRight3 = getInput(5, frameCount);
		const jack_default_audio_sample_t* inputLeft4 = getInput(6, frameCount);
		const jack_default_audio_sample_t* inputRight4 = getInput(7, frameCount);

		jack_default_audio_sample_t* outputLeft1 = getOutput(0, frameCount);
		jack_default_audio_sample_t* outputRight1 = getOutput(1, frameCount);
		jack_default_audio_sample_t* outputLeft2 = getOutput(2, frameCount);
		jack_default_audio_sample_t* outputRight2 = getOutput(3, frameCount);
		jack_default_audio_sample_t* subOut = getOutput(4, frameCount);

		Array<sample_t> &input = manager.getInput();
		const Array<sample_t> &output = manager.getOutput();
		const Array<sample_t> &sub = manager.getSubWoofer();

		for (size_t frame = 0; frame < frameCount; frame++) {
			input[0] = *inputLeft1++;
			input[1] = *inputRight1++;
			input[2] = *inputLeft2++;
			input[3] = *inputRight2++;
			input[4] = *inputLeft3++;
			input[5] = *inputRight3++;
			input[6] = *inputLeft4++;
			input[7] = *inputRight4++;

			manager.process();

			*outputLeft1++ = output[0];
			*outputRight1++ = output[1];
			*outputLeft2++ = output[2];
			*outputRight2++ = output[3];
			*subOut++ = sub[0];
		}

		return true;
	}
	virtual bool setSampleRate(jack_nframes_t sampleRate)
	{
		Guard guard(mutex);
		manager.configure(frequencies, sampleRate);
		return true;
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
	SumToAll(array<freq_t, CROSSOVERS> freqs) :
		frequencies(freqs)
	{
		addInput("left_in1");
		addInput("right_in1");
		addInput("left_in2");
		addInput("right_in2");
		addInput("left_in3");
		addInput("right_in3");
		addInput("left_in4");
		addInput("right_in4");

		addOutput("left_out1");
		addOutput("right_out1");
		addOutput("left_out2");
		addOutput("right_out2");
		addOutput("sub_out");
	};

	~SumToAll()
	{
	}
};

class JackClientOwner
{
	std::recursive_mutex m;
	JackClient * client = nullptr;

	void unsafeSet(JackClient *newClient) {
		Guard guard(mutex);
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
		Guard guard(mutex);
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

	array<freq_t, 1> frequencies;
	frequencies[1] = 80;

	SumToAll<1> processor(frequencies);

	client.set(new JackClient("Speakerman", processor));

	client.get().open(JackOptions::JackNullOption);
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
	}

	return 0;
}

