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
#include <iostream>
#include <cmath>

#include <signal.h>

#include <tdap/Array.hpp>
#include <speakerman/jack/Client.hpp>

using namespace speakerman;
using namespace jack;
using namespace tdap;

typedef double sample_t;
typedef double accurate_t;

//static accurate_t crossoverFrequencies[] = {
////		168, 1566, 2500, 6300
//		80, 168, 1566, 6300
////		80, 168, 1000, 2700
////		80, 80 * M_SQRT2,
////		160, 160 * M_SQRT2,
////		320, 320 * M_SQRT2,
////		640, 640 * M_SQRT2,
////		1280, 1280 * M_SQRT2,
////		2560, 2560 * M_SQRT2,
////		5120, 5120 * M_SQRT2,
////		10240
//};


struct SumToAll : public Client
{
	static size_t constexpr GROUPS = 2;
	static size_t constexpr CHANNELS = 2;
	static size_t constexpr FILTER_ORDER = 2;
	static size_t constexpr RC_TIMES = 20;
	static size_t constexpr MAX_SAMPLERATE = 192000;
	static double constexpr MAX_PREDICTION_SECONDS = 0.01;
	static size_t constexpr MAX_PREDICTION_SAMPLES = (MAX_SAMPLERATE * MAX_PREDICTION_SECONDS + 0.5);

public:

	ClientPort input_0_0;
	ClientPort input_0_1;
	ClientPort input_1_0;
	ClientPort input_1_1;
	ClientPort output_0_0;
	ClientPort output_0_1;
	ClientPort output_1_0;
	ClientPort output_1_1;
	ClientPort output_sub;

	Array<RefArray<jack_default_audio_sample_t>> inputs;
	Array<RefArray<jack_default_audio_sample_t>> outputs;


protected:
	virtual bool process(jack_nframes_t frameCount) override
	{
		inputs[0] = RefArray<jack_default_audio_sample_t>(input_0_0.getBuffer(), frameCount);
		inputs[1] = RefArray<jack_default_audio_sample_t>(input_0_1.getBuffer(), frameCount);
		inputs[2] = RefArray<jack_default_audio_sample_t>(input_1_0.getBuffer(), frameCount);
		inputs[3] = RefArray<jack_default_audio_sample_t>(input_1_1.getBuffer(), frameCount);

		outputs[0] = RefArray<jack_default_audio_sample_t>(output_0_0.getBuffer(), frameCount);
		outputs[1] = RefArray<jack_default_audio_sample_t>(output_0_1.getBuffer(), frameCount);
		outputs[2] = RefArray<jack_default_audio_sample_t>(output_1_0.getBuffer(), frameCount);
		outputs[3] = RefArray<jack_default_audio_sample_t>(output_1_1.getBuffer(), frameCount);
		outputs[4] = RefArray<jack_default_audio_sample_t>(output_sub.getBuffer(), frameCount);

		outputs[0].copy(0, inputs[0], 0, frameCount);
		outputs[1].copy(0, inputs[1], 0, frameCount);
		outputs[2].copy(0, inputs[2], 0, frameCount);
		outputs[3].copy(0, inputs[3], 0, frameCount);
		outputs[4].zero();


		return true;
	}

	virtual bool setContext(jack_nframes_t newBufferSize, jack_nframes_t newSampleRate) override
	{
		return configure(newBufferSize, newSampleRate);
	}

