/*
 * JackPort.hpp
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

#ifndef SMS_SPEAKERMAN_JACKPORT_GUARD_H_
#define SMS_SPEAKERMAN_JACKPORT_GUARD_H_

#include <jack/jack.h>
#include <string>

namespace speakerman {

enum class Direction
{
	IN, OUT
};

class JackPort
{
	std::string name;
	std::string connectNamePattern;
	std::string connectTypePattern;
	unsigned long connectFlags;
	Direction direction;
	jack_port_t * port = nullptr;

	void registerPort(jack_client_t *client);
	signed connect(jack_client_t *, bool disconnectPrevious);
	void deRegisterPort(jack_client_t *client);
	jack_default_audio_sample_t * getBuffer(jack_nframes_t frames);

public:
	JackPort(Direction direction, std::string name, std::string namePattern, std::string typePattern, unsigned long flags);
	JackPort(Direction direction, std::string name);
	~JackPort();
	std::string getName() const
	{
		return name;
	}
	friend class JackProcessor;
};


} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_JACKPORT_GUARD_H_ */
