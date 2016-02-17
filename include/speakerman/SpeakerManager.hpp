/*
 * SpeakerManager.hpp
 *
 * Part of 'Speaker management system'
 *
 * Copyright (C) 2013-2014 Michel Fleur.
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

#ifndef SMS_SPEAKERMAN_SPEAKERMANAGER_GUARD_H_
#define SMS_SPEAKERMAN_SPEAKERMANAGER_GUARD_H_

#include <iostream>
#include <speakerman/jack/JackProcessor.hpp>


namespace speakerman {

class SpeakerManager : public JackProcessor
{
	PortDefinitions portDefinitions_;

	static constexpr size_t IN_1_1 = 0;
	static constexpr size_t IN_1_2 = 1;
	static constexpr size_t IN_2_1 = 2;
	static constexpr size_t IN_2_2 = 3;
	static constexpr size_t OUT_1_1 = 4;
	static constexpr size_t OUT_1_2 = 5;
	static constexpr size_t OUT_2_1 = 6;
	static constexpr size_t OUT_2_2 = 7;
	static constexpr size_t OUT_SUB = 8;

protected:
	virtual const PortDefinitions &getDefinitions() override { return portDefinitions_; }
	virtual bool onMetricsUpdate(ProcessingMetrics metrics) override
	{
		std::cout << "Updated metrics: {rate:" << metrics.rate << ", bsize:" << metrics.bufferSize << "}" << std::endl;
		return true;
	}
	virtual void onPortsRegistered() override
	{
		std::cout << "No action on ports registered" << std::endl;
	}
	virtual bool process(jack_nframes_t frames, const Ports &ports) override
	{
		ports.getBuffer(OUT_1_1).copy(ports.getBuffer(IN_1_1));
		ports.getBuffer(OUT_1_2).copy(ports.getBuffer(IN_1_2));
		ports.getBuffer(OUT_2_1).copy(ports.getBuffer(IN_2_1));
		ports.getBuffer(OUT_2_2).copy(ports.getBuffer(IN_2_2));

		for (size_t i = 0; i < frames; i++) {
			ports.getBuffer(OUT_SUB)[i] = 0.25 * (
					ports.getBuffer(IN_1_1)[i] +
					ports.getBuffer(IN_1_2)[i] +
					ports.getBuffer(IN_2_1)[i] +
					ports.getBuffer(IN_2_2)[i]);
		}
		return true;
	}

public:
	virtual bool needBufferSizeCallback() const override { return false; }
	virtual bool needSampleRateCallback() const override { return true; }

	SpeakerManager() :
		portDefinitions_(16, 32)
	{
		portDefinitions_.addInput("in_1_channel_1");
		portDefinitions_.addInput("in_1_channel_2");
		portDefinitions_.addInput("in_2_channel_1");
		portDefinitions_.addInput("in_2_channel_2");

		portDefinitions_.addOutput("out_1_channel_1");
		portDefinitions_.addOutput("out_1_channel_2");
		portDefinitions_.addOutput("out_2_channel_1");
		portDefinitions_.addOutput("out_2_channel_2");
		portDefinitions_.addOutput("out_subwoofer");
	}
};

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_SPEAKERMANAGER_GUARD_H_ */
