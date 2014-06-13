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
#include <simpledsp/CharacteristicSamples.hpp>
#include <simpledsp/Precondition.hpp>
#include <simpledsp/Biquad.hpp>
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

class SumToAll : public Client
{
	typedef speakerman::Dynamics<accurate_t, 4, 2, 3, 3> Dynamics;
	typedef Dynamics::UserConfig UserConfig;
	typedef Dynamics::Config Configuration;
	typedef Dynamics::Processor<2> Processor;

	struct DoubleConfiguration {
		Configuration configuration1;
		Configuration configuration2;
	};
	UserConfig userConfig1;
	UserConfig userConfig2;

	LockFreeConsumer<DoubleConfiguration, simpledsp::DefaultAssignableCheck> consumer;
	Configuration &wConf1 = consumer.producerValue().configuration1;
	Configuration &wConf2 = consumer.producerValue().configuration2;
	Processor processor1;
	Processor processor2;

	ClientPort input_0_0;
	ClientPort input_0_1;
	ClientPort input_1_0;
	ClientPort input_1_1;
	ClientPort output_0_0;
	ClientPort output_0_1;
	ClientPort output_1_0;
	ClientPort output_1_1;
	ClientPort output_sub;


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

		if (consumer.consume(true)) {
			processor1.checkFilterChanges();
			processor2.checkFilterChanges();
		}

		jack_default_audio_sample_t samples[4];

		for (size_t frame = 0; frame < frameCount; frame++) {

			processor1.input[0] = *inputLeft1++;
			processor1.input[1] = *inputRight1++;
			processor2.input[0] = *inputLeft2++;
			processor2.input[1] = *inputRight2++;

			processor1.process();
			processor2.process();

			*outputLeft1++ = processor1.output[0];
			*outputRight1++ = processor1.output[1];
			*outputLeft2++ = processor2.output[0];
			*outputRight2++ = processor2.output[1];

			sample_t sub = 0.0;
			if (processor1.conf.seperateSubChannel) {
				for (size_t channel = 0; channel < 2; channel++) {
					sub += processor1.subout[channel];
				}
			}
			if (processor2.conf.seperateSubChannel) {
				for (size_t channel = 0; channel < 2; channel++) {
					sub += processor2.subout[channel];
				}
			}
			*subOut++ = sub;
		}

		processor1.displayIntegrations();

		return true;
	}
	virtual bool setSamplerate(jack_nframes_t sampleRate)
	{
		wConf1.configure(userConfig1, sampleRate);
		wConf2.configure(userConfig2, sampleRate);

		std::cout << "Write config" << endl;
		consumer.produce();

		std::cout << "Written config" << endl;
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
	SumToAll(
			ArrayVector<accurate_t, 4> &frequencies,
			ArrayVector<accurate_t, 3> &allPassRcTimes,
			ArrayVector<accurate_t, 3> &bandRcTimes,
			accurate_t threshold1,
			ArrayVector<accurate_t, 5> &bandThreshold1,
			accurate_t threshold2,
			ArrayVector<accurate_t, 5> &bandThreshold2
			)
:
		Client(9),
		input_0_0(addPort(PortDirection::IN, "input_0_0")),
		input_1_0(addPort(PortDirection::IN, "input_1_0")),
		input_0_1(addPort(PortDirection::IN, "input_0_1")),
		input_1_1(addPort(PortDirection::IN, "input_1_1")),
		output_0_0(addPort(PortDirection::OUT, "output_0_0")),
		output_1_0(addPort(PortDirection::OUT, "output_1_0")),
		output_0_1(addPort(PortDirection::OUT, "output_0_1")),
		output_1_1(addPort(PortDirection::OUT, "output_1_1")),
		output_sub(addPort(PortDirection::OUT, "output_sub")),
		processor1(consumer.consumerValue().configuration1),
		processor2(consumer.consumerValue().configuration2)
	{
		userConfig1.frequencies.assign(frequencies);
		userConfig1.allPassRcs.assign(allPassRcTimes);
		userConfig1.bandRcs.assign(bandRcTimes);
		userConfig1.bandThreshold.assign(bandThreshold1);
		userConfig1.threshold = threshold1;

		userConfig2.frequencies.assign(frequencies);
		userConfig2.allPassRcs.assign(allPassRcTimes);
		userConfig2.bandRcs.assign(bandRcTimes);
		userConfig2.bandThreshold.assign(bandThreshold2);
		userConfig2.threshold = threshold2;
		userConfig2.seperateSubChannel = false;
		finishDefiningPorts();
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

	ArrayVector<accurate_t, 4> frequencies;
	frequencies[0] = 80;
	frequencies[1] = 240;
	frequencies[2] = 4500; // 1200
	frequencies[3] = 6500;//3200

	ArrayVector<accurate_t, 3> allPassRcTimes;
	allPassRcTimes[0] = 0.30;
	allPassRcTimes[1] = 0.66;
	allPassRcTimes[2] = 1.7;

	ArrayVector<accurate_t, 3> bandRcTimes;
	bandRcTimes[0] = 0.025;
	bandRcTimes[1] = 0.1;
	bandRcTimes[2] = 0.330;

	ArrayVector<accurate_t, 5> bandThresholds1;
	bandThresholds1[0] = 0.20;
	bandThresholds1[1] = 0.25;
	bandThresholds1[2] = 0.25;
	bandThresholds1[3] = 0.2;
	bandThresholds1[4] = 0.05;

	ArrayVector<accurate_t, 5> bandThresholds2;
	bandThresholds2[0] = 0.10;
	bandThresholds2[1] = 0.25;
	bandThresholds2[2] = 0.25;
	bandThresholds2[3] = 0.2;
	bandThresholds2[4] = 0.05;

	accurate_t threshold1 = 0.2;
	accurate_t threshold2 = 0.1;

	SumToAll processor(frequencies, allPassRcTimes, bandRcTimes, threshold1, bandThresholds1, threshold2, bandThresholds2);

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

