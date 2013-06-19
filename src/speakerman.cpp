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
#include <stdio.h>
#include <signal.h>
#include <simpledsp/Precondition.hpp>
#include <simpledsp/Butterworth.hpp>
#include <speakerman/utils/Mutex.hpp>
#include <speakerman/JackConnector.hpp>

using namespace speakerman;

typedef Iir<jack_default_audio_sample_t, double> JackFilters;

class SumToAll : public JackProcessor
{
	CoefficientBuilder builder;
	JackFilters::FixedOrderMultiFilter<2, 4> filter1;
	JackFilters::FixedOrderMultiFilter<2, 4> filter2;

protected:
	virtual bool process(jack_nframes_t frameCount)
	{
		const jack_default_audio_sample_t* inputLeft1 = getInput(0, frameCount);
		const jack_default_audio_sample_t* inputRight1 = getInput(1, frameCount);
		const jack_default_audio_sample_t* inputLeft2 = getInput(2, frameCount);
		const jack_default_audio_sample_t* inputRight2 = getInput(3, frameCount);

		jack_default_audio_sample_t* outputLeft1 = getOutput(0, frameCount);
		jack_default_audio_sample_t* outputRight1 = getOutput(1, frameCount);
		jack_default_audio_sample_t* outputLeft2 = getOutput(2, frameCount);
		jack_default_audio_sample_t* outputRight2 = getOutput(3, frameCount);
		jack_default_audio_sample_t* subOut = getOutput(4, frameCount);

		for (jack_nframes_t i = 0; i < frameCount; i++) {
			jack_default_audio_sample_t x = 0.5  * ( + inputRight2[i]);
			const jack_default_audio_sample_t inL1 = inputLeft1[i];
			const jack_default_audio_sample_t inR1 = inputRight1[i];
			const jack_default_audio_sample_t inL2 = inputLeft2[i];
			const jack_default_audio_sample_t inR2 = inputRight2[i];

			// profiles
			// - number of dynamic processing chains
			// - number of output chains
			// - matrix from number of inputs to processing chains
			// - matrix from processing to output chains
			// - output chains have N x input channels (depends on number of cross-overs)
			// - matrix to actual outputs (may sum crossovers)

			jack_default_audio_sample_t subL1 = filter2.filter(0, filter1.filter(0, inL1));
			jack_default_audio_sample_t subR1 = filter2.filter(1, filter1.filter(1, inR1));
			jack_default_audio_sample_t subL2 = filter2.filter(2, filter1.filter(2, inL2));
			jack_default_audio_sample_t subR2 = filter2.filter(3, filter1.filter(3, inR2));

			outputLeft1[i] = inL1 - subL1;
			outputRight1[i] = inR1 - subR1;
			outputLeft2[i] = inL2 - subL2;
			outputRight2[i] = inR2 - subR2;

			subOut[i] = subL1 + subR1 + subL2 + subR2;
		}

		return true;
	}
	virtual bool setSampleRate(jack_nframes_t sampleRate)
	{
		std::cout << "Sample rate set to " << sampleRate << std::endl;

		Butterworth::createCoefficients(builder, (frequency_t)sampleRate, 80, Butterworth::Pass::LOW);
		filter1.setCoefficients(builder);
		filter2.setCoefficients(builder);

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
	SumToAll() : builder(2), filter1(builder), filter2(builder) {
		addInput("left_in1");
		addInput("right_in1");
		addInput("left_in2");
		addInput("right_in2");

		addOutput("left_out1");
		addOutput("right_out1");
		addOutput("left_out2");
		addOutput("right_out2");
		addOutput("sub_out");
	};


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
	   std::cerr << "Caught signal " << strsignal(signum) << std::endl;

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
		std::this_thread::sleep_for( duration );
	}

	return 0;
}

