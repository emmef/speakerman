/*
 * tdap/Rms.hpp
 *
 * Part of TdAP
 * Time-domain Audio Processing
 * Copyright (C) 2015-2016 Michel Fleur.
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

#ifndef TDAP_RMS_HEADER_GUARD
#define TDAP_RMS_HEADER_GUARD

#include <iostream>

#include <type_traits>
#include <cmath>

#include <tdap/Array.hpp>
#include <tdap/Integration.hpp>

namespace tdap {
using namespace std;

template <typename S, size_t N>
class BucketRms
{
	static_assert(is_arithmetic<S>::value, "Expected floating-point type parameter");
	static_assert(N > 0 && N < 1024, "Bucket count out range");

	S bucket_[N];
	size_t bucketNr_ = 0;
	size_t sampleNr_ = 0;
	size_t bucketSize_ = 1;
	S output;
public:
	void zero()
	{
		for (size_t i = 0; i < N; i++) {
			bucket_[i] = 0;
		}
	}

	void setvalue(S value)
	{
		bucketNr_ = 0;
		S square = value * value;
		bucket_[0] = square * sampleNr_;
		S fullBucketValue = square * bucketSize_;
		for (size_t i = 1; i < N; i++) {
			bucket_[i] = fullBucketValue;
		}
	}

	const size_t getWindowSize() const
	{
		return bucketSize_ * N;
	}

	size_t setWindowSize(size_t windowSize)
	{
		size_t samplesPerBucket = bucketSize_;
		bucketSize_ = Value<size_t>::max(1, windowSize / N);
		double scale = 1.0 * bucketSize_ / samplesPerBucket;
		size_t i;
		for (i = 0; i < bucketNr_; i++) {
			bucket_[i] *= scale;
		}
		for (i++; i < N; i++) {
			bucket_[i] *= scale;
		}
		return getWindowSize();
	}

	S addAndGet(S sample)
	{
		return addAndGetSquare(sample * sample);
	}

	S addSquareAndGet(S square)
	{
		bucket_[bucketNr_] += square;
		sampleNr_++;
		if (sampleNr_ == bucketSize_) {
			sampleNr_ = 0;
			S sum = 0.0;
			for (size_t i = 0; i < N; i++) {
				sum += bucket_[i];
			}
			if (bucketNr_ < N - 1) {
				bucketNr_++;
			}
			else {
				bucketNr_ = 0;
			}
			bucket_[bucketNr_] = 0;
			sum /= N;
			sum /= bucketSize_;
			output = sqrt(sum);
		}
		return output;
	}
};

template <typename S, size_t BUCKETS_PER_RC, size_t RC_PER_WINDOW>
class BucketIntegratedRms
{
	static_assert(BUCKETS_PER_RC >= 1 && BUCKETS_PER_RC <= 16, "Invalid number of buckets per RC");
	static_assert(RC_PER_WINDOW >= 1 && RC_PER_WINDOW <= 16, "Invalid number of RC per window");
	static_assert(is_floating_point<S>::value, "Expected floating-point type parameter");

	static constexpr double INTEGRATOR_WINDOW_SIZE_RATIO = 0.25;
	static constexpr double INTEGRATOR_BUCKET_RATIO = 8;
	static constexpr size_t BUCKET_COUNT = BUCKETS_PER_RC * RC_PER_WINDOW;

	BucketRms<S, BUCKET_COUNT> rms_;
	IntegrationCoefficients<S> coeffs_;
	S int1_, int2_;

public:
	size_t setWindowSize(size_t newSize)
	{
		size_t windowSize = rms_.setWindowSize(newSize);
		coeffs_.setCharacteristicSamples(windowSize / RC_PER_WINDOW);
		return windowSize;
	}

	void zero(S value)
	{
		rms_.zero();
		int1_ = int2_ = 0;
	}

	void setValue(S value)
	{
		rms_.setvalue(value);
		int1_ = int2_ = value;
	}

	S addAndGet(S value)
	{
		return addSquareAndGet(value * value);
	}

	S addSquareAndGet(S square)
	{
		S rms = rms_.addSquareAndGet(square);
		return coeffs_.integrate(coeffs_.integrate(rms, int1_), int2_);
	}

	S addSquareCompareAndGet(S square, S minimumRms)
	{
		S rms = rms_.addSquareAndGet(square);
		return coeffs_.integrate(
				coeffs_.integrate(
						Value<S>::max(minimumRms, rms),
						int1_),
				int2_);
	}
};

template <typename S>
using DefaultRms = BucketIntegratedRms<S, 8, 2>;

} /* End of name space tdap */

#endif /* TDAP_RMS_HEADER_GUARD */
