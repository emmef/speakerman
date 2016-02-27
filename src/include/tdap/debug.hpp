/*
 * tdap/debug.hpp
 *
 * Part of TdAP
 * Time-domain Audio Processing
 * Copyright (C) 2015 Michel Fleur.
 * Source https://bitbucket.org/emmef/tdap
 * Email  tdap@emmef.org
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

#ifndef TDAP_DEBUG_HEADER_GUARD
#define TDAP_DEBUG_HEADER_GUARD

/**
 * Defining some facilities that can be used in unit tests
 * specific for Array. These unit tests should set the
 * TDAP_DEBUG_FACILITY before including <saaspl/Array.hpp>
 * and preferably ONLY include that header.
 * Actually, this section just defines the define to
 * create the facilities, which results in something that
 * is optimized away when TDAP_DEBUG_FACILITY is not set.
 */
#ifdef TDAP_DEBUG_FACILITY_VERBOSE
#	include <iostream>
#	define TDAP_DEBUG_FACILITY 1
#endif

namespace tdap {

#ifdef TDAP_DEBUG_FACILITY
	static int debugArrayRegisterCount(bool add, int &count)
	{
		static constexpr unsigned MAX_COUNTS = 100;
		static unsigned number = 0;
		static int *array[MAX_COUNTS];

		if (add) {
			if (number < MAX_COUNTS) {
				array[number++] = &count;
				return 1;
			}
			return 0;
		}
		else {
#	ifdef TDAP_DEBUG_FACILITY_VERBOSE
			std::cout << "TDAP_DEBUG_FACILITY: reset all counts" << std::endl;
#	endif
			for (unsigned i = 0; i < number; i++) {
				*array[i] = 0;
			}
			return 1;
		}
	}
	static void debugArrayResetCounts()
	{
		int dummy;
		debugArrayRegisterCount(false, dummy);
	}
#	ifdef TDAP_DEBUG_FACILITY_VERBOSE
#		define TDAP_DEBUG_DEF_COUNT(name) \
		static int debug##name##Count = 0; \
		static inline void debug##name##Call() { \
			static const int delta = debugArrayRegisterCount(true, debug##name##Count); \
			debug##name##Count += delta; \
			std::cout << "\t" << #name << "(" << debug##name##Count << ")" << std::endl; \
		}\
		static inline void debug##name##Zero() { \
			debug##name##Count = 0; \
		}
#	else
#		define TDAP_DEBUG_DEF_COUNT(name) \
		static int debug##name##Count = 0; \
		static inline void debug##name##Call() { \
			static const int delta = debugArrayRegisterCount(true, debug##name##Count); \
			debug##name##Count += delta; \
		}\
		static inline void debug##name##Zero() { \
			debug##name##Count = 0; \
		}
#	endif
#else
#	define TDAP_DEBUG_DEF_COUNT(name) \
	static inline void debug##name##Zero() { } \
	static inline void debug##name##Call() { }

	static inline void debugArrayResetCounts() { }
#endif

} /* End of name space tdap */

#endif /* TDAP_DEBUG_HEADER_GUARD */
