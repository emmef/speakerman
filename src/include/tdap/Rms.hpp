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

	S addSquareAndGetSquare(S square)
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
			output = sum;
		}
		return output;
	}
};

template <typename S, size_t BUCKET_COUNT>
class BucketIntegratedRms
{
	static_assert(BUCKET_COUNT >= 2 && BUCKET_COUNT <= 64, "Invalid number of buckets");
	static_assert(is_floating_point<S>::value, "Expected floating-point type parameter");

	static constexpr double INTEGRATOR_WINDOW_SIZE_RATIO = 0.25;
	static constexpr double INTEGRATOR_BUCKET_RATIO = 8;

	BucketRms<S, BUCKET_COUNT> rms_;
	IntegrationCoefficients<S> coeffs_;
	S int1_, int2_;

public:
	size_t setWindowSizeAndRc(size_t newSize, size_t rcSize)
	{
		size_t windowSize = rms_.setWindowSize(newSize);
		size_t minRc = 2 * windowSize / BUCKET_COUNT;
		coeffs_.setCharacteristicSamples(Values::max(minRc, rcSize));
		return windowSize;
	}

	size_t setWindowSize(size_t newSize)
	{
		size_t windowSize = rms_.setWindowSize(newSize);
		size_t minRc = 2 * windowSize / BUCKET_COUNT;
		coeffs_.setCharacteristicSamples(Values::max(minRc, windowSize / 4));
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

	S addSquareAndGetFastAttack(S square)
	{
		S rms = rms_.addSquareAndGetSquare(square);
		return sqrt(coeffs_.integrate(coeffs_.integrate(rms, int1_), int2_));
	}

	S addSquareAndGetFastAttackWithMinimum(S square, S minimumSqr)
	{
		S rms = Values::max(minimumSqr, rms_.addSquareAndGetSquare(square));
		return sqrt(coeffs_.integrate(coeffs_.integrate(rms, int1_), int2_));
	}

	S addSquareCompareAndGet(S square, S minimumRms)
	{
		S rms = Values::max(minimumRms, rms_.addSquareAndGet(square));
		return coeffs_.integrate(coeffs_.integrate(rms,int1_),int2_);
	}
};

	template <typename S, size_t BUCKETS, size_t LEVELS>
	class MultiBucketMean
	{
	public:
		static constexpr size_t MINIMUM_BUCKETS = 4;
		static constexpr size_t MAXIMUM_BUCKETS = 64;
		static_assert(is_arithmetic<S>::value, "Expected floating-point type parameter");
		static_assert(Power2::constant::is(BUCKETS), "Bucket count must be valid power of two");
		static_assert(Values::is_between(BUCKETS, MINIMUM_BUCKETS, 64), "Bucket count must be between 4 and 256");
		static_assert(Values::is_between(LEVELS, 1, 16), "Levels must be between 1 and 16");

		static constexpr size_t BUCKET_MASK = BUCKETS - 1;
		static constexpr double BUCKET_WEIGHT = 1.0 / BUCKETS;

	private:
		struct BucketEntry
		{
			size_t current_;
			S bucket_[BUCKETS];
			BucketEntry *next_;
			S mean_;

			S addBucketValue(S value)
			{
				bucket_[current_] = value;
				if (next && (current_ & 1)) {
					S sum = value ;
					next_->addBucketValue(value + bucket_[current_ - 1]);
				}
				current_++;
				current_ &= BUCKET_MASK;
				S sum = 0;
				for (size_t i = 0; i < BUCKETS; i++) {
					sum += bucket_[i];
				}
				mean_ = sum * BUCKET_WEIGHT;
				return mean_;
			}

			void zero()
			{
				setValue(static_cast<S>(0));
			}

			void setValue(S value)
			{
				for (size_t i = 0; i < BUCKETS; i++) {
					bucket_[i] = value;
				}
				current_ = 0;
				mean_ = value;
			}

			void operator = (const BucketEntry &source)
			{
				current_ = source.current_;
				mean_ = source.mean_;
				for (size_t i = 0; i < BUCKETS; i++) {
					bucket_[i] = source.bucket_[i];
				}
			}
		};

		BucketEntry entry_[LEVELS];

		void init()
		{
			zero();
			size_t i;
			for (i = 0; i < LEVELS - 1; i++) {
				entry_[i].next_ = &entry_[i + 1];
			}
			entry_[i].next_ = nullptr;
		}

	public:
		MultiBucketMean() { init(); }
		void zero()
		{
			setValue(static_cast<S>(0));
		}

		void setValue(S value)
		{
			for (size_t i= 0; i < LEVELS; i++) {
				entry_[i].setValue(value);
			}
		}

		void addBucketValue(S value)
		{
			entry_[0].addBucketValue(value);
		}

		FixedSizeArray<S, LEVELS> getMeans()
		{
			FixedSizeArray<S, LEVELS> means;
			for (size_t i= 0; i < LEVELS; i++) {
				means[i] = entry_[i].mean_;
			}
		};

