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

    using namespace std;

    template<typename C, typename S, size_t ORDER, bool FLUSH = false>
    inline static S iir_filter_fixed(
            const C *const c,  // (ORDER + 1) C-coefficients
            const C *const d,  // (ORDER + 1) D-coefficients
            S *const xHistory, // (ORDER) x value history
            S *const yHistory, // (ORDER) y value history
            const S input)      // input sample value
    {
        static_assert(is_floating_point<C>::value, "Coefficient type should be floating-point");
        static_assert(is_arithmetic<S>::value, "Sample type should be arithmetic");
        static_assert(ORDER > 0, "ORDER of filter must be positive");

        C Y = 0;
        C X = input; // input is xN0
        C yN0 = 0.0;
        size_t i, j;
        for (i = 0, j = 1; i < ORDER; i++, j++) {
            const C xN1 = xHistory[i];
            const C yN1 = yHistory[i];
            xHistory[i] = X;
            X = xN1;
            yHistory[i] = Y;
            Y = yN1;
            yN0 += c[j] * xN1 + d[j] * yN1;
        }
        yN0 += c[0] * input;

        if (FLUSH) {
            Denormal::flush(yN0);
        }
        yHistory[0] = yN0;

        return yN0;
    }

    template<typename C, typename S, bool FLUSH = false>
    inline static S iir_filter(
            int order,
            const C *const c,  // (order + 1) C-coefficients
            const C *const d,  // (order + 1) D-coefficients
            S *const xHistory, // (order) x value history
            S *const yHistory, // (order) y value history
            const S input)      // input sample value
    {
        static_assert(is_floating_point<C>::value, "Coefficient type should be floating-point");
        static_assert(is_arithmetic<S>::value, "Sample type should be arithmetic");

        C Y = 0;
        C X = input; // input is xN0
        C yN0 = 0.0;
        int i, j;
        for (i = 0, j = 1; i < order; i++, j++) {
            const C xN1 = xHistory[i];
            const C yN1 = yHistory[i];
            xHistory[i] = X;
            X = xN1;
            yHistory[i] = Y;
            Y = yN1;
            yN0 += c[j] * xN1 + d[j] * yN1;
        }
        yN0 += c[0] * input;

        if (FLUSH) {
            Denormal::flush(yN0);
        }
        yHistory[0] = yN0;

        return yN0;
    }

    struct IirCoefficients
    {
        static constexpr size_t coefficientsForOrder(size_t order)
        { return order + 1; }

        static constexpr size_t totalCoefficientsForOrder(size_t order)
        { return 2 * coefficientsForOrder(order); }

        static constexpr size_t historyForOrder(size_t order)
        { return order; }

        static constexpr size_t totalHistoryForOrder(size_t order)
        { return 2 * historyForOrder(order); }

        virtual size_t order() const = 0;

        virtual size_t maxOrder() const = 0;

        virtual bool hasFixedOrder() const = 0;

        virtual void setOrder(size_t newOrder) = 0;

        virtual void setC(size_t idx, const double coefficient) = 0;

        virtual void setD(size_t idx, const double coefficient) = 0;

        virtual double getC(size_t idx) const = 0;

        virtual double getD(size_t idx) const = 0;

        size_t coefficientCount() const
        { return coefficientsForOrder(order()); }

        size_t totalCoefficientsCount() const
        { return totalCoefficientsForOrder(order()); }

        size_t historyCount() const
        { return historyForOrder(order()); }

        size_t totalHistoryCount() const
        { return totalHistoryForOrder(order()); }

        virtual ~IirCoefficients() = default;
    };

    template<typename C, typename COEFFICIENT_CLASS>
    class WrappedIirCoefficients;


    template<typename C>
    class VariableSizedIirCoefficients;

    template<typename C, size_t ORDER>
    class FixedSizeIirCoefficients
    {
        static_assert(is_floating_point<C>::value, "Coefficient type should be floating-point");
    public:
        static constexpr size_t COEFFS = IirCoefficients::coefficientsForOrder(ORDER);
        static constexpr size_t TOTAL_COEEFS = IirCoefficients::totalCoefficientsForOrder(ORDER);
        static constexpr size_t C_OFFSET = 0;
        static constexpr size_t D_OFFSET = COEFFS;
        static constexpr size_t HISTORY = IirCoefficients::historyForOrder(ORDER);
        static constexpr size_t TOTAL_HISTORY = IirCoefficients::totalHistoryForOrder(ORDER);
    private:
        constexpr size_t getCOffset(size_t idx) const
        { return Value<size_t>::valid_below(idx, COEFFS) + C_OFFSET; }

        constexpr size_t getDOffset(size_t idx) const
        { return Value<size_t>::valid_below(idx, COEFFS) + D_OFFSET; }

        C &C_(size_t idx)
        { return data[getCOffset(idx)]; }

        C &D_(size_t idx)
        { return data[getDOffset(idx)]; }

        const C &C_(size_t idx) const
        { return data[getCOffset(idx)]; }

        const C &D_(size_t idx) const
        { return data[getDOffset(idx)]; }

        const C *const unsafeC() const
        { return data + C_OFFSET; }

        const C *const unsafeD() const
        { return data + D_OFFSET; }


    public:
        size_t order() const
        { return ORDER; }

        size_t maxOrder() const
        { return ORDER; }

        bool hasFixedOrder() const
        { return true; }

        void setOrder(size_t newOrder)
        {};

        void setC(size_t idx, const C coefficient)
        { C_(idx) = coefficient; }

        void setD(size_t idx, const C coefficient)
        { D_(idx) = coefficient; }

        C getC(size_t idx) const
        { return C_(idx); }

        C getD(size_t idx) const
        { return D_(idx); }

        void setTransparent()
        {
            for (size_t i = 0; i < TOTAL_COEEFS; i++) {
                data[i] = 0.0;
            }
            C_(0) = 1;
        }

        void assign(const IirCoefficients &source)
        {
            if (source.order() == ORDER) {
                for (size_t i = 0; i < COEFFS; i++) {
                    C_(i) = source.getC(i);
                    D_(i) = source.getD(i);
                }
                return;
            }
            throw invalid_argument("Value not below threshold");
        }

        void operator=(const IirCoefficients &source)
        {
            assign(source);
        }

        template<typename S>
        void assign(const FixedSizeIirCoefficients<S, ORDER> &coeffs)
        {
            for (size_t i = 0; i < COEFFS; i++) {
                setC(i, coeffs.getC(i));
                setD(i, coeffs.getD(i));
            }
        }

        template<typename S>
        void operator=(const FixedSizeIirCoefficients<S, ORDER> &coeffs)
        {
            assign(coeffs);
        }

        template<typename S>
        void assign(const VariableSizedIirCoefficients<S> &coeffs)
        {
            if (coeffs.order() != ORDER) {
                throw invalid_argument("FixedSizeIirCoefficients: Source coefficients must be of same order");
            }
            for (size_t i = 0; i < COEFFS; i++) {
                setC(i, coeffs.getC(i));
                setD(i, coeffs.getD(i));
            }
        }

        template<typename S>
        void operator=(const VariableSizedIirCoefficients<S> &coeffs)
        {
            assign(coeffs);
        }

        template<typename S, bool flushToZero>
        S do_filter(
                S *const xHistory, // (ORDER) x value history
                S *const yHistory, // (ORDER) y value history
                S input) const
        {
            return iir_filter_fixed<C, S, ORDER, flushToZero>(
                    unsafeC(),
                    unsafeD(),
                    xHistory,
                    yHistory,
                    input);
        }

        template<typename S>
        S filter(S *const xHistory, S *const yHistory, const S input) const
        {
            return do_filter<S, false>(xHistory, yHistory, input);
        }

        WrappedIirCoefficients<C, FixedSizeIirCoefficients<C, ORDER>> wrap()
        {
            return WrappedIirCoefficients<C, FixedSizeIirCoefficients<C, ORDER>>(*this);
        }

    private:
        alignas(Count<C>::align()) C data[TOTAL_COEEFS];
    };


    template<typename C>
    class VariableSizedIirCoefficients
    {
        const size_t maxOrder_;
        size_t order_;
        C *data_;

        const size_t getCBaseOffset() const
        { return 0; }

        const size_t getDBaseOffset() const
        { return maxOrder_ + 1; }

        const size_t getCOffset(size_t i) const
        { return getCBaseOffset() + Value<size_t>::valid_below_or_same(i, order_); }

        const size_t getDOffset(size_t i) const
        { return getDBaseOffset() + Value<size_t>::valid_below_or_same(i, order_); }

        C &C_(size_t i)
        { return data_[getCOffset(i)]; }

        C &D_(size_t i)
        { return data_[getDOffset(i)]; }

        const C &C_(size_t i) const
        { return data_[getCOffset(i)]; }

        const C &D_(size_t i) const
        { return data_[getDOffset(i)]; }

        const C *const unsafeC() const
        { return data_ + getCBaseOffset(); }

        const C *const unsafeD() const
        { return data_ + getDBaseOffset(); }

    public:
        VariableSizedIirCoefficients(size_t maxOrder) :
                maxOrder_(Value<size_t>::valid_between(maxOrder, 1, 64)),
                order_(maxOrder_),
                data_(new C[IirCoefficients::totalCoefficientsForOrder(maxOrder_)])
        {}

        VariableSizedIirCoefficients(size_t maxOrder, size_t order) :
                maxOrder_(Value<size_t>::valid_between(maxOrder, 1, 64)),
                order_(Value<size_t>::valid_between(order, 1, maxOrder_)),
                data_(new C[IirCoefficients::totalCoefficientsForOrder(maxOrder_)])
        {}

        size_t order() const
        { return order_; }

        size_t maxOrder() const
        { return maxOrder_; }

        bool hasFixedOrder()
        { return false; }

        void setOrder(size_t order)
        { order_ = Value<size_t>::valid_between(order, 1, maxOrder_); }

        void setC(size_t idx, const C coefficient) override
        { C_(idx) = coefficient; }

        void setD(size_t idx, const C coefficient) override
        { D_(idx) = coefficient; }

        C getC(size_t idx) const override
        { return C_(idx); }

        C getD(size_t idx) const override
        { return D_(idx); }

        void assign(const IirCoefficients &source)
        {
            setOrder(source.order());
            for (size_t i = 0; i <= order_; i++) {
                C_(i) = source.getC(i);
                D_(i) = source.getD(i);
            }
        }

        template<typename S, size_t ORDER>
        void assign(const FixedSizeIirCoefficients<S, ORDER> &coeffs)
        {
            if (ORDER > maxOrder()) {
                throw invalid_argument("VariableSizedIirCoefficients: order of source exceeds my max order");
            }
            setOrder(ORDER);
            for (size_t i = 0; i < IirCoefficients::totalCoefficientsForOrder(order_); i++) {
                setC(i, coeffs.getC(i));
                setD(i, coeffs.getD(i));
            }
        }

        template<typename S, size_t ORDER>
        void operator=(const FixedSizeIirCoefficients<S, ORDER> &coeffs)
        {
            assign(coeffs);
        }

        template<typename S, size_t ORDER>
        void assign(const VariableSizedIirCoefficients<S> &coeffs)
        {
            if (coeffs.order() > maxOrder()) {
                throw invalid_argument("VariableSizedIirCoefficients: order of source exceeds my max order");
            }
            setOrder(coeffs.order());
            for (size_t i = 0; i < IirCoefficients::totalCoefficientsForOrder(order_); i++) {
                setC(i, coeffs.getC(i));
                setD(i, coeffs.getD(i));
            }
        }

        template<typename S, size_t ORDER>
        void operator=(const VariableSizedIirCoefficients<S> &coeffs)
        {
            assign(coeffs);
        }

        template<typename S, bool flushToZero = false>
        S do_filter(
                S *const xHistory, // (ORDER) x value history
                S *const yHistory, // (ORDER) y value history
                S input) const
        {
            return iir_filter<C, S, flushToZero>(
                    order_,
                    unsafeC(),
                    unsafeD(),
                    xHistory,
                    yHistory,
                    input);
        }

        template<typename S>
        S filter(
                S *const xHistory, // (ORDER) x value history
                S *const yHistory, // (ORDER) y value history
                S input) const
        {
            return do_filter<S, false>(xHistory, yHistory, input);
        }

        WrappedIirCoefficients<C, VariableSizedIirCoefficients<C>> wrap()
        {
            return WrappedIirCoefficients<C, VariableSizedIirCoefficients<C>>(*this);
        }

        ~VariableSizedIirCoefficients()
        {
            delete[] data_;
        }
    };


    template<typename C, typename COEFFICIENT_CLASS>
    class WrappedIirCoefficients : public IirCoefficients
    {
        COEFFICIENT_CLASS &coefficients_;

    public:
        WrappedIirCoefficients(COEFFICIENT_CLASS &wrapped) : coefficients_(wrapped)
        {}

        virtual size_t order() const override
        { return coefficients_.order(); }

        virtual size_t maxOrder() const override
        { return coefficients_.maxOrder(); }

        virtual bool hasFixedOrder() const override
        { return coefficients_.hasFixedOrder(); }

        virtual void setOrder(size_t newOrder) override
        { coefficients_.setOrder(newOrder); }

        virtual void setC(size_t idx, const double coefficient) override
        { coefficients_.setC(idx, coefficient); }

        virtual void setD(size_t idx, const double coefficient) override
        { coefficients_.setD(idx, coefficient); }

        virtual double getC(size_t idx) const override
        { return coefficients_.getC(idx); }

        virtual double getD(size_t idx) const override
        { return coefficients_.getD(idx); }

        template<typename T>
        void assign(const IirCoefficients &source)
        {
            coefficients_.assign(source);
        }


        template<typename S, bool flushToZero = false>
        S do_filter(
                S *const xHistory, // (ORDER) x value history
                S *const yHistory, // (ORDER) y value history
                S input) const
        {
            coefficients_.template do_filter<S, flushToZero>(xHistory, yHistory, input);
        }

        template<typename S>
        S filter(S *const xHistory, S *const yHistory, const S input) const
        {
            return do_filter<S, false>(xHistory, yHistory, input);
        }

        virtual ~WrappedIirCoefficients() = default;
    };

    template<typename C, size_t CHANNELS, size_t ORDER>
    struct FixedSizeIirCoefficientFilter
    {
        using Coefficients = FixedSizeIirCoefficients<C, ORDER>;

        static constexpr size_t historySize()
        { return IirCoefficients::historyForOrder(ORDER); }

        static constexpr size_t coefficientSize()
        { return IirCoefficients::coefficientsForOrder(ORDER); }

        struct History
        {
            C x[historySize()];
            C y[historySize()];
        };

        struct SingleChannelFilter : public tdap::Filter<C>
        {
            FixedSizeIirCoefficientFilter<C, CHANNELS, ORDER> &wrapped_;

            virtual void reset()
            { wrapped_.reset(); }

            virtual C filter(C input)
            {
                return wrapped_.filter(0, input);
            }

            SingleChannelFilter(FixedSizeIirCoefficientFilter<C, CHANNELS, ORDER> &wrapped) :
                    wrapped_(wrapped)
            {}
        };

        struct MultiChannelFilter : public tdap::MultiFilter<C>
        {
            FixedSizeIirCoefficientFilter<C, CHANNELS, ORDER> &wrapped_;

            virtual size_t channels() const override
            { return CHANNELS; }

            virtual void reset() override
            { wrapped_.reset(); }

            virtual C filter(size_t idx, C input) override
            {
                return wrapped_.filter(idx, input);
            }

            MultiChannelFilter(FixedSizeIirCoefficientFilter &wrapped) :
                    wrapped_(wrapped)
            {}
        };

        Coefficients coefficients_;
        History history[CHANNELS];

        FixedSizeIirCoefficientFilter() = default;

        FixedSizeIirCoefficientFilter(const Coefficients &coefficients) : coefficients_(coefficients)
        {}

        void reset()
        {
            for (size_t channel = 0; channel < CHANNELS; channel++) {
                for (size_t t = 0; t < historySize(); t++) {
                    history[channel].x[t] = 0;
                    history[channel].y[t] = 0;
                }
            }
        }

        template<bool flushToZero>
        C do_filter(size_t channel, C input)
        {
            IndexPolicy::array(channel, CHANNELS);
            return coefficients_.template do_filter<C, flushToZero>(history[channel].x, history[channel].y, input);
        }

        C filter(size_t channel, C input)
        {
            return do_filter<false>(channel, input);
        }

        template<size_t N, typename ...A>
        void filterArray(const FixedSizeArrayTraits<C, N, A...> &input, FixedSizeArrayTraits<C, N, A...> &output)
        {
            for (size_t channel = 0; channel < Value<size_t>::min(CHANNELS, N); channel++) {
                output[channel] = filter(channel, input[channel]);
            }
        }

        template<typename ...A>
        void filterArray(const ArrayTraits<C, A...> &input, ArrayTraits<C, A...> &output)
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

        Filter<C> *createFilter()
        {
            return new SingleChannelFilter(*this);
        }

        MultiFilter<C> *createMultiFilter()
        {
            return new MultiChannelFilter(*this);
        }
    };

    template<typename S, size_t ORDER, size_t CHANNELS, bool FLUSH = false, size_t ALIGN = Count<S>::align()>
    struct MultiFilterData
    {
        static_assert(is_arithmetic<S>::value, "Value type should be arithmetic");
        static_assert(ORDER > 0, "ORDER of filter must be positive");
        static_assert(CHANNELS > 0, "CHANNELS of filter must be positive");
        static constexpr size_t HISTORY = IirCoefficients::historyForOrder(ORDER);


        struct alignas(ALIGN) Vector : public FixedSizeArray<S, HISTORY> {
            using FixedSizeArray<S, HISTORY>::operator=;
            template<typename ...A>
            Vector(const FixedSizeArrayTraits<S, HISTORY, A...> &source) : FixedSizeArray<S, HISTORY>(source)
            {}
            Vector(const S value) : FixedSizeArray<S, HISTORY>(value)
            {}
        };

        static_assert(Count<Vector>::is_valid_sum(CHANNELS, CHANNELS, CHANNELS), "CHANNELS must theoretically fit in memory");

        FixedSizeIirCoefficients<S, ORDER> coeff;
        Vector xHistory[CHANNELS];
        Vector yHistory[CHANNELS];

        void zero()
        {
            for (size_t i = 0; i < CHANNELS; i++) {
                xHistory[i].zero();
                yHistory[i].zero();
            }
        }

        MultiFilterData()
        {
            zero();
        }

        template<typename ...A>
        inline void filter(
                FixedSizeArrayTraits<S, CHANNELS, A...> &target,
                const FixedSizeArrayTraits<S, CHANNELS, A...> &source)
        {
            Vector Y = 0.0;
            const Vector input = source;
            Vector X = input; // source is xN0
            Vector yN0 = 0.0;
            size_t i, j;
            Vector xN1;
            Vector yN1;
            for (i = 0, j = 1; i < ORDER; i++, j++) {
                xN1 = xHistory[i];
                yN1 = yHistory[i];
                xHistory[i] = X;
                X = xN1;
                yHistory[i] = Y;
                Y = yN1;
                yN0 += xN1 * coeff.getC(j);
                yN0 += yN1 * coeff.getD(j);
            }
            yN0 += input * coeff.getC(0);

            if (FLUSH) {
                Denormal::flush(yN0);
            }
            yHistory[0] = yN0;

            target = yN0;
        }
    };



} /* End of name space tdap */

#endif /* TDAP_IIRCOEFFICIENTS_HEADER_GUARD */
