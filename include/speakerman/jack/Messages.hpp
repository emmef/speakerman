/*
 * Messages.hpp
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

#ifndef SMS_SPEAKERMAN_MESSAGES_GUARD_H_
#define SMS_SPEAKERMAN_MESSAGES_GUARD_H_

#include <jack/jack.h>

namespace speakerman {

namespace jack {

	inline string statusMessage(jack_status_t status)
	{
		if (status == 0) {
			return "";
		}
		string message = "Status";

		if (status & JackStatus::JackFailure) {
			message += "/ Overall operation failed.";
		}
		if (status & JackInvalidOption) {
			message += "/ The operation contained an invalid or unsupported option.";
		}
		if (status & JackNameNotUnique) {
			message += "/ The desired client name was not unique";
		}
		if (status & JackServerStarted) {
			message += "/ The JACK server was started as a result of this operation.";
		}
		if (status & JackServerFailed) {
			message += "/ Unable to connect to the JACK server.";
		}
		if (status & JackServerFailed) {
			message += "/ Unable to connect to the JACK server.";
		}
		if (status & JackServerError) {
			message += "/ Communication error with the JACK server.";
		}
		if (status & JackNoSuchClient) {
			message += "/ Requested client does not exist.";
		}
		if (status & JackLoadFailure) {
			message += "/ Unable to load internal client";
		}
		if (status & JackInitFailure) {
			message += "/ Unable to initialize client";
		}
		if (status & JackShmFailure) {
			message += "/ Unable to access shared memory";
		}
		if (status & JackVersionError) {
			message += "/ Client's protocol version does not match";
		}
		if (status & JackBackendError) {
			message += "/ Backend error";
		}
		if (status & JackClientZombie) {
			message += "/ Client zombified failure";
		}

		return message;
	}


} /* End of namespace jack */
} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_MESSAGES_GUARD_H_ */