		S getMean(size_t level)
		{
			entry_[IndexPolicy::array(level, LEVELS)].mean_;
		}

		S getBucket(size_t level, size_t bucket)
		{
			entry_[IndexPolicy::array(level, LEVELS)].bucket_[IndexPolicy::array(bucket, BUCKETS)];
		}

		void operator = (const MultiBucketMean &source)
		{
			for (size_t i= 0; i < LEVELS; i++) {
				entry_[i] = source.entry_[i];
			}
		}
	};

	template<typename S, size_t BUCKETS, size_t LEVELS>
	class MultiRcRms
	{
	public:
		enum class Modus { INTEGRATE_THEN_ROOT, ROOT_THEN_INTEGRATE };
	private:
		MultiBucketMean<S, BUCKETS, LEVELS> mean_;
		Modus modus[LEVELS];
		S scale_[LEVELS];
		S threshold_[LEVELS];
		S int1_[LEVELS], int2_[LEVELS];
		S value_[LEVELS];
		size_t samplesPerBucket, sample_;
		S sum;
		IntegrationCoefficients<S> coeffs_[LEVELS];

		void init()
		{
			for (size_t i = 0; i < LEVELS; i++) {
				scale_[i] = 1;
				threshold_[i] = 0;
			}
			zero();
			samplesPerBucket = 1;
		}
	public:
		MultiRcRms()
		{
			for (size_t i = 0; i < LEVELS; i++) {
				scale_[i] = 1;
				threshold_[i] = 0;
			}
			samplesPerBucket = 1;
		}

		size_t setSmallWindow(size_t newSize)
		{
			size_t proposal1 = Values::force_between(newSize, BUCKETS, (const size_t) (1e6 * BUCKETS));
			samplesPerBucket = newSize / BUCKETS;
			size_t integrationSamples = MultiBucketMean<S, BUCKETS, LEVELS>::MINIMUM_BUCKETS * samplesPerBucket;
			for (size_t i = 0; i < LEVELS; i++) {
				coeffs_[i].setCharacteristicSamples(integrationSamples);
				integrationSamples *= 2;
			}
			return samplesPerBucket * BUCKETS;
		}

		size_t setSmallWindowAndRc(size_t newSize, size_t proposedIntegrationSamples)
		{
			size_t proposal1 = Values::force_between(newSize, BUCKETS, (const size_t) (1e6 * BUCKETS));
			samplesPerBucket = newSize / BUCKETS;
			size_t minimumIntegrationSamples = MultiBucketMean<S, BUCKETS, LEVELS>::MINIMUM_BUCKETS * samplesPerBucket;
			size_t integrationSamples = Values::max(proposedIntegrationSamples, minimumIntegrationSamples);
			for (size_t i = 0; i < LEVELS; i++) {
				coeffs_[i].setCharacteristicSamples(integrationSamples);
				integrationSamples *= 2;
			}
			return samplesPerBucket * BUCKETS;
		}

		void configure(size_t level, S scale, S threshold, Modus modus)
		{
			S sc = Values::valid_between(scale, 1e-3, 1e6);
			scale[IndexPolicy::array(level, LEVELS)] = sc;
			if (modus == Modus::INTEGRATE_THEN_ROOT) {
				threshold[level] = 1.0 / sc;
			}
			else {
				threshold[level] = 1.0 / (sc * sc);
			}
		}

		void addSquare(S square)
		{
			sum += square;
			sample_++;
			if (sample_ == samplesPerBucket) {
				sample_ = 0;
				mean_.addBucketValue(sum / samplesPerBucket);
			}
			for (size_t i = 0; i < LEVELS; i++) {
				S mean = mean_.getMean(i);
				if (modus[i] == Modus::INTEGRATE_THEN_ROOT) {
					S max = Values::max(mean, threshold_[i]);
					value_[i] = sqrt(coeffs_[i].integrate(coeffs_[i].integrate(max, int1_[i]), int2_[i]));
				}
				else {
					S max = Values::max(sqrt(mean), threshold_[i]);
					value_[i] = coeffs_[i].integrate(coeffs_[i].integrate(max, int1_[i]), int2_[i]);
				}
			}
		}

		S getValue(size_t level)
		{
			return value_[IndexPolicy::array(level, LEVELS)];
		}

		FixedSizeArray<S, LEVELS> getValues()
		{
			FixedSizeArray<S, LEVELS> result;
			for (size_t i = 0; i < LEVELS; i++) {
				result[i] = value_[i];
			}
			return result;
		};

		void zero()
		{
			setValue(static_cast<S>(0));
		}

		void setValue(S value)
		{
			S square = value * value;
			mean_.setValue(square);
			for (size_t i = 0; i < LEVELS; i++) {
				int1_[i] = int2_[i] = square;
				value_[i] = value;
			}
			sample_ = 0;
			sum = samplesPerBucket * square;
		}


	};

template <typename S>
using DefaultRms = BucketIntegratedRms<S, 16>;

} /* End of name space tdap */

#endif /* TDAP_RMS_HEADER_GUARD */
