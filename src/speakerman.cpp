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

#include <saaspl/CdHornCompensation.hpp>
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

using CdHorn = CdHornCompensation<accurate_t>;
using CdHornUserConfig = CdHorn::UserConfig;
using CdHornConfig = CdHorn::Config;
template<size_t CHANNELS>
using CdHornProcessor = CdHorn::Processor<CHANNELS>;


struct SumToAll : public Client
{
	static size_t constexpr CROSSOVERS = 8;
	static size_t constexpr BANDS = CROSSOVERS + 1;
	static size_t constexpr CHANNELS = 2;
	static size_t constexpr FILTER_ORDER = 2;
	static size_t constexpr ALLPASS_RC_TIMES = 3;
	static size_t constexpr BAND_RC_TIMES = 7;

private:
	typedef RmsLimiter<sample_t, accurate_t, CROSSOVERS, FILTER_ORDER, ALLPASS_RC_TIMES, BAND_RC_TIMES> Dynamics;

	struct DoubleConfiguration {
		Dynamics::Config dynamicsConfig1;
		Limiter::Config limitingConfig1;
		CdHornConfig cdHornCOnfig1;

		Dynamics::Config dynamicsConfig2;
		Limiter::Config limitingConfig2;
		CdHornConfig cdHornCOnfig2;

		Limiter::Config limitingSubConfig;
	};

	Dynamics::UserConfig dynamicsUserConfig1;
	Limiter::UserConfig limitingUserConfig1;
	CdHornUserConfig cdHornUserConfig1;

	Dynamics::UserConfig dynamicsUserConfig2;
	Limiter::UserConfig limitingUserConfig2;
	CdHornUserConfig cdHornUserConfig2;

	Limiter::UserConfig limitingUserConfig3;

	util::WriteLockFreeRead<DoubleConfiguration, TypeCheckCopyAssignableNonPolymorphic> consumer;
	Dynamics::Config &wDynConf1 = consumer.writerValue().dynamicsConfig1;
	Limiter::Config &wLimConf1 = consumer.writerValue().limitingConfig1;
	CdHornConfig &wCdHornConfig1 = consumer.writerValue().cdHornCOnfig1;

	Dynamics::Config &wDynConf2 = consumer.writerValue().dynamicsConfig2;
	Limiter::Config &wLimConf2 = consumer.writerValue().limitingConfig2;
	CdHornConfig &wCdHornConfig2 = consumer.writerValue().cdHornCOnfig2;

	Limiter::Config &wLimSubConf = consumer.writerValue().limitingSubConfig;

	Dynamics::Processor<CHANNELS> dynamics1;
	Limiter::Processor<CHANNELS> limiter1;
	CdHornProcessor<CHANNELS> cdHorn1;

	Dynamics::Processor<CHANNELS> dynamics2;
	Limiter::Processor<CHANNELS> limiter2;
	CdHornProcessor<CHANNELS> cdHorn2;

	Limiter::Processor<1> subLimiter;

	ClientPort input_0_0;
	ClientPort input_0_1;
	ClientPort input_1_0;
	ClientPort input_1_1;
	ClientPort output_0_0;
	ClientPort output_0_1;
	ClientPort output_1_0;
	ClientPort output_1_1;
	ClientPort output_sub;

