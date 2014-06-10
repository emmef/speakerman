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

template<typename T, size_t ROWS, size_t COLUMNS> class VolumeMatrix
{
	Matrix<T, ROWS, COLUMNS> matrix;
	const T min, max;


public:
	VolumeMatrix(T minimumValue, T maximumValue) :
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
	void set(size_t column, size_t row, T value)
	{
		matrix(column, row) = value < min ? min : value > max ? max : value;
	}
	const T get(size_t column, size_t row) const
	{
		return matrix(column, row);
	}
	const T operator() (size_t column, size_t row) const
	{
		return matrix(column, row);
	}

	template<size_t SCALE1, size_t SCALE2>
	void multiply(const Vector<T, COLUMNS, SCALE1> &input,  Vector<T, ROWS, SCALE2> &output)
	{
		matrix.multiply(input, output);
	}
	T getMinimum() const
	{
		return min;
	}
	T getMaximum() const
	{
		return max;
	}

};


} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_VOLUMEMATRIX_GUARD_H_ */
