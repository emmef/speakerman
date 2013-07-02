/*
 * JackProcessor.cpp
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

#include <speakerman/JackProcessor.hpp>

namespace speakerman {

JackProcessor::JackProcessor() : inputs(10), outputs(10)
{

}

void JackProcessor::registerPorts(jack_client_t *client)
{
	for (size_t i = 0 ; i < inputs.size(); i++) {
		inputs.get(i).registerPort(client);
	}
	for (size_t i = 0 ; i < outputs.size(); i++) {
		outputs.get(i).registerPort(client);
	}
}

void JackProcessor::unRegisterPorts(jack_client_t *client)
{
	for (size_t i = 0 ; i < inputs.size(); i++) {
		inputs.get(i).deRegisterPort(client);
	}
	for (size_t i = 0 ; i < outputs.size(); i++) {
		outputs.get(i).deRegisterPort(client);
	}
}

signed JackProcessor::connectPorts(jack_client_t *client, bool disconnectPreviousOutputs, bool disconnectPreviousInputs)
{
	signed total = 0;
	for (size_t i = 0 ; i < inputs.size(); i++) {
		if (inputs.get(i).connect(client, disconnectPreviousOutputs) > 0) {
			total ++;
		}
	}
	for (size_t i = 0 ; i < outputs.size(); i++) {
		if (outputs.get(i).connect(client, disconnectPreviousInputs) > 0) {
			total++;
		}
	}
	return total;
}


const jack_default_audio_sample_t *JackProcessor::getInput(size_t number, jack_nframes_t frameCount) const
{
	return inputs.get(number).getBuffer(frameCount);
}
jack_default_audio_sample_t *JackProcessor::getOutput(size_t number, jack_nframes_t frameCount) const
{
	return outputs.get(number).getBuffer(frameCount);
}

} /* End of namespace speakerman */
