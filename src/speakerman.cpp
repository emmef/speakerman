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
#include <speakerman/utils/Mutex.hpp>
#include <speakerman/JackConnector.hpp>

using namespace speakerman;

class SumToAll : public JackClient
{
protected:
	virtual int process(jack_nframes_t frameCount)
	{
		const jack_default_audio_sample_t* input1 = getInput(0, frameCount);
		const jack_default_audio_sample_t* input2 = getInput(1, frameCount);
		jack_default_audio_sample_t* output1 = getOutput(0, frameCount);
		jack_default_audio_sample_t* output2 = getOutput(1, frameCount);

		for (jack_nframes_t i = 0; i < frameCount; i++) {
			jack_default_audio_sample_t x = input1[i] + input2[i];
			output1[i] = x;
			output2[i] = x;
		}

		return 0;
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
	SumToAll(string name) : JackClient(name) {
		addInput("left_in");
		addInput("right_in");

		addOutput("left_out");
		addOutput("right_out");
	};


};


int main(int count, char * arguments[]) {
	SumToAll client("Speakerman");
	client.open();
	client.activate();
	std::chrono::milliseconds duration(10000);
	std::this_thread::sleep_for( duration );
	return 0;
}

