/*
 * tdap/IndexPolicy.hpp
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

#ifndef TDAP_INDEX_POLICY_HEADER_GUARD
#define TDAP_INDEX_POLICY_HEADER_GUARD

#include <stdexcept>

namespace tdap {

#ifndef TDAP_INDEX_POLICY_METHODS_CHECKED
	static constexpr bool defaultMethodIndexPolicy = true;
#elif TDAP_INDEX_POLICY_METHODS_CHECKED == 0
	static constexpr bool defaultMethodIndexPolicy = false;
#else
	static constexpr bool defaultMethodIndexPolicy = true;
#endif

#ifndef TDAP_INDEX_POLICY_OPERATORS_CHECKED
	static constexpr bool defaultOperatorIndexPolicy = true;
#elif TDAP_INDEX_POLICY_OPERATORS_CHECKED == 0
	static constexpr bool defaultOperatorIndexPolicy = false;
#else
	static constexpr bool defaultOperatorIndexPolicy = true;
#endif

struct IndexPolicy
{
	static inline size_t force(size_t index, size_t size)
	{
		if (index < size) {
			return index;
		}
		throw std::out_of_range("Index out of range");
	}

	static inline size_t array(size_t index, size_t size)
	{
		return defaultOperatorIndexPolicy ? force(index, size) : index;
	}

	static size_t method(size_t index, size_t size)
	{
		return defaultMethodIndexPolicy ? force(index, size) : index;
	}
};


} /* End of name space tdap */

#endif /* TDAP_INDEX_POLICY_HEADER_GUARD */
