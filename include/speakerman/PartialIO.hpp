/*
 * PartialIO.hpp
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

#ifndef SMS_SPEAKERMAN_PARTIALIO_GUARD_H_
#define SMS_SPEAKERMAN_PARTIALIO_GUARD_H_

#include <iostream>
#include <simpledsp/Array.hpp>
#include <simpledsp/Precondition.hpp>

namespace speakerman {

template <typename Sample> class PartialIO
{
	Array<Sample> unconnected_nodes;
	Array<Sample *> node;
public:
	PartialIO(const size_t inputs) :
		unconnected_nodes(inputs, CopyZero::ZERO),
		node(inputs)
	{
		disconnect();
	}

	size_t length() const
	{
		return node.length();
	}

	void connect(Array<Sample> &array, size_t offset)
	{
		size_t endIndex = offset + node.length();

		if (array.length() >= endIndex) {
			for (size_t i = 0, j = offset; j < node.length(); i++, j++) {
				node[i] = &array[j];
			}
			std::cout << "Start address is " << &array[offset] << std::endl;
			return;
		}

		throw std::invalid_argument("Combination of inputs and offset won't fit in array");
	}

	void disconnect()
	{
		for (size_t i = 0; i < node.length(); i++) {
			node[i] = &unconnected_nodes[i];
		}
	}

	Sample &operator[](size_t index) const
	{
		return *node[index];
	}

	void copy()
	{
		for (size_t i = 0; i < node.length(); i++) {
			unconnected_nodes[i] = *node[i];
		}
	}

	Sample &intern(size_t index) const
	{
		return unconnected_nodes[index];
	}
};


} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_PARTIALIO_GUARD_H_ */