	bool configure(jack_nframes_t newBufferSize, jack_nframes_t sampleRate)
	{
//		processor.configure(userConfiguration, sampleRate, newBufferSize);

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

	virtual void connectPortsOnActivate() {
		unique_ptr<PortNames> capturePorts(getPortNames(nullptr, nullptr, JackPortIsPhysical|JackPortIsOutput));

		for (size_t i = 0; i < std::min((size_t)4, capturePorts->length()); i++) {
			const char* portName = capturePorts->get(i);
			switch (i % 4) {
			case 0:
				std::cout << "Connecting " << portName << " -> input_0_0" << std::endl;
				input_0_0.connect(portName);
				break;
			case 1:
				std::cout << "Connecting " << portName << " -> input_0_1" << std::endl;
				input_0_1.connect(portName);
				break;
			case 2:
				std::cout << "Connecting " << portName << " -> input_1_0" << std::endl;
				input_1_0.connect(portName);
				break;
			case 3:
				std::cout << "Connecting " << portName << " -> input_1_1" << std::endl;
				input_1_1.connect(portName);
				break;
			}
		}

		unique_ptr<PortNames> playbackPorts(getPortNames(nullptr, nullptr, JackPortIsPhysical|JackPortIsInput));

		if (playbackPorts->length() > 0) {
			std::cout << "Connecting output_0_0 -> " << playbackPorts->get(0) << std::endl;
			output_0_0.connect(playbackPorts->get(0));
		}
		if (playbackPorts->length() > 1) {
			std::cout << "Connecting output_0_0 -> " << playbackPorts->get(1) << std::endl;
			output_0_1.connect(playbackPorts->get(1));
		}
		if (playbackPorts->length() > 2) {
			std::cout << "Connecting output_0_0 -> " << playbackPorts->get(2) << std::endl;
			output_1_0.connect(playbackPorts->get(2));
		}
		if (playbackPorts->length() > 3) {
			std::cout << "Connecting output_0_0 -> " << playbackPorts->get(3) << std::endl;
			output_1_1.connect(playbackPorts->get(3));
		}
		if (playbackPorts->length() > 4) {
			std::cout << "Connecting output_sub -> " << playbackPorts->get(4) << std::endl;
			output_sub.connect(playbackPorts->get(4));
		}

		unique_ptr<PortNames> pulseAudioPorts(getPortNames("PulseAudio.*", nullptr, JackPortIsOutput));

		for (size_t i = 0; i < pulseAudioPorts->length(); i++) {
			const char* portName = pulseAudioPorts->get(i);
			for (size_t j = 0; j < playbackPorts->length(); j++) {
				if (disconnectPort(portName, playbackPorts->get(j))) {
					std::cout << "Disconnected " << portName << " <-> " << playbackPorts->get(j) << std::endl;
				}
			}
			switch (i % 4) {
			case 0:
				std::cout << "Connecting " << portName << " -> input_0_0" << std::endl;
				input_0_0.connect(portName);
				break;
			case 1:
				std::cout << "Connecting " << portName << " -> input_0_1" << std::endl;
				input_0_1.connect(portName);
				break;
			case 2:
				std::cout << "Connecting " << portName << " -> input_1_0" << std::endl;
				input_1_0.connect(portName);
				break;
			case 3:
				std::cout << "Connecting " << portName << " -> input_1_1" << std::endl;
				input_1_1.connect(portName);
				break;
			}
		}

	}

public:
	SumToAll()
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
		inputs(CHANNELS * GROUPS),
		outputs(CHANNELS * GROUPS + 1)
	{
		finishDefiningPorts();
	};

	bool reconfigure()
	{
		if (sampleRate() > 0 && bufferSize() > 0) {
			return configure(bufferSize(), sampleRate());
		}
		throw std::runtime_error("Cannot reconfigure if no initial config was done");
	}

	virtual ~SumToAll()
	{
		cout << "Finishing up!" << endl;
	}
};

template<class T>
class ClientOwner
{
	atomic<T *> __client;

public:
	ClientOwner() {
		__client.store(0);
	}

	void setClient(T * client)
	{
		T* previous = __client.exchange(client);
		if (previous != nullptr) {
			previous->close();
			delete previous;
		}
	}

	T &get() const
	{
		return *__client;
	}

	~ClientOwner()
	{
		setClient(nullptr);
	}
};
ClientOwner<SumToAll> clientOwner;


extern "C" {
	void signal_callback_handler(int signum)
	{
	   std::cerr << std::endl << "Caught signal " << strsignal(signum) << std::endl;

	   clientOwner.setClient(nullptr);

	   exit(signum);
	}
}

inline static accurate_t frequencyWeight(accurate_t f, accurate_t shelve1, accurate_t shelve2, accurate_t power)
{
		accurate_t fRel = pow(f / shelve1, power);
		accurate_t fShelve2Corr = pow(1.0 / shelve2, power);
		return (1 + fRel * fShelve2Corr) / (1.0 + fRel);
}


int main(int count, char * arguments[]) {
	signal(SIGINT, signal_callback_handler);
	signal(SIGTERM, signal_callback_handler);
	signal(SIGABRT, signal_callback_handler);

	clientOwner.setClient(new SumToAll());

	clientOwner.get().open("speakerman", JackOptions::JackNullOption);
	clientOwner.get().activate();

	std::chrono::milliseconds duration(1000);
	bool running = true;
	while (running) {
		char cmnd;
		std::this_thread::sleep_for( duration );
		std::cin >> cmnd;
		switch (cmnd) {
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

