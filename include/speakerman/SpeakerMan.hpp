/*
 * SpeakerMan.hpp
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

#ifndef SMS_SPEAKERMAN_SPEAKERMAN_GUARD_H_
#define SMS_SPEAKERMAN_SPEAKERMAN_GUARD_H_

#include <jack/jack.h>
#include <speakerman/Frame.hpp>
#include <speakerman/Mixer.hpp>
#include <speakerman/AnalogCrossOver.hpp>


namespace speakerman {

static constexpr size_t MAX_CHANNELS = 16;
static constexpr size_t SUB_FILTER_ORDER = 2;

class SpeakerManager
{
	AnalogCrossOver<jack_default_audio_sample_t, accurate_t, SUB_FILTER_ORDER, MAX_CHANNELS> x;
};

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_SPEAKERMAN_GUARD_H_ */
