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
	static size_t constexpr GROUPS = 2;
	static size_t constexpr CHANNELS = 2;
	static size_t constexpr FILTER_ORDER = 2;
	static size_t constexpr RC_TIMES = 20;
	static size_t constexpr MAX_SAMPLERATE = 192000;
	static double constexpr MAX_PREDICTION_SECONDS = 0.01;
	static size_t constexpr MAX_PREDICTION_SAMPLES = (size_t)MAX_SAMPLERATE * MAX_PREDICTION_SECONDS;

public:
	using Volume = VolumeMatrix<jack_default_audio_sample_t, accurate_t, sample_t>;
	using CrossoverConfig = CrossoverUserConfig<double, 4>;
	using Crossover = LinkwitzRiley<sample_t, accurate_t, accurate_t, 4, CROSSOVERS, CHANNELS * 2>;
	using CrossoverData = Crossover::ConfigData;
	using CrossoverProcessor = Crossover::Crossover;
	using RmsLimiterConfig = MultibandRmsLimiter::UserConfig<sample_t>;
	using RmsLimiterData = MultibandRmsLimiter::ConfigData<sample_t, accurate_t, BANDS, RC_TIMES>;
	using RmsLimiterProcessor = MultibandRmsLimiter::Processor<sample_t, accurate_t, BANDS, RC_TIMES, CHANNELS>;
	using Summer = BandSummer::Processor<sample_t>;
	using LimiterConfig = Limiter::UserConfig;
	using LimiterData = Limiter::Config;
	using LimiterProcessor = Limiter::FixedChannelProcessor<sample_t, CHANNELS, MAX_PREDICTION_SAMPLES>;
	using CdHornConfig = CdHornCompensation<accurate_t>::UserConfig;
	using CdHornData = CdHornCompensation<accurate_t>::Config<CHANNELS>;
	using CdHornProcessor = CdHornCompensation<accurate_t>::Processor<CHANNELS * 2>;

	typedef RmsLimiter<sample_t, accurate_t, CROSSOVERS, FILTER_ORDER, RC_TIMES> Dynamics;

	struct GroupUserConfiguration
	{
		RmsLimiterConfig rms;
		size_t summerSplitIndex;
		bool summerSumSplitPart;
		LimiterConfig limiter;
		CdHornConfig cdHorn;

		GroupUserConfiguration(const Crossovers<double> &crossovers) :
			rms(crossovers, RC_TIMES, true)
		{

		}
	};

	struct GroupConfiguration
	{
		RmsLimiterData rms;
		size_t summerSplitIndex;
		bool summerSumSplitPart;
		LimiterData limiter;
		CdHornData cdHorn;

		void reconfigure(const GroupUserConfiguration &userConf, jack_nframes_t sampleRate)
		{
			rms.configure(userConf.rms, sampleRate);
			summerSplitIndex = userConf.summerSplitIndex;
			summerSumSplitPart = userConf.summerSumSplitPart;
			limiter.configure(userConf.limiter, sampleRate);
			cdHorn.configure(userConf.cdHorn, sampleRate);
		}
	};

	struct GroupsUserConfiguration
	{
		CrossoverConfig crossover;
		GroupUserConfiguration group[GROUPS];
		LimiterConfig limiterSub;

		GroupsUserConfiguration(const Crossovers<double> &crossovers) :
			crossover(crossovers),
			group({crossovers, crossovers})
		{
		}
	};

	struct GroupsConfiguration {
		CrossoverData crossover;
		GroupConfiguration group[GROUPS];
		GroupConfiguration group2;
		LimiterData limiterDataSub;

		void reconfigure(const GroupsUserConfiguration &userConfig, jack_nframes_t sampleRate, jack_nframes_t frameSize)
		{
			const CrossoverContext<sample_t, double> context(frameSize, CHANNELS, sampleRate);
			crossover = CrossoverData::createConfigurationData(userConfig.crossover, context);
			for (size_t groupNumber = 0; groupNumber < GROUPS; groupNumber++) {
				group[groupNumber].reconfigure(userConfig.group[groupNumber], sampleRate);
			}
			limiterDataSub.configure(userConfig.limiterSub, sampleRate);
		}
	};

	struct GroupProcessor
	{
		CrossoverProcessor crossover;
		RmsLimiterProcessor rmsLimiter;
		Summer summer;
		LimiterProcessor limiter;
		CdHornProcessor cdHorn;

		GroupProcessor() :
			summer(1, true) { }
	};

private:

	GroupsUserConfiguration userConfiguration;

	util::WriteLockFreeRead<GroupsConfiguration, TypeCheckCopyAssignableNonPolymorphic> consumer;

	GroupsConfiguration & writeConfiguration = consumer.writerValue();
	GroupsConfiguration & readConfiguration = consumer.readerValue();

	Volume volume;
	GroupProcessor processor1;
	GroupProcessor processor2;

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

	volatile jack_nframes_t lastSampleRate = 0;
	volatile jack_nframes_t lastBufferSize = 0;


