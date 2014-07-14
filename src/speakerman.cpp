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
#include <cfenv>
#include <mutex>
#include <signal.h>
#include <string.h>
#include <exception>
#include <math.h>
#include <stdio.h>
#include <signal.h>
#include <speakerman/jack/Client.hpp>

#include <saaspl/Delay.hpp>
#include <saaspl/Limiter.hpp>
#include <saaspl/RmsLimiter.hpp>
#include <saaspl/TypeCheck.hpp>
#include <saaspl/WriteLockFreeRead.hpp>

using namespace speakerman;
using namespace speakerman::jack;
using namespace std::chrono;
using namespace saaspl;

enum class Modus { FILTER, BYPASS, ZERO, HIGH, LOW, DOUBLE };

Modus modus = Modus::FILTER;
jack_default_audio_sample_t lowOutputOne = 0;
jack_default_audio_sample_t lowOutputOneNew;
typedef saaspl::Delay<jack_default_audio_sample_t> Delay;
typedef double sample_t;
typedef double accurate_t;

struct SumToAll : public Client
{
	static size_t constexpr CROSSOVERS = 5;
	static size_t constexpr BANDS = CROSSOVERS + 1;
	static size_t constexpr CHANNELS = 2;
	static size_t constexpr FILTER_ORDER = 2;
	static size_t constexpr ALLPASS_RC_TIMES = 3;
	static size_t constexpr BAND_RC_TIMES = 7;

private:
	typedef RmsLimiter<sample_t, accurate_t, CROSSOVERS, FILTER_ORDER, ALLPASS_RC_TIMES, BAND_RC_TIMES> Dynamics;

	struct DoubleConfiguration {
		Dynamics::Config dynamicsConfig1;
		Dynamics::Config dynamicsConfig2;
		Limiter::Config limitingConfig1;
		Limiter::Config limitingConfig2;
		bool bypassLimiter = false;
	};

	Dynamics::UserConfig dynamicsUserConfig1;
	Dynamics::UserConfig dynamicsUserConfig2;
	Limiter::UserConfig limitingUserConfig1;
	Limiter::UserConfig limitingUserConfig2;

	util::WriteLockFreeRead<DoubleConfiguration, TypeCheckCopyAssignableNonPolymorphic> consumer;
	Dynamics::Config &wDynConf1 = consumer.writerValue().dynamicsConfig1;
	Dynamics::Config &wDynConf2 = consumer.writerValue().dynamicsConfig2;
	Limiter::Config &wLimConf1 = consumer.writerValue().limitingConfig1;
	Limiter::Config &wLimConf2 = consumer.writerValue().limitingConfig2;

	Dynamics::Processor<CHANNELS> dynamics1;
	Dynamics::Processor<CHANNELS> dynamics2;
	Limiter::Processor<CHANNELS> limiter1;
	Limiter::Processor<CHANNELS> limiter2;

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
		const jack_default_audio_sample_t* inputLeft1 = input_0_0.getBuffer();
		const jack_default_audio_sample_t* inputRight1 = input_0_1.getBuffer();
		const jack_default_audio_sample_t* inputLeft2 = input_1_0.getBuffer();
		const jack_default_audio_sample_t* inputRight2 = input_1_1.getBuffer();

		jack_default_audio_sample_t* outputLeft1 = output_0_0.getBuffer();
		jack_default_audio_sample_t* outputRight1 = output_0_1.getBuffer();
		jack_default_audio_sample_t* outputLeft2 = output_1_0.getBuffer();
		jack_default_audio_sample_t* outputRight2 = output_1_1.getBuffer();
		jack_default_audio_sample_t* subOut = output_sub.getBuffer();

		if (consumer.read(true)) {
			dynamics1.checkFilterChanges();
			dynamics2.checkFilterChanges();
			limiter1.initConfigChange();
			limiter2.initConfigChange();
		}

		jack_default_audio_sample_t samples[4];

		for (size_t frame = 0; frame < frameCount; frame++) {

			dynamics1.input[0] = *inputLeft1++;
			dynamics1.input[1] = *inputRight1++;
			dynamics2.input[0] = *inputLeft2++;
			dynamics2.input[1] = *inputRight2++;

			dynamics1.process();
			dynamics2.process();

			limiter1.process(dynamics1.output);
			limiter2.process(dynamics2.output);

			*outputLeft1++ = dynamics1.output[0];
			*outputRight1++ = dynamics1.output[1];

			*outputLeft2++ = dynamics2.output[0];
			*outputRight2++ = dynamics2.output[1];

			sample_t sub = 0.0;
			if (dynamics1.conf.seperateSubChannel) {
				for (size_t channel = 0; channel < 2; channel++) {
					sub += dynamics1.subout[channel];
				}
			}
			if (dynamics2.conf.seperateSubChannel) {
				for (size_t channel = 0; channel < 2; channel++) {
					sub += dynamics2.subout[channel];
				}
			}
			*subOut++ = sub;
		}

