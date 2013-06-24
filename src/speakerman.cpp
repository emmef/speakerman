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

using namespace speakerman;

typedef Iir<jack_default_audio_sample_t, double> JackFilters;

enum class Modus { FILTER, BYPASS, ZERO, HIGH, LOW };

Modus modus = Modus::FILTER;
jack_default_audio_sample_t lowOutputOne = 0;
jack_default_audio_sample_t lowOutputOneNew;
typedef SingleReadDelay<jack_default_audio_sample_t> Delay;

class SumToAll : public JackProcessor
{
	CoefficientBuilder builder;
	JackFilters::FixedOrderMultiFilter<2, 8> lowPass;
	simpledsp::Noise<4> noise;
	Delay *delay[4];

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


		switch (modus) {
		case Modus::ZERO:
			for (jack_nframes_t i = 0; i < frameCount; i++) {
				outputLeft1[i] = 0;
				outputRight1[i] = 0;
				outputLeft2[i] = 0;
				outputRight2[i] = 0;

				subOut[i] = 0;
			}
			break;
		case Modus::BYPASS :
			for (jack_nframes_t i = 0; i < frameCount; i++) {
				outputLeft1[i] = inputLeft1[i];
				outputRight1[i] = inputRight1[i];
				outputLeft2[i] = inputLeft2[i];
				outputRight2[i] = inputRight2[i];

				subOut[i] = 0;
			}
			break;
		case Modus::HIGH:
			for (jack_nframes_t i = 0; i < frameCount; i++) {
				const jack_default_audio_sample_t inL1 = inputLeft1[i];
				const jack_default_audio_sample_t inR1 = inputRight1[i];
				const jack_default_audio_sample_t inL2 = inputLeft2[i];
				const jack_default_audio_sample_t inR2 = inputRight2[i];

				// profiles
				// - number of dynamic processing chains
				// - number of output chains
				// - matrix from number of inputs to processing chains
				// - matrix from processing to output chains
				// - output chains have output channels = N x input channels (depends on number of cross-overs)
				// - matrix to actual outputs (may sum crossovers)
				// - equalizing/limiting (both optional) per output

				jack_default_audio_sample_t subL1 = lowPass.filter(0, lowPass.filter(1, inL1));
				jack_default_audio_sample_t subR1 = lowPass.filter(2, lowPass.filter(3, inR1));
				jack_default_audio_sample_t subL2 = lowPass.filter(4, lowPass.filter(5, inL2));
				jack_default_audio_sample_t subR2 = lowPass.filter(6, lowPass.filter(7, inR2));

				delay[0]->write(inL1);
				delay[1]->write(inR1);
				delay[2]->write(inL2);
				delay[3]->write(inR2);

				outputLeft1[i] = delay[0]->read() - subL1;
				outputRight1[i] = delay[1]->read() - subR1;
				outputLeft2[i] = delay[2]->read() - subL2;
				outputRight2[i] = delay[3]->read() - subL2;

				subOut[i] = 0;
			}
			break;
		case Modus::LOW:
			for (jack_nframes_t i = 0; i < frameCount; i++) {
				const jack_default_audio_sample_t inL1 = inputLeft1[i];
				const jack_default_audio_sample_t inR1 = inputRight1[i];
				const jack_default_audio_sample_t inL2 = inputLeft2[i];
				const jack_default_audio_sample_t inR2 = inputRight2[i];

				// profiles
				// - number of dynamic processing chains
				// - number of output chains
				// - matrix from number of inputs to processing chains
				// - matrix from processing to output chains
				// - output chains have output channels = N x input channels (depends on number of cross-overs)
				// - matrix to actual outputs (may sum crossovers)	>n-1 + Sn
				// - equalizing/limiting (both optional) per output

				jack_default_audio_sample_t subL1 = lowPass.filter(0, lowPass.filter(1, inL1));
				jack_default_audio_sample_t subR1 = lowPass.filter(2, lowPass.filter(3, inR1));
				jack_default_audio_sample_t subL2 = lowPass.filter(4, lowPass.filter(5, inL2));
				jack_default_audio_sample_t subR2 = lowPass.filter(6, lowPass.filter(7, inR2));

				outputLeft1[i] = 0;
				outputRight1[i] = 0;
				outputLeft2[i] = 0;
				outputRight2[i] = 0;

				subOut[i] = 0.5 * (subL1 + subR1 + subL2 + subR2);;
			}
			break;
		default:
			for (jack_nframes_t i = 0; i < frameCount; i++) {
				const jack_default_audio_sample_t inL1 = inputLeft1[i] + noise.get(0);
				const jack_default_audio_sample_t inR1 = inputRight1[i] + noise.get(1);
				const jack_default_audio_sample_t inL2 = inputLeft2[i] + noise.get(2);
				const jack_default_audio_sample_t inR2 = inputRight2[i] + noise.get(3);

				// profiles
				// - number of dynamic processing chains
				// - number of output chains
				// - matrix from number of inputs to processing chains
				// - matrix from processing to output chains
				// - output chains have output channels = N x input channels (depends on number of cross-overs)
				// - matrix to actual outputs (may sum crossovers)
				// - equalizing/limiting (both optional) per output

				jack_default_audio_sample_t subL1 = lowPass.filter(0, lowPass.filter(1, inL1));
				jack_default_audio_sample_t subR1 = lowPass.filter(2, lowPass.filter(3, inR1));
				jack_default_audio_sample_t subL2 = lowPass.filter(4, lowPass.filter(5, inL2));
				jack_default_audio_sample_t subR2 = lowPass.filter(6, lowPass.filter(7, inR2));

				delay[0]->write(inL1);
				delay[1]->write(inR1);
				delay[2]->write(inL2);
				delay[3]->write(inR2);

				outputLeft1[i] = delay[0]->read() - subL1;
				outputRight1[i] = delay[1]->read() - subR1;
				outputLeft2[i] = delay[2]->read() - subL2;
				outputRight2[i] = delay[3]->read() - subL2;


				subOut[i] = 0.5 * (subL1 + subR1 + subL2 + subR2);
			}
			break;
		}
		return true;
	}
	virtual bool setSampleRate(jack_nframes_t sampleRate)
	{
		std::cout << "Sample rate set to " << sampleRate << std::endl;
		double frequency = 80;
		Butterworth::createCoefficients(builder, (frequency_t)sampleRate, frequency, Butterworth::Pass::LOW);
		lowPass.setCoefficients(builder);

		delay[0]->buffer().clear();
		delay[1]->buffer().clear();
		delay[2]->buffer().clear();
		delay[3]->buffer().clear();

		delay[0]->setDelay(0.5 * sampleRate / frequency);
		delay[1]->setDelay(0.5 * sampleRate / frequency);
		delay[2]->setDelay(0.5 * sampleRate / frequency);
		delay[3]->setDelay(0.5 * sampleRate / frequency);

		size_t length = simpledsp::effectiveLength(builder, sampleRate, frequency, pow(2, -18), 10);

		std::cout << "Effective filter length is " << length << " samples or " << 1e3*length / sampleRate << " ms." << " or " << (length * frequency / sampleRate) << " cycles " << std::endl;

		noise.setCutoff(sampleRate, 20);
		noise.setAmplitude(pow(2, -24));

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
	SumToAll() : builder(2), lowPass(builder), noise(0.001, 100, 9600) {
		addInput("left_in1");
		addInput("right_in1");
		addInput("left_in2");
		addInput("right_in2");

		addOutput("left_out1");
		addOutput("right_out1");
		addOutput("left_out2");
		addOutput("right_out2");
		addOutput("sub_out");

		delay[0] = new Delay(96000);
		delay[1] = new Delay(96000);
		delay[2] = new Delay(96000);
		delay[3] = new Delay(96000);
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
		default:
			std::cerr << "Unknown command " << cmnd << std::endl;
			break;
		}
		std::cout << "Magnitude of DC is " <<  lowOutputOneNew << std::endl;
	}

	return 0;
}