protected:
	virtual bool process(jack_nframes_t frameCount) override
	{
		Array<RefArray<jack_default_audio_sample_t>> inputs(4);
		Array<RefArray<jack_default_audio_sample_t>> outputs(5);

		inputs[0] = RefArray<jack_default_audio_sample_t>(frameCount, input_0_0.getBuffer());
		inputs[1] = RefArray<jack_default_audio_sample_t>(frameCount, input_0_1.getBuffer());
		inputs[2] = RefArray<jack_default_audio_sample_t>(frameCount, input_1_0.getBuffer());
		inputs[3] = RefArray<jack_default_audio_sample_t>(frameCount, input_1_1.getBuffer());

		outputs[0] = RefArray<jack_default_audio_sample_t>(frameCount, output_0_0.getBuffer());
		outputs[1] = RefArray<jack_default_audio_sample_t>(frameCount, output_0_1.getBuffer());
		outputs[2] = RefArray<jack_default_audio_sample_t>(frameCount, output_1_0.getBuffer());
		outputs[3] = RefArray<jack_default_audio_sample_t>(frameCount, output_1_1.getBuffer());
		outputs[4] = RefArray<jack_default_audio_sample_t>(frameCount, output_sub.getBuffer());

		if (consumer.read(true)) {
			// update local config data
		}

		if (bufferSize() == 0 || sampleRate() == 0) {
			return false;
		}
//		if (modus == Modus::BYPASS) {
			outputs[0].copy(0, inputs[0], 0, frameCount);
			outputs[1].copy(0, inputs[1], 0, frameCount);
			outputs[2].copy(0, inputs[2], 0, frameCount);
			outputs[3].copy(0, inputs[3], 0, frameCount);
			outputs[4].zero();

			return true;
//		}

		return true;
	}

	virtual bool setContext(jack_nframes_t newBufferSize, jack_nframes_t newSampleRate) override
	{
		return configure(newBufferSize, newSampleRate);
	}

	bool configure(jack_nframes_t newBufferSize, jack_nframes_t sampleRate)
	{
		writeConfiguration.reconfigure(userConfiguration, sampleRate, newBufferSize);

		return consumer.write();
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
			const Crossovers<accurate_t> &crossovers)
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
		userConfiguration(crossovers),
		volume(CHANNELS * GROUPS, CHANNELS * GROUPS)
	{
		for (size_t ch = 0; ch < CHANNELS * GROUPS; ch++) {
			volume(ch, ch) = 1;
		}
		finishDefiningPorts();
	};

	bool reconfigure()
	{
		if (sampleRate() > 0 && bufferSize() > 0) {
			return configure(bufferSize(), sampleRate());
		}
		throw std::runtime_error("Cannot reconfigure if no initial config was done");
	}

	GroupsUserConfiguration &config()
	{
		return userConfiguration;
	}

	const GroupsConfiguration &effectiveConfig()
	{
		return writeConfiguration;
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

void setRmsLimiterConfig(std::string identifier, SumToAll::RmsLimiterConfig &config)
{
	config.rcs().setSmoothingRc(0.010);
	double startTime = 0.040;
	double endTime = 0.60;
	for (int i = 0; i < SumToAll::RC_TIMES; i++) {
		double time = startTime * pow(endTime / startTime, 1.0 * (i - 1) / (SumToAll::RC_TIMES - 2));
		config.rcs().setRc(i, time);
	}
	config.setMaxBandLevelDifference(4.0);
	config.setThreshold(0.25);
}

void setLimiterConfig(std::string identifier, SumToAll::LimiterConfig &config)
{
	config.setAttackTime(0.003);
	config.setReleaseTime(0.009);
	config.setPredictionTime(0.004);
	config.setSmoothingTime(0.002);
	config.setThreshold(0.75);
}

void setCdHornConfig(std::string identifier, SumToAll::CdHornConfig &config)
{
	config.setStartFrequency(5000);
	config.setTopFrequency(20000);
	config.setSlope(3.0);
	config.setBypass(true);
}

void setGroupDefaults(std::string identifier, SumToAll::GroupUserConfiguration &config)
{
	setRmsLimiterConfig(identifier + ".rms", config.rms);
	setLimiterConfig(identifier + ".limiter", config.limiter);
	setCdHornConfig(identifier + ".cdhorn", config.cdHorn);
	config.summerSplitIndex = 1;
	config.summerSumSplitPart = true;
}

void setDefaults(std::string identifier, SumToAll::GroupsUserConfiguration &config)
{
	static const char * groupNumbers[SumToAll::GROUPS] = { ".group1", ".group2" };
	for (size_t grp = 0; grp < SumToAll::GROUPS; grp++) {
		setGroupDefaults(identifier + groupNumbers[grp], config.group[grp]);
	}
	setLimiterConfig(identifier + ".sub.limiter", config.limiterSub);
}



int main(int count, char * arguments[]) {
	signal(SIGINT, signal_callback_handler);
	signal(SIGTERM, signal_callback_handler);
	signal(SIGABRT, signal_callback_handler);

	saaspl::Crossovers<accurate_t> crossovers(SumToAll::CROSSOVERS, SumToAll::FILTER_ORDER * 2, 20, 20000);

	for (size_t i = 0; i < SumToAll::CROSSOVERS; i++) {
		crossovers.setCrossover(i, crossoverFrequencies[i]);
	}

	clientOwner.setClient(new SumToAll(crossovers));

	SumToAll::GroupsUserConfiguration &config = clientOwner.get().config();

	setDefaults("default", config);

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
		case 'h':
		case 'H':
			std::cout << "High-pass" << std::endl;
			modus = Modus::HIGH;
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

