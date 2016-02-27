/*
 * tdap/Followers.hpp
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

#ifndef TDAP_FOLLOWERS_HEADER_GUARD
#define TDAP_FOLLOWERS_HEADER_GUARD

#include <tdap/Integration.hpp>

namespace tdap {

using namespace std;
/**
 * Follows rises in input and holds those for a number of samples. After that,
 * follow the (lower) input.
 */
template< typename S>
class HoldMax
{
	static_assert(is_arithmetic<S>::value, "Sample type S must be arithmetic");
	size_t holdSamples;
	size_t toHold;
	S holdValue;

public:
	HoldMax(size_t holdForSamples, S initialHoldValue = 0) :
		holdSamples(holdForSamples), toHold(0), holdValue(initialHoldValue) {}

	void resetHold()
	{
		toHold = 0;
	}

	void setHoldCount(size_t newCount)
	{
		holdSamples = newCount;
		if (toHold > holdSamples) {
			toHold = holdSamples;
		}
	}

	S apply(S input)
	{
		if (input > holdValue) {
			toHold = holdSamples;
			holdValue = input;
			return holdValue;
		}
		if (toHold > 0) {
			toHold--;
			return holdValue;
		}
		return input;
	}
};

/**
 * Follows rises in input and holds those for a number of samples. After that,
 * follows the (lower) input in an integrated fashion.
 */
template<typename S, typename C>
class HoldMaxRelease
{
	static_assert(is_arithmetic<S>::value, "Sample type S must be arithmetic");

	size_t holdSamples;
	size_t toHold;
	S holdValue;
	IntegratorFilter<C> integrator_;

public:
	HoldMaxRelease(size_t holdForSamples, C integrationSamples, S initialHoldValue = 0) :
		holdSamples(holdForSamples), toHold(0), holdValue(initialHoldValue), integrator_(integrationSamples) {}

	void resetHold()
	{
		toHold = 0;
	}

	void setHoldCount(size_t newCount)
	{
		holdSamples = newCount;
		if (toHold > holdSamples) {
			toHold = holdSamples;
		}
	}

	S apply(S input)
	{
		if (input > holdValue) {
			toHold = holdSamples;
			holdValue = input;
			integrator_.setOutput(input);
			return holdValue;
		}
		if (toHold > 0) {
			toHold--;
			return holdValue;
		}
		return integrator_.integrate(input);
	}

	IntegratorFilter<C> &integrator()
	{
		return integrator_;
	}
};

template<typename S, typename C>
class HoldMaxIntegrated
{
	static_assert(is_arithmetic<S>::value, "Sample type S must be arithmetic");
	HoldMax<S> holdMax;
	IntegratorFilter<C> integrator_;

public:
	HoldMaxIntegrated(size_t holdForSamples, C integrationSamples, S initialHoldValue = 0) :
		holdMax(holdForSamples, initialHoldValue), integrator_(integrationSamples, initialHoldValue) {}

	void resetHold()
	{
		holdMax.resetHold();
	}

	void setHoldCount(size_t newCount)
	{
		holdMax.setHoldCount(newCount);
	}

	S apply(S input)
	{
		return integrator_.integrate(holdMax.apply(input));
	}

	IntegratorFilter<C> &integrator()
	{
		return integrator_;
	}
};

template<typename S>
class HoldMaxDoubleIntegrated
{
	static_assert(is_arithmetic<S>::value, "Sample type S must be arithmetic");
	HoldMax<S> holdMax;
	IntegrationCoefficients<S> coeffs;
	S i1, i2;

public:
	HoldMaxDoubleIntegrated(size_t holdForSamples, S integrationSamples, S initialHoldValue = 0) :
		holdMax(holdForSamples, initialHoldValue), coeffs(integrationSamples),
		i1(initialHoldValue), i2(initialHoldValue) {}
	HoldMaxDoubleIntegrated() : HoldMaxDoubleIntegrated(15, 10, 1.0) {}

	void resetHold()
	{
		holdMax.resetHold();
	}

	void setMetrics(double integrationSamples, size_t holdCount)
	{
		holdMax.setHoldCount(holdCount);
		coeffs.setCharacteristicSamples(integrationSamples);
	}

	S apply(S input)
	{
		return coeffs.integrate(coeffs.integrate(holdMax.apply(input), i1), i2);
	}

	S setValue(S x)
	{
		i1 = i2 = x;
	}

	S applyWithMinimum(S input, S minimum)
	{
		return coeffs.integrate(coeffs.integrate(holdMax.apply(Values::max(input, minimum)), i1), i2);
	}
};

template<typename C>
class HoldMaxAttackRelease
{
	static_assert(is_arithmetic<C>::value, "Sample type S must be arithmetic");
	HoldMax<C> holdMax;
	AttackReleaseFilter<C> integrator_;

public:
	HoldMaxAttackRelease(size_t holdForSamples, C attackSamples, C releaseSamples, C initialHoldValue = 0) :
		holdMax(holdForSamples, initialHoldValue), integrator_(attackSamples, releaseSamples, initialHoldValue) {}

	void resetHold()
	{
		holdMax.resetHold();
	}

	void setHoldCount(size_t newCount)
	{
		holdMax.setHoldCount(newCount);
	}

	C apply(C input)
	{
		return integrator_.integrate(holdMax.apply(input));
	}

	AttackReleaseFilter<C> &integrator()
	{
		return integrator_;
	}
};

template<typename C>
class SmoothHoldMaxAttackRelease
{
	static_assert(is_arithmetic<C>::value, "Sample type S must be arithmetic");
	HoldMax<C> holdMax;
	AttackReleaseSmoothFilter<C> integrator_;

public:
	SmoothHoldMaxAttackRelease(size_t holdForSamples, C attackSamples, C releaseSamples, C initialHoldValue = 0) :
		holdMax(holdForSamples, initialHoldValue), integrator_(attackSamples, releaseSamples, initialHoldValue) {}

	void resetHold()
	{
		holdMax.resetHold();
	}

	void setHoldCount(size_t newCount)
	{
		holdMax.setHoldCount(newCount);
	}

	C apply(C input)
	{
		return integrator_.integrate(holdMax.apply(input));
	}

	void setValue(C x)
	{
		integrator_.setOutput(x);
	}

	AttackReleaseSmoothFilter<C> &integrator()
	{
		return integrator_;
	}
};

} /* End of name space tdap */

#endif /* TDAP_FOLLOWERS_HEADER_GUARD */
