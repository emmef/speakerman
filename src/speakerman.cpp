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
#include <memory>
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

#include <saaspl/Crossovers.hpp>
#include <saaspl/Delay.hpp>
#include <saaspl/RmsLimiter.hpp>
#include <saaspl/TypeCheck.hpp>
#include <saaspl/WriteLockFreeRead.hpp>


#include <saaspl/BandSummer.hpp>
#include <saaspl/CdHornCompensation.hpp>
#include <saaspl/Limiter.hpp>
#include <saaspl/LinkwitzRileyCrossover.hpp>
#include <saaspl/MultibandRmsLimiter.hpp>
#include <saaspl/VolumeMatrix.hpp>

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

static accurate_t crossoverFrequencies[] = {
//		168, 1566, 2500, 6300
		80, 168, 1566, 6300
//		80, 168, 1000, 2700
//		80, 80 * M_SQRT2,
//		160, 160 * M_SQRT2,
//		320, 320 * M_SQRT2,
//		640, 640 * M_SQRT2,
//		1280, 1280 * M_SQRT2,
//		2560, 2560 * M_SQRT2,
//		5120, 5120 * M_SQRT2,
//		10240
};

struct SumToAll : public Client
{
	static size_t constexpr CROSSOVERS = sizeof(crossoverFrequencies) / sizeof(accurate_t);
	static size_t constexpr BANDS = CROSSOVERS + 1;
	static size_t constexpr CHANNELS = 2;
	static size_t constexpr FILTER_ORDER = 2;
	static size_t constexpr RC_TIMES = 20;

private:
	using Volume = VolumeMatrix<jack_default_audio_sample_t, accurate_t, sample_t>;
	using CrossoverConfig = CrossoverUserConfig<jack_nframes_t, 4>;
	using Crossover = LinkwitzRiley<sample_t, accurate_t, accurate_t, 4>;
	using CrossoverData = Crossover::ConfigData;
	using CrossoverProcessor = Crossover::Crossover;
	using RmsLimiterConfig = MultibandRmsLimiter::UserConfig<sample_t>;
	using RmsLimiterData = MultibandRmsLimiter::ConfigData<sample_t, accurate_t, BANDS, RC_TIMES>;
	using RmsLimiterProcessor = MultibandRmsLimiter::Processor<sample_t, accurate_t, BANDS, RC_TIMES, CHANNELS>;
	using Summer = BandSummer::Processor<sample_t>;
	using LimiterConfig = Limiter::UserConfig;
	using LimiterData = Limiter::Config;
	using LimiterProcessor = Limiter::Processor<sample_t>;
	using CdHornConfig = CdHornCompensation<accurate_t>::UserConfig;
	using CdHornData = CdHornCompensation<accurate_t>::Config;
	using CdHornProcessor = CdHornCompensation<accurate_t>::Processor<CHANNELS * 2>;

	typedef RmsLimiter<sample_t, accurate_t, CROSSOVERS, FILTER_ORDER, RC_TIMES> Dynamics;

	struct GroupUserConfiguration
	{
		RmsLimiterConfig rms;
		size_t summerSplitIndex;
		bool summerSumSplitPart;
		LimiterConfig limiter;
		CdHornConfig cdHorn;
	};

	struct GroupConfiguration
	{
		RmsLimiterData rms;
		size_t summerSplitIndex;
		bool summerSumSplitPart;
		LimiterData limiter;
		CdHornData cdHorn;
	};

	struct GroupsUserConfiguration
	{
		CrossoverConfig crossover;
		GroupUserConfiguration group1;
		GroupUserConfiguration group2;
		LimiterConfig limiterSub;
	};

	struct GroupsConfiguration {
		CrossoverData crossover;
		GroupConfiguration group1;
		GroupConfiguration group2;
		LimiterData limiterDataSub;
	};

	GroupsUserConfiguration userConfiguration;

	util::WriteLockFreeRead<GroupsConfiguration, TypeCheckCopyAssignableNonPolymorphic> consumer;

