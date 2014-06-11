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
#include <atomic>
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
#include <simpledsp/LockFreeConsumer.hpp>
#include <simpledsp/Noise.hpp>
#include <simpledsp/SingleReadDelay.hpp>
#include <speakerman/jack/Client.hpp>
#include <speakerman/SpeakerMan.hpp>

using namespace speakerman;
using namespace speakerman::jack;

enum class Modus { FILTER, BYPASS, ZERO, HIGH, LOW, DOUBLE };

Modus modus = Modus::FILTER;
jack_default_audio_sample_t lowOutputOne = 0;
jack_default_audio_sample_t lowOutputOneNew;
typedef SingleReadDelay<jack_default_audio_sample_t> Delay;

template<size_t CROSSOVERS> class SumToAll : public Client
{
	typedef simpledsp::Iir::Fixed::MultiFixedChannelFilter<sample_t, accurate_t, 2, 4> Filter;

	SpeakerManager<2,CROSSOVERS,4,4,4,1> manager;
	array<freq_t,CROSSOVERS> frequencies;
	ClientPort input_0_0;
	ClientPort input_0_1;
	ClientPort input_1_0;
	ClientPort input_1_1;
	ClientPort output_0_0;
	ClientPort output_0_1;
	ClientPort output_1_0;
	ClientPort output_1_1;
	ClientPort output_sub;
	// DEBUG


	struct Configuration
	{
		CoefficientsBuilder highPassCoefficients;
		CoefficientsBuilder lowPassCoefficients;

		Configuration() :
			highPassCoefficients(2),
			lowPassCoefficients(2) { }
	};
	LockFreeConsumer<Configuration> consumer;
	Configuration &readConfiguration;
	Configuration &writeConfiguration;
	size_t processConfigVersion = -1;
	Filter hf;
	Filter lf;

	static void writeCoeffs(CoefficientsBuilder &builder, string name)
	{
		cout << "Write " << name;
		for (size_t i = 0; i <= builder.order(); i++) {
			cout << "; C[" << i << "]=" << builder.getC(i) << "; D[" << i << "]=" << builder.getD(i);
		}
		cout << endl;
	}

protected:
	virtual bool process(jack_nframes_t frameCount)
	{
		MemoryFence fence;
		const jack_default_audio_sample_t* inputLeft1 = input_0_0.getBuffer();
		const jack_default_audio_sample_t* inputRight1 = input_0_1.getBuffer();
		const jack_default_audio_sample_t* inputLeft2 = input_1_0.getBuffer();
		const jack_default_audio_sample_t* inputRight2 = input_1_1.getBuffer();

		jack_default_audio_sample_t* outputLeft1 = output_0_0.getBuffer();
		jack_default_audio_sample_t* outputRight1 = output_0_1.getBuffer();
		jack_default_audio_sample_t* outputLeft2 = output_1_0.getBuffer();
		jack_default_audio_sample_t* outputRight2 = output_1_1.getBuffer();
		jack_default_audio_sample_t* subOut = output_sub.getBuffer();

		ArrayVector<accurate_t, 4> &input = manager.getInput();
		const ArrayVector<accurate_t, 4> &output = manager.getOutput();
		const ArrayVector<accurate_t, 1> &sub = manager.getSubWoofer();

		jack_default_audio_sample_t samples[4];

		if (consumer.consume(true)) {
			cout << "Read configuration" << endl;
			writeCoeffs(writeConfiguration.highPassCoefficients, "High pass");
			writeCoeffs(writeConfiguration.lowPassCoefficients, "Low pass");
			hf.setCoefficients(readConfiguration.highPassCoefficients);
			lf.setCoefficients(readConfiguration.lowPassCoefficients);
		}

		for (size_t frame = 0; frame < frameCount; frame++) {
			input[0] = *inputLeft1++;
			input[1] = *inputRight1++;
			input[2] = *inputLeft2++;
			input[3] = *inputRight2++;

			accurate_t sub = 0.0;
			for (size_t i = 0; i < 4; i++) {
				simpledsp::accurate_t x = input[i];
				samples[i] = hf.filter(i, x);
				sub += lf.filter(i, x);
			}

			*outputLeft1++ = samples[0];
			*outputRight1++ = samples[1];
			*outputLeft2++ = samples[2];
			*outputRight2++ = samples[3];
			*subOut++ = sub;

//			manager.process();
//
//			*outputLeft1++ = output.get(0);
//			*outputRight1++ = output.get(1);
//			*outputLeft2++ = output.get(2);
//			*outputRight2++ = output.get(3);
//
//			*subOut++ = sub.get(0);
		}

		return true;
	}
	virtual bool setSamplerate(jack_nframes_t sampleRate)
	{
		MemoryFence fence;
		std::cerr << "Configuring samplerate: " << sampleRate << endl;
		manager.configure(frequencies, sampleRate);

		simpledsp::Butterworth::createRelativeFrequencyCoefficients(writeConfiguration.lowPassCoefficients, 180 / sampleRate, simpledsp::Butterworth::Pass::LOW, true);
		simpledsp::Butterworth::createRelativeFrequencyCoefficients(writeConfiguration.highPassCoefficients, 180 / sampleRate, simpledsp::Butterworth::Pass::HIGH, true);
		writeCoeffs(writeConfiguration.lowPassCoefficients, "Low pass");
		writeCoeffs(writeConfiguration.highPassCoefficients, "High pass");
		cout << endl;

		simpledsp::Butterworth::createRelativeFrequencyCoefficients(writeConfiguration.highPassCoefficients, 180 / sampleRate, simpledsp::Butterworth::Pass::HIGH, true);
		simpledsp::Butterworth::createRelativeFrequencyCoefficients(writeConfiguration.lowPassCoefficients, 180 / sampleRate, simpledsp::Butterworth::Pass::LOW, true);
		writeCoeffs(writeConfiguration.lowPassCoefficients, "Low pass");
		writeCoeffs(writeConfiguration.highPassCoefficients, "High pass");
		cout << endl;

		cout << "Wrote configuration" << endl;
		consumer.produce();

		return true;
	}
	virtual void beforeShutdown()
	{
		std::cerr << "Before shutdown";
	}

	virtual void afterShutdown()
	{
		std::cerr << "After shutdown";
	}
	virtual void connectPortsOnActivate() { }

public:
	SumToAll(array<freq_t, CROSSOVERS> freqs) :
		Client(9),
		frequencies(freqs),
		input_0_0(addPort(PortDirection::IN, "input_0_0")),
		input_1_0(addPort(PortDirection::IN, "input_1_0")),
		input_0_1(addPort(PortDirection::IN, "input_0_1")),
		input_1_1(addPort(PortDirection::IN, "input_1_1")),
		output_0_0(addPort(PortDirection::OUT, "output_0_0")),
		output_1_0(addPort(PortDirection::OUT, "output_1_0")),
		output_0_1(addPort(PortDirection::OUT, "output_0_1")),
		output_1_1(addPort(PortDirection::OUT, "output_1_1")),
		output_sub(addPort(PortDirection::OUT, "output_sub")),
		readConfiguration(consumer.consumerValue()),
		writeConfiguration(consumer.producerValue())
	{
		finishDefiningPorts();
		CoefficientsBuilder x(2);
		for (size_t i = 0; i < CROSSOVERS; i++) {
			cout <<  "- crossover[" << i << "]: " << frequencies[i] << endl;
		}
	};

	~SumToAll()
	{
	}
};


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

	array<freq_t, 3> frequencies;
	frequencies[0] = 80;
	frequencies[1] = 800;
	frequencies[2] = 8000;

	SumToAll<3> processor(frequencies);

	processor.open("speakerman", JackOptions::JackNullOption);
	processor.activate();

	std::chrono::milliseconds duration(1000);
	bool running = true;
	while (running) {
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
		case 'q' :
		case 'Q' :
			std::cout << "Quiting..." << std::endl;
			running=false;
			break;
		default:
			std::cerr << "Unknown command " << cmnd << std::endl;
			break;
		}
	}

	return 0;
}