		return true;
	}
	virtual bool setSamplerate(jack_nframes_t sampleRate)
	{
		wDynConf1.configure(dynamicsUserConfig1, sampleRate);
		cout << "P" << limitingUserConfig1.getPredictionTime() << endl;
		wLimConf1.configure(limitingUserConfig1, sampleRate);

		wDynConf2.configure(dynamicsUserConfig2, sampleRate);
		wLimConf2.configure(limitingUserConfig2, sampleRate);

		std::cout << "Write config" << endl;
		consumer.write();

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
			ArrayVector<accurate_t, CROSSOVERS> &frequencies,
			ArrayVector<accurate_t, ALLPASS_RC_TIMES> &allPassRcTimes,
			ArrayVector<accurate_t, BAND_RC_TIMES> &bandRcTimes,
			accurate_t threshold1,
			ArrayVector<accurate_t, BANDS> &bandThreshold1,
			accurate_t threshold2,
			ArrayVector<accurate_t, BANDS> &bandThreshold2
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
		dynamics1(consumer.readerValue().dynamicsConfig1),
		dynamics2(consumer.readerValue().dynamicsConfig2),
		limiter1(2048, consumer.readerValue().limitingConfig1, consumer.readerValue().dynamicsConfig1.valueRc),
		limiter2(2048, consumer.readerValue().limitingConfig2, consumer.readerValue().dynamicsConfig2.valueRc)
	{
		dynamicsUserConfig1.frequencies.assign(frequencies);
		dynamicsUserConfig1.allPassRcs.assign(allPassRcTimes);
		dynamicsUserConfig1.bandRcs.assign(bandRcTimes);
		dynamicsUserConfig1.bandThreshold.assign(bandThreshold1);
		dynamicsUserConfig1.threshold = threshold1;

		limitingUserConfig1.setThreshold(saaspl::min(1.0, threshold1 * 2));
		limitingUserConfig1.setAttackTime(0.003);
		limitingUserConfig1.setSmoothingTime(0.001);
		limitingUserConfig1.setPredictionTime(0.004);

		dynamicsUserConfig2.frequencies.assign(frequencies);
		dynamicsUserConfig2.allPassRcs.assign(allPassRcTimes);
		dynamicsUserConfig2.bandRcs.assign(bandRcTimes);
		dynamicsUserConfig2.bandThreshold.assign(bandThreshold2);
		dynamicsUserConfig2.threshold = threshold2;
		dynamicsUserConfig2.seperateSubChannel = false;

		limitingUserConfig2.setThreshold(saaspl::min(1.0, threshold2 * 2));
		limitingUserConfig2.setAttackTime(0.003);
		limitingUserConfig2.setAttackTime(0.003);
		limitingUserConfig2.setSmoothingTime(0.001);
		limitingUserConfig2.setPredictionTime(0.004);

		finishDefiningPorts();
	};

	virtual ~SumToAll()
	{
		cout << "Finishing up!" << endl;
	}

	void bypassLimiter() {
		consumer.writerValue().bypassLimiter = !consumer.writerValue().bypassLimiter;
		consumer.write(1000);
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

int main(int count, char * arguments[]) {
	signal(SIGINT, signal_callback_handler);
	signal(SIGTERM, signal_callback_handler);
	signal(SIGABRT, signal_callback_handler);

	ArrayVector<accurate_t, SumToAll::CROSSOVERS> frequencies;
	frequencies[0] = 80;
	frequencies[1] = 180;
	frequencies[2] = 1566;
	frequencies[3] = 4500;//3200
	frequencies[4] = 6500;
//	ArrayVector<accurate_t, SumToAll::CROSSOVERS> frequencies;
//	frequencies[0] = 80;
//	frequencies[1] = 240;
//	frequencies[2] = 1566;
//	frequencies[3] = 4500; // 1200
//	frequencies[4] = 6500;//3200

	ArrayVector<accurate_t, SumToAll::ALLPASS_RC_TIMES> allPassRcTimes;
	allPassRcTimes[0] = 0.33;
	allPassRcTimes[1] = 0.33;
	allPassRcTimes[2] = 0.66;

	ArrayVector<accurate_t, SumToAll::BAND_RC_TIMES> bandRcTimes;
	bandRcTimes[0] = 0.033;
	bandRcTimes[1] = 0.066;
	bandRcTimes[2] = 0.100;
	bandRcTimes[3] = 0.133;
	bandRcTimes[4] = 0.200;
	bandRcTimes[5] = 0.330;
	bandRcTimes[6] = 0.500;

	// Equal log weight spread
	accurate_t minimumFreq= 30;
	accurate_t maximumFreq= 12000;
	ArrayVector<accurate_t, SumToAll::BANDS> bandThresholds1;
	bandThresholds1[0] = frequencies[0] / minimumFreq;
	for (size_t i = 1; i < SumToAll::CROSSOVERS; i++) {
		double f = frequencies[i];
		double warp = (frequencies[0] + 0.25 * f) / (frequencies[0] + 0.5 * f);
		bandThresholds1[i] = warp * frequencies[i] / frequencies[i - 1];
	}
	bandThresholds1[SumToAll::CROSSOVERS] = maximumFreq / frequencies[SumToAll::CROSSOVERS - 1];
	accurate_t scale = 0.0;
	for (size_t i = 0; i < SumToAll::BANDS; i++) {
		scale += bandThresholds1[i];
	}
	for (size_t i = 0; i < SumToAll::BANDS; i++) {
		bandThresholds1[i] /= scale;
	}

	ArrayVector<accurate_t, SumToAll::BANDS> bandThresholds2;
	bandThresholds2 = bandThresholds1;

	accurate_t threshold1 = 0.25;
	accurate_t threshold2 = 0.1;

	clientOwner.setClient(new SumToAll(frequencies, allPassRcTimes, bandRcTimes, threshold1, bandThresholds1, threshold2, bandThresholds2));

	clientOwner.get().open("speakerman", JackOptions::JackNullOption);
	clientOwner.get().activate();

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
			std::cout << "Limiter" << std::endl;
			clientOwner.get().bypassLimiter();
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

