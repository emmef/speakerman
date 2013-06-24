/*
 * SingleMulti.hpp
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

#ifndef SMS_SPEAKERMAN_SINGLEMULTI_GUARD_H_
#define SMS_SPEAKERMAN_SINGLEMULTI_GUARD_H_

#include <simpledsp/Array.hpp>
#include <speakerman/Frame.hpp>

namespace speakerman {

template <typename Sample> struct SingleMulti
{
	Frame<Sample> &single;
	const Array<Sample> multiSamples;
	const Array<VariableFrame<Sample> > vectors;

	Frame<Sample> &getMulti(size_t index) const
	{
		return vectors[index];
	}

	Frame<Sample> &getSingle() const
	{
		return single;
	}

	size_t multiCount() const
	{
		return vectors.length();
	}

	SingleMulti(Frame<Sample> &sngl, size_t splitCount) :
		single(sngl),
		multiSamples(Precondition::validPositiveProduct<Sample>(sngl.size(), splitCount)),
		vectors(splitCount)
	{
		const size_t size = single.size();
		size_t offset = 0;
		for (size_t i = 0; i < splitCount; i++, offset += size) {
			vectors[i].init(size, multiSamples.unsafeData() + offset);
		}
	}

	~SingleMulti()
	{
	}
};


} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_SINGLEMULTI_GUARD_H_ */
