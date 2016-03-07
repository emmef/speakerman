/*
 * tdap/IirCoefficients.hpp
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

#ifndef TDAP_IIRCOEFFICIENTS_HEADER_GUARD
#define TDAP_IIRCOEFFICIENTS_HEADER_GUARD

#include <type_traits>
#include <cstddef>

#include <tdap/Count.hpp>
#include <tdap/Denormal.hpp>
#include <tdap/Value.hpp>
#include <tdap/Filters.hpp>

namespace tdap {

template <typename COEFFICIENT, size_t ORDER, bool flushToZero = false>
inline static COEFFICIENT iir_filter_fixed(
		const COEFFICIENT * const c,  // (ORDER + 1) C-coefficients
		const COEFFICIENT * const d,  // (ORDER + 1) D-coefficients
		COEFFICIENT * const xHistory, // (ORDER) x value history
		COEFFICIENT * const yHistory, // (ORDER) y value history
		const COEFFICIENT input)      // input sample value
{
	static_assert(std::is_floating_point<COEFFICIENT>::value, "Coefficient type should be floating-point");
	static_assert(ORDER > 0, "ORDER of filter must be positive");

	COEFFICIENT Y = 0;
	COEFFICIENT X = input; // input is xN0
	COEFFICIENT yN0 = 0.0;
	size_t i, j;
	for (i = 0, j = 1; i < ORDER; i++, j++) {
		const COEFFICIENT xN1 = xHistory[i];
		const COEFFICIENT yN1 = yHistory[i];
		xHistory[i] = X;
		X = xN1;
		yHistory[i] = Y;
		Y = yN1;
		yN0 += c[j] * xN1 + d[j] * yN1;
	}
	yN0 += c[0] * input;

	if (flushToZero) {
		Denormal::flush(yN0);
	}
	yHistory[0] = yN0;

	return yN0;
}

template <typename COEFFICIENT, bool flushToZero = false>
inline static COEFFICIENT iir_filter(
		int order,
		const COEFFICIENT * const c,  // (order + 1) C-coefficients
		const COEFFICIENT * const d,  // (order + 1) D-coefficients
		COEFFICIENT * const xHistory, // (order) x value history
		COEFFICIENT * const yHistory, // (order) y value history
		const COEFFICIENT input)      // input sample value
{
	static_assert(std::is_floating_point<COEFFICIENT>::value, "Coefficient type should be floating-point");

	COEFFICIENT Y = 0;
	COEFFICIENT X = input; // input is xN0
	COEFFICIENT yN0 = 0.0;
	int i, j;
	for (i = 0, j = 1; i < order; i++, j++) {
		const COEFFICIENT xN1 = xHistory[i];
		const COEFFICIENT yN1 = yHistory[i];
		xHistory[i] = X;
		X = xN1;
		yHistory[i] = Y;
		Y = yN1;
		yN0 += c[j] * xN1 + d[j] * yN1;
	}
	yN0 += c[0] * input;

	if (flushToZero) {
		Denormal::flush(yN0);
	}
	yHistory[0] = yN0;

	return yN0;
}

template <typename COEFFICIENT>
class IirCoefficients
{
	static_assert(std::is_floating_point<COEFFICIENT>::value, "Type T must be a floating-point type");
public:
	static constexpr size_t coefficientsForOrder(size_t order) { return order + 1; }
	static constexpr size_t totalCoefficientsForOrder(size_t order) { return 2 * coefficientsForOrder(order); }
	static constexpr size_t historyForOrder(size_t order) { return order; }
	static constexpr size_t totalHistoryForOrder(size_t order) { return 2 * historyForOrder(order); }

	virtual size_t order() const = 0;
	virtual size_t maxOrder() const = 0;
	virtual bool hasFixedOrder() const = 0;
	virtual void setOrder(size_t newOrder) = 0;
	virtual void setC(size_t idx, const COEFFICIENT coefficient) = 0;
	virtual void setD(size_t idx, const COEFFICIENT coefficient) = 0;
	virtual COEFFICIENT getC(size_t idx) const = 0;
	virtual COEFFICIENT getD(size_t idx) const = 0;
	virtual COEFFICIENT filter(COEFFICIENT * const xHistory, COEFFICIENT * const yHistory, const COEFFICIENT input) const = 0;

	size_t coefficientCount() const { return coefficientsForOrder(order()); }
	size_t totalCoefficientsCount() const { return totalCoefficientsForOrder(order()); }
	size_t historyCount() const { return historyForOrder(order()); }
	size_t totalHistoryCount() const { return totalHistoryForOrder(order()); }

	virtual ~IirCoefficients() = default;
};

template <typename COEFFICIENT, typename COEFFICIENT_CLASS>
class WrappedIirCoefficients;

template <typename COEFFICIENT, size_t ORDER>
class FixedSizeIirCoefficients
{
	static_assert(std::is_floating_point<COEFFICIENT>::value, "Coefficient type should be floating-point");
public:
	static constexpr size_t COEFFS = IirCoefficients<COEFFICIENT>::coefficientsForOrder(ORDER);
	static constexpr size_t TOTAL_COEEFS = IirCoefficients<COEFFICIENT>::totalCoefficientsForOrder(ORDER);
	static constexpr size_t C_OFFSET = 0;
	static constexpr size_t D_OFFSET = COEFFS;
	static constexpr size_t HISTORY = IirCoefficients<COEFFICIENT>::historyForOrder(ORDER);
	static constexpr size_t TOTAL_HISTORY = IirCoefficients<COEFFICIENT>::totalHistoryForOrder(ORDER);
private:
	constexpr size_t getCOffset(size_t idx) const { return Value<size_t>::valid_below(idx, COEFFS) + C_OFFSET; }
	constexpr size_t getDOffset(size_t idx) const { return Value<size_t>::valid_below(idx, COEFFS) + D_OFFSET; }

	COEFFICIENT &C(size_t idx) { return data[getCOffset(idx)]; }
	COEFFICIENT &D(size_t idx) { return data[getDOffset(idx)]; }
	const COEFFICIENT &C(size_t idx) const { return data[getCOffset(idx)]; }
	const COEFFICIENT &D(size_t idx) const { return data[getDOffset(idx)]; }

	const COEFFICIENT * const unsafeC() const { return data + C_OFFSET; }
	const COEFFICIENT * const unsafeD() const { return data + D_OFFSET; }


public:
	size_t order() const { return ORDER; }
	size_t maxOrder() const { return ORDER; }
	bool hasFixedOrder() const { return true; }
	void setOrder(size_t newOrder) { };

	void setC(size_t idx, const COEFFICIENT coefficient) { C(idx) = coefficient; }
	void setD(size_t idx, const COEFFICIENT coefficient) { D(idx) = coefficient; }
	COEFFICIENT getC(size_t idx) const { return C(idx); }
	COEFFICIENT getD(size_t idx) const { return D(idx); }

	void setTransparent()
	{
		for (size_t i = 0; i < TOTAL_COEEFS; i++) {
			data[i] = 0.0;
		}
		C(0) = 1;
	}

	template<typename T>
	void assign(const IirCoefficients<T> &source)
	{
		if (source.order() == ORDER) {
			for (size_t i = 0; i < COEFFS; i++) {
				C(i) = source.getC(i);
				D(i) = source.getD(i);
			}
			return;
		}
		throw std::invalid_argument("Value not below threshold");
	}

	template<bool flushToZero>
	COEFFICIENT do_filter(
			COEFFICIENT * const xHistory, // (ORDER) x value history
			COEFFICIENT * const yHistory, // (ORDER) y value history
			COEFFICIENT input) const
	{
		return iir_filter_fixed<COEFFICIENT, ORDER, flushToZero>(
				unsafeC(),
				unsafeD(),
				xHistory,
				yHistory,
				input);
	}

	COEFFICIENT filter(COEFFICIENT * const xHistory, COEFFICIENT * const yHistory, const COEFFICIENT input) const
	{
		return do_filter<false>(xHistory, yHistory, input);
	}

	WrappedIirCoefficients<COEFFICIENT, FixedSizeIirCoefficients<COEFFICIENT, ORDER>> wrap()
	{
		return WrappedIirCoefficients<COEFFICIENT, FixedSizeIirCoefficients<COEFFICIENT, ORDER>>(*this);
	}

private:
	COEFFICIENT data[TOTAL_COEEFS];
};


template <typename COEFFICIENT>
class VariableSizedIirCoefficients
{
	const size_t maxOrder_;
	size_t order_;
	COEFFICIENT * data_;

	const size_t getCBaseOffset() const { return 0; }
	const size_t getDBaseOffset() const { return maxOrder_ + 1; }

	const size_t getCOffset(size_t i) const { return getCBaseOffset() + Value<size_t>::valid_below_or_same(i, order_); }
	const size_t getDOffset(size_t i) const { return getDBaseOffset() + Value<size_t>::valid_below_or_same(i, order_); }

	COEFFICIENT &C(size_t i) { return data_[getCOffset(i)]; }
	COEFFICIENT &D(size_t i) { return data_[getDOffset(i)]; }
	const COEFFICIENT &C(size_t i) const { return data_[getCOffset(i)]; }
	const COEFFICIENT &D(size_t i) const { return data_[getDOffset(i)]; }

	const COEFFICIENT * const unsafeC() const { return data_ + getCBaseOffset(); }
	const COEFFICIENT * const unsafeD() const { return data_ + getDBaseOffset(); }

public:
	VariableSizedIirCoefficients(size_t maxOrder) :
		maxOrder_(Value<size_t>::valid_between(maxOrder,1,64)),
		order_(maxOrder_),
		data_(new COEFFICIENT[IirCoefficients<COEFFICIENT>::totalCoefficientsForOrder(maxOrder_)]) { }

	VariableSizedIirCoefficients(size_t maxOrder, size_t order) :
		maxOrder_(Value<size_t>::valid_between(maxOrder,1,64)),
		order_(Value<size_t>::valid_between(order, 1, maxOrder_)),
		data_(new COEFFICIENT[IirCoefficients<COEFFICIENT>::totalCoefficientsForOrder(maxOrder_)]) { }

	size_t order() const override { return order_; }
	size_t maxOrder() const override { return maxOrder_; }
	bool hasFixedOrder() { return false; }
	void setOrder(size_t order) { order_ = Value<size_t>::valid_between(order, 1, maxOrder_); }

	void setC(size_t idx, const COEFFICIENT coefficient) override { C(idx) = coefficient; }
	void setD(size_t idx, const COEFFICIENT coefficient) override { D(idx) = coefficient; }
	COEFFICIENT getC(size_t idx) const override { return C(idx); }
	COEFFICIENT getD(size_t idx) const override { return D(idx); }

	template<typename T>
	void assign(const IirCoefficients<T> &source)
	{
		setOrder(source.order());
		for (size_t i = 0; i <= order_; i++) {
			C(i) = source.getC(i);
			D(i) = source.getD(i);
		}
	}

	template<bool flushToZero = false>
	COEFFICIENT do_filter(
			COEFFICIENT * const xHistory, // (ORDER) x value history
			COEFFICIENT * const yHistory, // (ORDER) y value history
			COEFFICIENT input) const
	{
		return iir_filter<COEFFICIENT, flushToZero>(
				order_,
				unsafeC(),
				unsafeD(),
				xHistory,
				yHistory,
				input);
	}

	COEFFICIENT filter(COEFFICIENT * const xHistory, COEFFICIENT * const yHistory, const COEFFICIENT input) const
	{
		return do_filter<false>(xHistory, yHistory, input);
	}

	WrappedIirCoefficients<COEFFICIENT, VariableSizedIirCoefficients<COEFFICIENT>> wrap()
	{
		return WrappedIirCoefficients<COEFFICIENT, VariableSizedIirCoefficients<COEFFICIENT>>(*this);
	}

	~VariableSizedIirCoefficients()
	{
		delete[] data_;
	}
};


template <typename COEFFICIENT, typename COEFFICIENT_CLASS>
class WrappedIirCoefficients : public IirCoefficients<COEFFICIENT>
{
	COEFFICIENT_CLASS &coefficients_;

public:
	WrappedIirCoefficients(COEFFICIENT_CLASS &wrapped) : coefficients_(wrapped) {}

	virtual size_t order() const override { return coefficients_.order(); }
	virtual size_t maxOrder() const override { return coefficients_.maxOrder(); }
	virtual bool hasFixedOrder() const override { return coefficients_.hasFixedOrder(); }
	virtual void setOrder(size_t newOrder) override { coefficients_.setOrder(newOrder); }

	virtual void setC(size_t idx, const COEFFICIENT coefficient) override { coefficients_.setC(idx, coefficient); }
	virtual void setD(size_t idx, const COEFFICIENT coefficient) override { coefficients_.setD(idx, coefficient); }
	virtual COEFFICIENT getC(size_t idx) const override { return coefficients_.getC(idx); }
	virtual COEFFICIENT getD(size_t idx) const override { return coefficients_.getD(idx); }

	template<typename T>
	void assign(const IirCoefficients<T> &source)
	{
		coefficients_.assign(source);
	}

	template<bool flushToZero = false>
	COEFFICIENT do_filter(
			COEFFICIENT * const xHistory, // (ORDER) x value history
			COEFFICIENT * const yHistory, // (ORDER) y value history
			COEFFICIENT input) const
	{
		coefficients_.do_filter<flushToZero>(xHistory, yHistory, input);
	}

	virtual COEFFICIENT filter(COEFFICIENT * const xHistory, COEFFICIENT * const yHistory, const COEFFICIENT input) const
	{
		return coefficients_.filter(xHistory, yHistory, input);
	}

	virtual ~WrappedIirCoefficients() = default;
};

template<typename COEFFICIENT, size_t CHANNELS, size_t ORDER>
struct FixedSizeIirCoefficientFilter
{
	using Coefficients = FixedSizeIirCoefficients<COEFFICIENT, ORDER>;
	static constexpr size_t historySize() { return IirCoefficients<COEFFICIENT>::historyForOrder(ORDER); }

	struct History
	{
		COEFFICIENT x[historySize()];
		COEFFICIENT y[historySize()];
	};

	struct SingleChannelFilter: public tdap::Filter<COEFFICIENT>
	{
		FixedSizeIirCoefficientFilter<COEFFICIENT, CHANNELS, ORDER> &wrapped_;

		virtual void reset() { wrapped_.reset(); }
		virtual COEFFICIENT filter(COEFFICIENT input)
		{
			return wrapped_.filter(0, input);
		}

		SingleChannelFilter(FixedSizeIirCoefficientFilter<COEFFICIENT, CHANNELS, ORDER> &wrapped) :
			wrapped_(wrapped) {}
	};

	struct MultiChannelFilter: public tdap::MultiFilter<COEFFICIENT>
	{
		FixedSizeIirCoefficientFilter<COEFFICIENT, CHANNELS, ORDER> &wrapped_;

		virtual size_t channels() const override { return CHANNELS; }
		virtual void reset() override { wrapped_.reset(); }
		virtual COEFFICIENT filter(size_t idx, COEFFICIENT input) override
		{
			return wrapped_.filter(idx, input);
		}

		MultiChannelFilter(FixedSizeIirCoefficientFilter &wrapped) :
			wrapped_(wrapped) {}
	};

	Coefficients coefficients_;
	History history[CHANNELS];

	FixedSizeIirCoefficientFilter() = default;
	FixedSizeIirCoefficientFilter(const Coefficients &coefficients) : coefficients_(coefficients) {}

	void reset()
	{
		for (size_t channel = 0; channel < CHANNELS; channel++) {
			for (size_t t = 0; t < historySize(); t++) {
				history[channel].x[t] = 0;
				history[channel].y[t] = 0;
			}
		}
	}

	template <bool flushToZero>
	COEFFICIENT do_filter(size_t channel, COEFFICIENT input)
	{
		IndexPolicy::array(channel, CHANNELS);
		return coefficients_.do_filter<flushToZero>(history[channel].x, history[channel].y, input);
	}

	COEFFICIENT filter(size_t channel, COEFFICIENT input)
	{
		return do_filter<false>(channel, input);
	}

	template<size_t N, typename ...A>
	void filterArray(const FixedSizeArrayTraits<COEFFICIENT, N, A...> &input, FixedSizeArrayTraits<COEFFICIENT, N, A...> &output)
	{
		for (size_t channel = 0; channel < Value<size_t>::min(CHANNELS, N); channel++) {
			output[channel] = filter(channel, input[channel]);
		}
	}

	template<typename ...A>
	void filterArray(const ArrayTraits<COEFFICIENT, A...> &input, ArrayTraits<COEFFICIENT, A...> &output)
	{
		for (size_t channel = 0; channel < Value<size_t>::min(CHANNELS, input.size(), output.size()); channel++) {
			output[channel] = filter(channel, input[channel]);
		}
	}

	SingleChannelFilter wrapSingle()
	{
		return SingleChannelFilter(*this);
	}

	MultiChannelFilter wrapMulti()
	{
		return MultiChannelFilter(*this);
	}

	Filter<COEFFICIENT> * createFilter()
	{
		return new SingleChannelFilter(*this);
	}

	MultiFilter<COEFFICIENT> * createMultiFilter()
	{
		return new MultiChannelFilter(*this);
	}
};



} /* End of name space tdap */

#endif /* TDAP_IIRCOEFFICIENTS_HEADER_GUARD */
