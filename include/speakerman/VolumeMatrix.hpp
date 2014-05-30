/*
 * VolumeMatrix.hpp
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

#ifndef SMS_SPEAKERMAN_VOLUMEMATRIX_GUARD_H_
#define SMS_SPEAKERMAN_VOLUMEMATRIX_GUARD_H_

#include <simpledsp/Matrix.hpp>

namespace speakerman {

using namespace simpledsp;

template<typename Sample, size_t ROWS, size_t COLUMNS> class VolumeMatrix
{
	Matrix<Sample, ROWS, COLUMNS> matrix;
	const Sample min, max;


public:
	VolumeMatrix(Sample minimumValue, Sample maximumValue) :
		min(minimumValue < maximumValue ? minimumValue : maximumValue),
		max(minimumValue < maximumValue ? maximumValue : minimumValue) {}

	size_t columns() const
	{
		return matrix.columns();
	}
	size_t rows() const
	{
		return matrix.rows();
	}
	void set(size_t column, size_t row, Sample value)
	{
		matrix(column, row) = value < min ? min : value > max ? max : value;
	}
	const Sample get(size_t column, size_t row) const
	{
		return matrix(column, row);
	}
	const Sample operator() (size_t column, size_t row) const
	{
		return matrix(column, row);
	}
	void multiply(const array<Sample, COLUMNS> &input, array<Sample, ROWS> &output) const
	{
		matrix.multiply(input, output);
	}
	void multiply(const FixedBuffer<Sample, COLUMNS> &input, FixedBuffer<Sample, ROWS> &output) const
	{
		matrix.multiply(input, output);
	}
	void multiply(const vector<Sample> &input, vector<Sample> &output) const
	{
		matrix.multiply(input, output);
	}
	void multiply(const Array<Sample> &input, Array<Sample> &output) const
	{
		matrix.multiply(input, output);
	}
	Sample getMinimum() const
	{
		return min;
	}
	Sample getMaximum() const
	{
		return max;
	}

};


} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_VOLUMEMATRIX_GUARD_H_ */