	volatile jack_nframes_t lastSampleRate = -1;

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
			subLimiter.initConfigChange();
		}

		jack_default_audio_sample_t samples[4];
		ArrayVector<sample_t, 1> subLimitingVector;

		for (size_t frame = 0; frame < frameCount; frame++) {

			dynamics1.input[0] = *inputLeft1++;
			dynamics1.input[1] = *inputRight1++;
			dynamics2.input[0] = *inputLeft2++;
			dynamics2.input[1] = *inputRight2++;

			dynamics1.process();
			cdHorn1.process(dynamics1.output);
			limiter1.processInPlace(dynamics1.output);

			dynamics2.process();
			cdHorn2.process(dynamics2.output);
			limiter2.processInPlace(dynamics2.output);

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
			subLimitingVector[0] = sub;
			subLimiter.processInPlace(subLimitingVector);
			*subOut++ = subLimitingVector[0];
		}

		return true;
	}

	virtual bool setSamplerate(jack_nframes_t sampleRate)
	{
		if (configure(sampleRate)) {
			lastSampleRate = sampleRate;
			return true;
		}
		return false;
	}

	bool configure(jack_nframes_t sampleRate)
	{
		wDynConf1.configure(dynamicsUserConfig1, sampleRate);
		wLimConf1.configure(limitingUserConfig1, sampleRate);
		wCdHornConfig1.configure(cdHornUserConfig1, sampleRate);

		wDynConf2.configure(dynamicsUserConfig2, sampleRate);
		wLimConf2.configure(limitingUserConfig2, sampleRate);
		wCdHornConfig2.configure(cdHornUserConfig2, sampleRate);

		wLimSubConf.configure(limitingUserConfig3, sampleRate);

		return consumer.write();
	}

	bool reconfigure()
	{
		if (lastSampleRate < 1) {
			throw std::runtime_error("Cannot reconfigure if no initial config was done");
		}
		return configure(lastSampleRate);
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
		limiter1(2048, consumer.readerValue().limitingConfig1, consumer.readerValue().dynamicsConfig1.valueRc),
		dynamics2(consumer.readerValue().dynamicsConfig2),
		limiter2(2048, consumer.readerValue().limitingConfig2, consumer.readerValue().dynamicsConfig2.valueRc),
		subLimiter(2048, consumer.readerValue().limitingSubConfig, consumer.readerValue().dynamicsConfig2.valueRc),
		cdHorn1(consumer.readerValue().cdHornCOnfig1),
		cdHorn2(consumer.readerValue().cdHornCOnfig2)
	{
		dynamicsUserConfig1.frequencies.assign(frequencies);
		dynamicsUserConfig1.allPassRcs.assign(allPassRcTimes);
		dynamicsUserConfig1.bandRcs.assign(bandRcTimes);
		dynamicsUserConfig1.bandThreshold.assign(bandThreshold1);
		dynamicsUserConfig1.threshold = threshold1;

		limitingUserConfig1.setThreshold(saaspl::min(1.0, threshold1 * 2));
		limitingUserConfig1.setAttackTime(0.003);
		limitingUserConfig1.setReleaseTime(0.003);
		limitingUserConfig1.setSmoothingTime(0.001);
		limitingUserConfig1.setPredictionTime(0.004);

		cdHornUserConfig1.setBypass(true);

		dynamicsUserConfig2.frequencies.assign(frequencies);
		dynamicsUserConfig2.allPassRcs.assign(allPassRcTimes);
		dynamicsUserConfig2.bandRcs.assign(bandRcTimes);
		dynamicsUserConfig2.bandThreshold.assign(bandThreshold2);
		dynamicsUserConfig2.threshold = threshold2;
		dynamicsUserConfig2.seperateSubChannel = false;

		limitingUserConfig2.setThreshold(saaspl::min(1.0, threshold2 * 2));
		limitingUserConfig2.setAttackTime(0.003);
		limitingUserConfig2.setReleaseTime(0.003);
		limitingUserConfig2.setSmoothingTime(0.001);
		limitingUserConfig2.setPredictionTime(0.004);

		cdHornUserConfig2.setBypass(true);

		limitingUserConfig3.setThreshold(saaspl::max(saaspl::min(1.0, threshold1 * 2), saaspl::min(1.0, threshold2 * 2)));
		limitingUserConfig3.setAttackTime(0.003);
		limitingUserConfig3.setReleaseTime(0.003);
		limitingUserConfig3.setSmoothingTime(0.001);
		limitingUserConfig3.setPredictionTime(0.004);

		finishDefiningPorts();
	};

	void toggleCdHorn()
	{
		cdHornUserConfig1.setBypass(!cdHornUserConfig1.getBypass());
		cdHornUserConfig2.setBypass(!cdHornUserConfig2.getBypass());
		reconfigure();
	}

	void rotateCdHornfrequency()
	{
		double f= cdHornUserConfig1.getTopFrequency();
		if (f < 24000) {
			f += 1000;
		}
		else {
			f = 16000;
		}
		cdHornUserConfig1.setTopFrequency(f);
		cdHornUserConfig2.setTopFrequency(f);
		reconfigure();
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

int main(int count, char * arguments[]) {
	signal(SIGINT, signal_callback_handler);
	signal(SIGTERM, signal_callback_handler);
	signal(SIGABRT, signal_callback_handler);

//	ArrayVector<accurate_t, SumToAll::CROSSOVERS> frequencies;
//	frequencies[0] = 80;
//	frequencies[1] = 180;
//	frequencies[2] = 1566;
//	frequencies[3] = 4500;//3200
//	frequencies[4] = 6500;
	ArrayVector<accurate_t, SumToAll::CROSSOVERS> frequencies;
	frequencies[0] = 83;
	frequencies[1] = 166;
//	frequencies[2] = 160 * sqrt(2);
//	frequencies[3] = 320;
//	frequencies[4] = 320 * sqrt(2);
//	frequencies[5] = 640; // 1200
//	frequencies[6] = 640 * sqrt(2);
//	frequencies[7] = 1280;//3200
	frequencies[2] = 1566;
	frequencies[3] = 1566 * sqrt(2);//3200
	frequencies[4] = 3132;
	frequencies[5] = 3132 * sqrt(2);//3200
	frequencies[6] = 6264;
	frequencies[7] = 6264 * sqrt(2);//3200

	ArrayVector<accurate_t, SumToAll::ALLPASS_RC_TIMES> allPassRcTimes;
	allPassRcTimes[0] = 0.33;
	allPassRcTimes[1] = 0.33;
	allPassRcTimes[2] = 0.66;

	ArrayVector<accurate_t, SumToAll::BAND_RC_TIMES> bandRcTimes;
	bandRcTimes[0] = 0.033;
	for (int i = 1; i < SumToAll::BAND_RC_TIMES; i++) {
		bandRcTimes[i] = 0.050 * pow(0.167 / 0.05, 1.0 * (i - 1) / (SumToAll::BAND_RC_TIMES - 2));
	}

	// Equal log weight spread
	accurate_t minimumFreq= 20;
	accurate_t maximumFreq= 15000;
	ArrayVector<accurate_t, SumToAll::BANDS> bandThresholds1;
	bandThresholds1[0] = frequencies[0] / minimumFreq;
	for (size_t i = 1; i < SumToAll::CROSSOVERS; i++) {
		double f = frequencies[i];
		double warp = 1.0;//(frequencies[0] + 0.25 * f) / (frequencies[0] + 0.5 * f);
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
		case 'c':
		case 'C':
			std::cout << "CD-Horn bypass toggle" << std::endl;
			clientOwner.get().toggleCdHorn();
			modus = Modus::BYPASS;
			break;
		case 'f':
		case 'F':
			std::cout << "CD-Horn frequency change" << std::endl;
			clientOwner.get().rotateCdHornfrequency();
			modus = Modus::FILTER;
			break;
		case 's':
		case 'S':
			std::cout << "CD-Horn slope" << std::endl;
			clientOwner.get().rotateCdHornfrequency();
			modus = Modus::ZERO;
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

