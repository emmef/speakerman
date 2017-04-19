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

#include <tdap/FixedSizeArray.hpp>
#include <tdap/Integration.hpp>
#include <tdap/Power2.hpp>

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
		static_assert(Values::is_between(BUCKETS, MINIMUM_BUCKETS, MAXIMUM_BUCKETS), "Bucket count must be between 4 and 256");
		static_assert(Values::is_between(LEVELS, (size_t)1, (size_t)16), "Levels must be between 1 and 16");

		static constexpr size_t BUCKET_MASK = BUCKETS - 1;

	private:
		struct BucketEntry
		{
			size_t current_;
			S bucket_[BUCKETS];
			BucketEntry *next_;
			S mean_;
			S weight_;

			void addBucketValue(S value)
			{
				bucket_[current_] = value;
				if (next_ && (current_ & 1)) {
					S sum = value ;
					next_->addBucketValue(value + bucket_[current_ - 1]);
				}
				current_++;
				current_ &= BUCKET_MASK;
				S sum = 0;
				for (size_t i = 0; i < BUCKETS; i++) {
					sum += bucket_[i];
				}
				mean_ = sum * weight_;
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
			size_t effectiveBuckets = 1;
			for (i = 0; i < LEVELS - 1; i++, effectiveBuckets *= 2) {
				entry_[i].next_ = &entry_[i + 1];
				entry_[i].weight_ = 1.0 / (BUCKETS * effectiveBuckets);
			}
			entry_[i].next_ = nullptr;
			entry_[i].weight_ = 1.0 / (BUCKETS * effectiveBuckets);
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

		inline void addBucketValue(S value)
		{
			entry_[0].addBucketValue(value);
		}

		FixedSizeArray<S, LEVELS> getMeans() const
		{
			FixedSizeArray<S, LEVELS> means;
			for (size_t level= 0; level < LEVELS; level++) {
				means[level] = entry_[level].mean_;
			}
			return means;
		};

		FixedSizeArray<FixedSizeArray<S, BUCKETS>, LEVELS> getBuckets() const
		{
			FixedSizeArray<FixedSizeArray<S, BUCKETS>, LEVELS> buckets;
			for (size_t level = 0; level < LEVELS; level++) {
				for (size_t bucket = 0; bucket < BUCKETS; bucket++) {
					buckets[level][bucket] = entry_[level].bucket_[bucket];
				}
			}
			return buckets;
		};

		inline S getMean(size_t level) const
		{
			return entry_[IndexPolicy::array(level, LEVELS)].mean_;
		}

		inline S getBucket(size_t level, size_t bucket) const
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
		MultiBucketMean<S, BUCKETS, LEVELS> mean_;
		S scale_[LEVELS];
		S int1_[LEVELS], int2_[LEVELS];
		size_t samplesPerBucket, sample_;
		S sum;
		IntegrationCoefficients<S> coeffs_[LEVELS];
		size_t true_levels_ = LEVELS / 2;

		void init()
		{
			for (size_t i = 0; i < LEVELS; i++) {
				scale_[i] = 1;
			}
			zero();
			samplesPerBucket = 1;
		}

		void addSquare(S square)
		{
			sum += square;
			sample_++;
			if (sample_ == samplesPerBucket) {
				sample_ = 0;
				mean_.addBucketValue(sum / samplesPerBucket);
				sum = 0;
			}

		}
	public:
		MultiRcRms()
		{
			for (size_t i = 0; i < LEVELS; i++) {
				scale_[i] = 1;
			}
			samplesPerBucket = 1;
			sample_ = 0;
			sum = 0;
			setIntegrators(0);
		}

		size_t setSmallWindow(size_t newSize)
		{
			size_t proposal1 = Values::force_between(newSize, BUCKETS, (const size_t) (1e6 * BUCKETS));
			samplesPerBucket = newSize / BUCKETS;
			size_t integrationSamples = samplesPerBucket *  BUCKETS / 4;
			for (size_t i = 0; i < LEVELS; i++) {
				coeffs_[i].setCharacteristicSamples(integrationSamples);
				integrationSamples *= 2;
			}
			return samplesPerBucket * BUCKETS;
		}

		size_t setSmallWindowAndRc(size_t newSize, double smallRcIntegrationBuckets, double largeRcIntegrationBuckets)
		{
			size_t proposal1 = Values::force_between(newSize, BUCKETS, (const size_t) (1e6 * BUCKETS));
			samplesPerBucket = newSize / BUCKETS;
			size_t minIntSamples = Value<double>::force_between(smallRcIntegrationBuckets, 2, BUCKETS) * samplesPerBucket;
			size_t maxIntSamples = Value<double>::force_between(largeRcIntegrationBuckets, 2, BUCKETS) * samplesPerBucket * (1L << (LEVELS - 1));
			double deltaBase = log(maxIntSamples) - log(minIntSamples);
			for (size_t i = 0; i < LEVELS; i++) {
				coeffs_[i].setCharacteristicSamples(minIntSamples * exp(deltaBase * i / (LEVELS - 1)));
			}
			return samplesPerBucket * BUCKETS;
		}

		/**
		 * Configure to which level (starting from smallest window size)
		 * a true RMS is done (integration of root of mean of squares) and
		 * after which a fast-attach RMS is done (root of integrated of
		 * mean of squares)
		 */
		void configure_true_levels(size_t new_true_levels)
		{
			if (new_true_levels > LEVELS) {
				throw std::out_of_range("true_levels_ out of range");
			}
			true_levels_ = new_true_levels;
		}

		S set_scale(size_t level, S scale)
		{
			S sc = Values::force_between(scale, 1e-3, 1e6);
			scale_[IndexPolicy::array(level, LEVELS)] = sc * sc;
			return sc;
		}

		S get_scale(size_t level) const
		{
			return sqrt(scale_[level]);
		}

		S addSquareGetValue(S square, S threshold)
		{
			addSquare(square);
			S value = threshold;
			ssize_t level;
			for (level = LEVELS - 1; level >= 0; level--) {
				S scaledSquaredMean = scale_[level] * mean_.getMean(level);
				S squaredMax = Values::max(value * value, scaledSquaredMean);
				S integratedSquaredMax = coeffs_[level].integrate(squaredMax, int1_[level]);
				integratedSquaredMax = coeffs_[level].integrate(integratedSquaredMax, int2_[level]);
				value = sqrt(integratedSquaredMax);
			}
//			for (; level >= 0; level--) {
//				S scaledMean = sqrt(scale_[level] * mean_.getMean(level));
//				S max = Values::max(value, scaledMean);
//				S integratedMax = coeffs_[level].integrate(max, int1_[level]);
//				value = coeffs_[level].integrate(max, int2_[level]);
//			}
			return value;
		}

		S addSquareGetValue(S square, S threshold, S &rawDetection)
		{
			addSquare(square);
			S value = threshold;
			rawDetection = 0;
			ssize_t level;
			for (level = LEVELS - 1; level >= 0; level--) {
				S scaledSquaredMean = scale_[level] * mean_.getMean(level);
				rawDetection = Values::max(rawDetection, scaledSquaredMean);
				S squaredMax = Values::max(value * value, scaledSquaredMean);
				S integratedSquaredMax = coeffs_[level].integrate(squaredMax, int1_[level]);
				integratedSquaredMax = coeffs_[level].integrate(integratedSquaredMax, int2_[level]);
				value = sqrt(integratedSquaredMax);
			}
//			for (; level >= 0; level--) {
//				double rawScaled = scale_[level] * mean_.getMean(level);
//				rawDetection = Values::max(rawDetection, rawScaled);
//				S scaledMean = sqrt(rawScaled);
//				S max = Values::max(value, scaledMean);
//				S integratedMax = coeffs_[level].integrate(max, int1_[level]);
//				value = coeffs_[level].integrate(max, int2_[level]);
//			}
			rawDetection = sqrt(rawDetection);
			return value;
		}

		void zero()
		{
			setValue(static_cast<S>(0));
		}

		void setValue(S value)
		{
			S square = value * value;
			mean_.setValue(square);

			setIntegrators(value);
			sample_ = 0;
			sum = samplesPerBucket * square;
		}

		void setIntegrators(S value)
		{
			size_t level;
			for (level = 0; level < true_levels_; level++) {
				int1_[level] = int2_[level] = value;
			}
			S square = value * value;
			for (; level < LEVELS; level++) {
				int1_[level] = int2_[level] = square;
			}
		}
	};

template <typename S>
using DefaultRms = BucketIntegratedRms<S, 16>;

} /* End of name space tdap */

#endif /* TDAP_RMS_HEADER_GUARD */
