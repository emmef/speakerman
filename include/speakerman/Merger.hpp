/*
 * Merger.hpp
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

#ifndef SMS_SPEAKERMAN_MERGER_GUARD_H_
#define SMS_SPEAKERMAN_MERGER_GUARD_H_

#include <speakerman/SingleMulti.hpp>

namespace speakerman {

template <typename Sample> class Merger
{
	SingleMulti<Sample> merger;

public:
	Merger(FixedFrame<Sample> &out, size_t mergeCount) :
		merger(out, mergeCount)
	{
	}

	Frame<Sample> &output() const
	{
		return merger.getSingle();
	}

	Frame<Sample> &input(size_t index) const
	{
		return merger.getMulti(index);
	}

	size_t inputs() const
	{
		return merger.multiCount();
	}

	virtual void merge()
	{
		merger.single.clear();
		for (size_t j = 0; j < inputs(); j++) {
			merger.single.add(merger.vectors.unsafeData()[j]);
		}
	}

	virtual ~Merger()
	{
	}
};

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_MERGER_GUARD_H_ */