	GroupsConfiguration & writeConfiguration = consumer.writerValue();
	GroupsConfiguration & readConfiguration = consumer.readerValue();

	CrossoverProcessor crossover;
	RmsLimiterProcessor rmsLimiter1;
	Summer summer1;
	LimiterProcessor limiter1;
	CdHornProcessor cdHorn1;

	RmsLimiterProcessor rmsLimiter2;
	Summer summer2;
	LimiterProcessor limiter2;
	CdHornProcessor cdHorn2;

	LimiterProcessor limiterSub;

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
			// update local config data
		}

		if (modus == Modus::BYPASS) {
			for (size_t frame = 0; frame < frameCount; frame++) {

				*outputLeft1++ = *inputLeft1++;
				*outputRight1++ = *inputRight1++;

				*outputLeft2++ = *inputLeft2++;
				*outputRight2++ = *inputRight2++;

				*subOut++ = 0;
			}

			return true;
		}

		// Apply volumes, splitter, rms-limiter, summer, limiter and speaker correction

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
	SumToAll(
			ArrayVector<accurate_t, CROSSOVERS> &frequencies,
			ArrayVector<accurate_t, RC_TIMES> &rcTimes,
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
		dynamicsUserConfig1.rcs.assign(rcTimes);
		dynamicsUserConfig1.bandThreshold.assign(bandThreshold1);
		dynamicsUserConfig1.threshold = threshold1;
		dynamicsUserConfig1.seperateSubChannel = true;

		limitingUserConfig1.setThreshold(saaspl::min(1.0, threshold1 * 2));
		limitingUserConfig1.setAttackTime(0.003);
		limitingUserConfig1.setReleaseTime(0.006);
		limitingUserConfig1.setSmoothingTime(0.003);
		limitingUserConfig1.setPredictionTime(0.004);

		cdHornUserConfig1.setBypass(true);

		dynamicsUserConfig2.frequencies.assign(frequencies);
		dynamicsUserConfig2.rcs.assign(rcTimes);
		dynamicsUserConfig2.bandThreshold.assign(bandThreshold2);
		dynamicsUserConfig2.threshold = threshold2;
		dynamicsUserConfig2.seperateSubChannel = true;

		limitingUserConfig2.setThreshold(saaspl::min(1.0, threshold2 * 2.5));
		limitingUserConfig2.setAttackTime(0.003);
		limitingUserConfig2.setReleaseTime(0.006);
		limitingUserConfig2.setSmoothingTime(0.003);
		limitingUserConfig2.setPredictionTime(0.004);

		cdHornUserConfig2.setBypass(true);

		limitingUserConfig3.setThreshold(saaspl::max(saaspl::min(1.0, threshold1 * 2), saaspl::min(1.0, threshold2 * 2)));
		limitingUserConfig3.setAttackTime(0.003);
		limitingUserConfig3.setReleaseTime(0.006);
		limitingUserConfig3.setSmoothingTime(0.003);
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

	void rotateThresholds()
	{
		rotateThreshold(limitingUserConfig1, wLimConf1);
		rotateThreshold(limitingUserConfig2, wLimConf2);
		rotateThreshold(limitingUserConfig3, wLimSubConf);
		consumer.write(200);
	}

	void rotateThreshold(Limiter::UserConfig &userConfig, Limiter::Config &config)
	{
		double threshold = userConfig.getThreshold();
		threshold *= pow(2, 0.25);

		if (threshold >= 1.0) {
			threshold = 0.25;
		}
		userConfig.setThreshold(threshold);
		config.configure(userConfig, lastSampleRate);
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

	saaspl::Crossovers<accurate_t> crossovers(SumToAll::CROSSOVERS, SumToAll::FILTER_ORDER * 2, 20, 20000);

	ArrayVector<accurate_t, SumToAll::CROSSOVERS> frequencies;
	for (size_t i = 0; i < SumToAll::CROSSOVERS; i++) {
		frequencies[i] = crossoverFrequencies[i];
	}

	ArrayVector<accurate_t, SumToAll::RC_TIMES> rcTimes;
	ArrayVector<accurate_t, SumToAll::RC_TIMES> bandRcTimes;
	bandRcTimes[0] = 0.010;
	rcTimes[0] = bandRcTimes[0];
	double startTime = 0.040;
	double endTime = 0.60;
	for (int i = 1; i < SumToAll::RC_TIMES; i++) {
		rcTimes[i] = startTime * pow(endTime / startTime, 1.0 * (i - 1) / (SumToAll::RC_TIMES - 2));
	}

	// Equal log weight spread
	accurate_t minimumFreq= 31.5;
	accurate_t maximumFreq= 12500;
	ArrayVector<accurate_t, SumToAll::BANDS> bandThresholds1;
	
	Array<accurate_t> freqs(SumToAll::CROSSOVERS + 2);
	freqs[0] = minimumFreq;
	for (size_t i = 0; i < SumToAll::CROSSOVERS; i++) {
		freqs[i + 1] = crossoverFrequencies[i];
	}
	freqs[SumToAll::CROSSOVERS + 1] = maximumFreq;
	accurate_t warpPower = 1.3;
	
	for (size_t band = 0; band <= SumToAll::CROSSOVERS; band++) {
		accurate_t weight = 0.0;
		for (accurate_t f = freqs[band]; f < freqs[band + 1]; f += 1.0) {
			accurate_t fWeight = frequencyWeight(f, minimumFreq, 5000, warpPower);
			weight += fWeight;
		}
		bandThresholds1[band] = weight;
	}
	accurate_t scale;
	accurate_t maxThreshold = 0.0;
	for (size_t i = 0; i < SumToAll::BANDS; i++) {
		if (bandThresholds1[i] > maxThreshold) {
			maxThreshold = bandThresholds1[i];
		}
	}
	scale = 0.0;
	for (size_t i = 0; i < SumToAll::BANDS; i++) {
		scale += bandThresholds1[i];
	}
	for (size_t i = 0; i < SumToAll::BANDS; i++) {
		bandThresholds1[i] = bandThresholds1[i] / scale;
	}
	scale = 0.0;
	for (size_t i = 0; i < SumToAll::BANDS; i++) {
		scale += bandThresholds1[i];
	}
	for (size_t i = 0; i < SumToAll::BANDS; i++) {
		bandThresholds1[i] = bandThresholds1[i] / scale;
	}
//	for (size_t i = 0; i < SumToAll::CROSSOVERS; i++) {
//		if (crossoverFrequencies[i] < 200) {
//			bandThresholds1[i] = 0.8;
//		}
//	}

	ArrayVector<accurate_t, SumToAll::BANDS> bandThresholds2;
	bandThresholds2 = bandThresholds1;

	accurate_t threshold1 = 0.25;
	accurate_t threshold2 = 0.25;

	clientOwner.setClient(new SumToAll(frequencies, rcTimes, threshold1, bandThresholds1, threshold2, bandThresholds2));

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
			if (modus == Modus::BYPASS) {
				std::cout << "Bypass off" << std::endl;
				modus = Modus::FILTER;
			}
			else {
				std::cout << "Bypass on" << std::endl;
				modus = Modus::BYPASS;
			}
			break;
		case 'c':
		case 'C':
			std::cout << "CD-Horn bypass toggle" << std::endl;
			clientOwner.get().toggleCdHorn();
			break;
		case 'f':
		case 'F':
			std::cout << "CD-Horn frequency change" << std::endl;
			clientOwner.get().rotateCdHornfrequency();
			break;
		case 's':
		case 'S':
			std::cout << "CD-Horn slope" << std::endl;
			clientOwner.get().rotateCdHornfrequency();
			break;
		case 'h':
		case 'H':
			std::cout << "High-pass" << std::endl;
			modus = Modus::HIGH;
			break;
		case 't' :
		case 'T' :
			std::cout << "Adapting threshold" << std::endl;
			clientOwner.get().rotateThresholds();
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

