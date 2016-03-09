/*
 * tdap/Weighting.hpp
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

#ifndef TDAP_WEIGHTING_HEADER_GUARD
#define TDAP_WEIGHTING_HEADER_GUARD

#include <iostream>
#include <cmath>

#include <tdap/IirBiquad.hpp>
#include <tdap/IirButterworth.hpp>

namespace tdap {
using namespace std;


struct ACurves
{
	// Defines the bulk of the curve with a parameterized equalizer
	static constexpr double PARAM_CENTER =        2516.0;
	static constexpr double PARAM_GAIN =          19.1;
	static constexpr double PARAM_BANDWIDTH =     8.12;

	// Cuts off higher and lower parts
	static constexpr double HIGH_PASS_FREQUENCY = 125;
	static constexpr double LOW_PASS_FREQUENCY =  21443;

	// Overall filter gain for 0dB @ 1kHz
	static constexpr double OVERALL_GAIN =        0.0597736;

	struct WeightPoint
	{
		double frequency;
		double gain;
	};

	// Weight-points to use for parameter fitting.
	static constexpr WeightPoint hz_low { 100.0, 0.1152 };
	static constexpr WeightPoint hz_mid { 400.0, 0.5888 };
	static constexpr WeightPoint hz_unity { 1000.0, 1.0 };
	static constexpr WeightPoint hz_top { 2516.0, 1.15213 };
	static constexpr WeightPoint hz_high { 10000, 0.7674 };

	static void setFirstOrder(IirCoefficients &coeffs)
	{
		if (coeffs.order() != 1) {
			if (coeffs.hasFixedOrder()) {
				throw std::invalid_argument("Coefficient argument cannot be set to order 1");
			}
			coeffs.setOrder(1);
		}
	}

	static void setCurveParameters(IirCoefficients &coeffs, double sampleRate)
	{
		BiQuad::setParametric(coeffs, sampleRate, PARAM_CENTER, PARAM_GAIN, PARAM_BANDWIDTH);
	}

	static void setLowPassParameters(IirCoefficients &coeffs, double sampleRate)
	{
		setFirstOrder(coeffs);
		Butterworth::create(coeffs, sampleRate, LOW_PASS_FREQUENCY, Butterworth::Pass::LOW, 1.0);
	}

	static void setHighPassParameters(IirCoefficients &coeffs, double sampleRate)
	{
		setFirstOrder(coeffs);
		Butterworth::create(coeffs, sampleRate, HIGH_PASS_FREQUENCY, Butterworth::Pass::HIGH, 1.0);
	}

	template<typename SAMPLE>
	class Coefficients
	{
		FixedSizeIirCoefficients<SAMPLE, 2> curve_;
		FixedSizeIirCoefficients<SAMPLE, 1> highPass_;
		FixedSizeIirCoefficients<SAMPLE, 1> lowPass_;
	public:
		Coefficients() = default;
		Coefficients(double sampleRate)
		{
			setSampleRate(sampleRate);
		}
		void setSampleRate(double sampleRate)
		{
			auto coeffs1 = curve_.wrap();
			setCurveParameters(coeffs1, sampleRate);
			auto coeffs2 = highPass_.wrap();
			setHighPassParameters(coeffs2, sampleRate);
			auto coeffs3 = lowPass_.wrap();
			setHighPassParameters(coeffs3, sampleRate);
		}
		const FixedSizeIirCoefficients<SAMPLE, 2> &curve() const
		{
			return curve_;
		}
		const FixedSizeIirCoefficients<SAMPLE, 1> &highPass() const
		{
			return highPass_;
		}
		const FixedSizeIirCoefficients<SAMPLE, 1> &lowPass() const
		{
			return lowPass_;
		}
	};

	template<typename SAMPLE, size_t CHANNELS>
	class Filter
	{
		Coefficients<SAMPLE> coefficients_;
		struct History
		{
			SAMPLE curveX[IirCoefficients::historyForOrder(2)];
			SAMPLE curveY[IirCoefficients::historyForOrder(2)];
			SAMPLE lowX[IirCoefficients::historyForOrder(1)];
			SAMPLE lowY[IirCoefficients::historyForOrder(1)];
			SAMPLE highX[IirCoefficients::historyForOrder(1)];
			SAMPLE highY[IirCoefficients::historyForOrder(1)];

			void reset()
			{
				for (size_t i = 0; i < IirCoefficients::historyForOrder(2); i++) {
					curveX[i] = 0;
					curveY[i] = 0;
				}
				for (size_t i = 0; i < IirCoefficients::historyForOrder(1); i++) {
					lowX[i] = 0;
					lowY[i] = 0;
					highX[i] = 0;
					highY[i] = 0;
				}
			}
		};
		History history_[CHANNELS];

		struct SingleChannelFilter: public tdap::Filter<SAMPLE>
		{
			Filter<SAMPLE, CHANNELS> &wrapped_;

			virtual void reset() { wrapped_.reset(); }
			virtual SAMPLE filter(SAMPLE input)
			{
				return wrapped_.filter(0, input);
			}

			SingleChannelFilter(Filter<SAMPLE, CHANNELS>  &wrapped) :
				wrapped_(wrapped) {}
			SingleChannelFilter(SingleChannelFilter &&source) : wrapped_(source.wrapped_) {}
		};

		struct MultiChannelFilter: public tdap::MultiFilter<SAMPLE>
		{
			Filter<SAMPLE, CHANNELS> &wrapped_;

			virtual size_t channels() const override { return CHANNELS; }
			virtual void reset() override { wrapped_.reset(); }
			virtual SAMPLE filter(size_t idx, SAMPLE input) override
			{
				return wrapped_.filter(idx, input);
			}

			MultiChannelFilter(Filter<SAMPLE, CHANNELS>  &wrapped) :
				wrapped_(wrapped) {}
			MultiChannelFilter(MultiChannelFilter &&source) : wrapped_(source.wrapped_) {}
		};

	public :
		Filter() = default;
		Filter(const Coefficients<SAMPLE> &coefficients) : coefficients_(coefficients) {}
		Filter(double sampleRate) : coefficients_(sampleRate) {}

		void setSampleRate(double sampleRate)
		{
			coefficients_.setSampleRate(sampleRate);
		}

		void reset()
		{
			for (size_t i = 0; i < CHANNELS; i++) {
				history_[i].reset();
			}
		}

		template<bool flushToZero>
		SAMPLE do_filter(size_t channel, SAMPLE input)
		{
			IndexPolicy::array(channel, CHANNELS);
			SAMPLE y = input;
			y = coefficients_.curve().do_filter<SAMPLE, flushToZero>(history_[channel].curveX, history_[channel].curveY, y);
			y = coefficients_.lowPass().do_filter<SAMPLE, flushToZero>(history_[channel].lowX, history_[channel].lowY, y);
			y = coefficients_.highPass().do_filter<SAMPLE, flushToZero>(history_[channel].highX, history_[channel].highY, y);
			return OVERALL_GAIN * y;
		}

		SAMPLE filter(size_t channel, SAMPLE input)
		{
			return do_filter<false>(channel, input);
		}

		template<size_t N, typename ...A>
		void filterArray(const FixedSizeArrayTraits<SAMPLE, N, A...> &input, FixedSizeArrayTraits<SAMPLE, N, A...> &output)
		{
			for (size_t channel = 0; channel < Value<size_t>::min(CHANNELS, N); channel++) {
				output[channel] = filter(channel, input[channel]);
			}
		}

		template<typename ...A>
		void filterArray(const ArrayTraits<SAMPLE, A...> &input, ArrayTraits<SAMPLE, A...> &output)
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

		tdap::Filter<SAMPLE> * createFilter()
		{
			return new SingleChannelFilter(*this);
		}

		tdap::MultiFilter<SAMPLE> * createMultiFilter()
		{
			return new MultiChannelFilter(*this);
		}

	};

	template<size_t SAMPLERATE_SCALE>
	struct SingleParametricBestFit
	{
		static_assert(SAMPLERATE_SCALE >= 8 && SAMPLERATE_SCALE <= 10000, "Need sensibly high sample rate");
		class DiscreteSineFunction
		{
			size_t period_;
			size_t time_;
			double timeFactor_;

			size_t getAndIncreaseTime()
			{
				size_t t = time_++;
				time_ %= period_;
				return t;
			}

		public:
			DiscreteSineFunction(double relativeFrequency) :
				period_(0.5 + 1.0 / relativeFrequency),
				time_(0),
				timeFactor_(2 * M_PI / period_) { }

			double next()
			{
				return sin(timeFactor_ * getAndIncreaseTime());
			}

			void reset()
			{
				time_ = 0;
			}

			double relativeFrequency() const { return 1.0 / period_; }

			double setRelativeFrequency(double relativeFrequency)
			{
				period_ = 0.5 + 1.0 / relativeFrequency;
				timeFactor_ = M_PI * 2 / period_;
				time_ = 0;
				return relativeFrequency;
			}
		};


		static constexpr double sampleRate() { return hz_top.frequency * SAMPLERATE_SCALE; }
		static constexpr double relativeFrequency(double f) { return f / sampleRate(); }

		static constexpr double rfreq_low = relativeFrequency(hz_low.frequency);
		static constexpr double rfreq_mid = relativeFrequency(hz_mid.frequency);
		static constexpr double rfreq_unity = relativeFrequency(hz_unity.frequency);
		static constexpr double rfreq_top = relativeFrequency(hz_top.frequency);
		static constexpr double rfreq_high = relativeFrequency(hz_high.frequency);

		static constexpr size_t LOW = 0;
		static constexpr size_t MID = 1;
		static constexpr size_t UNITY = 2;
		static constexpr size_t TOP = 3;
		static constexpr size_t HIGH = 4;
		static constexpr size_t COUNT = 5;

		DiscreteSineFunction sin_low;
		DiscreteSineFunction sin_mid;
		DiscreteSineFunction sin_unity;
		DiscreteSineFunction sin_top;
		DiscreteSineFunction sin_high;

		double totalGain;
		BiquadFilter<double, COUNT> parametric;
		FixedSizeIirCoefficientFilter<double, COUNT, 1> highPass;
		FixedSizeIirCoefficientFilter<double, COUNT, 1> lowPass;
		double parametricGain;
		double parametricBandwidth;
		double highPassFreq;
		double lowPassFreq;

		SingleParametricBestFit() :
			sin_low(rfreq_low), sin_mid(rfreq_mid), sin_unity(rfreq_unity),
			sin_top(rfreq_top), sin_high(rfreq_high),
			parametricGain(10),
			parametricBandwidth(4),
			totalGain(1 / parametricGain / hz_top.gain) { }

		void measure(double gains[COUNT])
		{
			sin_low.reset();
			sin_unity.reset();
			sin_top.reset();
			sin_low.reset();

			for (size_t i = 0; i < COUNT; i++) {
				gains[i] = 0;
			}

			auto wrappedPara = parametric.coefficients_.wrap();
			auto wrappedHighPass = highPass.coefficients_.wrap();
			auto wrappedLowPass = lowPass.coefficients_.wrap();
			BiQuad::setParametric(wrappedPara, sampleRate(), hz_top.frequency, parametricGain, parametricBandwidth);
			Butterworth::create(wrappedHighPass, sampleRate(), highPassFreq, Butterworth::Pass::HIGH, 1.0);
			Butterworth::create(wrappedLowPass, sampleRate(), lowPassFreq, Butterworth::Pass::LOW, 1.0);
			auto paraFilter = parametric.wrapMulti();
			auto highPassFilter = highPass.wrapMulti();
			auto lowPassFilter = lowPass.wrapMulti();
			size_t count = 0.5 + sampleRate();
			// Reduce starting effects
			for (size_t i = 0; i < count; i++) {
				lowPassFilter.filter(LOW, highPassFilter.filter(LOW, paraFilter.filter(LOW, sin_low.next())));
				lowPassFilter.filter(MID, highPassFilter.filter(MID, paraFilter.filter(MID, sin_mid.next())));
				lowPassFilter.filter(UNITY, highPassFilter.filter(UNITY, paraFilter.filter(UNITY, sin_unity.next())));
				lowPassFilter.filter(TOP, highPassFilter.filter(TOP, paraFilter.filter(TOP, sin_top.next())));
				lowPassFilter.filter(HIGH, highPassFilter.filter(HIGH, paraFilter.filter(HIGH, sin_high.next())));
			}
			//measure
			for (size_t i = 0; i < count; i++) {
				gains[LOW] = Value<double>::max(gains[LOW], fabs(lowPassFilter.filter(LOW, highPassFilter.filter(LOW, paraFilter.filter(LOW, sin_low.next())))));
				gains[MID] = Value<double>::max(gains[MID], fabs(lowPassFilter.filter(MID, highPassFilter.filter(MID, paraFilter.filter(MID, sin_mid.next())))));
				gains[UNITY] = Value<double>::max(gains[UNITY], fabs(lowPassFilter.filter(UNITY, highPassFilter.filter(UNITY, paraFilter.filter(UNITY, sin_unity.next())))));
				gains[TOP] = Value<double>::max(gains[TOP], fabs(lowPassFilter.filter(TOP, highPassFilter.filter(TOP, paraFilter.filter(TOP, sin_top.next())))));
				gains[HIGH] = Value<double>::max(gains[HIGH], fabs(lowPassFilter.filter(HIGH, highPassFilter.filter(HIGH, paraFilter.filter(HIGH, sin_high.next())))));
			}
			totalGain = 1.0 / gains[UNITY];
			for (size_t i = 0; i < COUNT; i++) {
				gains[i] *= totalGain;
			}
		}

		void fitTopUnityAndMid(double gains[COUNT])
		{
			while (true) {
				while (true) {
					measure(gains);

					if (Value<double>::relative_distance_within(gains[TOP], hz_top.gain, 0.01)) {
						break;
					}
					double bandwidth = parametricBandwidth;
					if (gains[TOP] > hz_top.gain) {
						parametricBandwidth = BiQuad::limitedBandwidth(parametricBandwidth * 1.04);
					}
					else {
						parametricBandwidth = BiQuad::limitedBandwidth(parametricBandwidth /= 1.004);
					}
					if (parametricBandwidth == bandwidth) {
						break;
					}
				}
				if (Value<double>::relative_distance_within(gains[MID], hz_mid.gain, 0.01)) {
					break;
				}
				if (gains[MID] < hz_mid.gain) { // too sharp
					parametricGain *= 0.99;
				}
				else {
					parametricGain /= 0.9;
				}
			}
		}

		void findParameters()
		{
			double gains[COUNT];

			parametricGain = 20;
			parametricBandwidth = 2 * hz_top.frequency / hz_mid.frequency;
			highPassFreq = 40;
			lowPassFreq = 20000;
			cout << "Start bandwidth " << parametricBandwidth << endl;
			while (true) {
				fitTopUnityAndMid(gains);
				bool lowOk = Value<double>::relative_distance_within(gains[LOW], hz_low.gain, 0.01);
				bool highOk = Value<double>::relative_distance_within(gains[HIGH], hz_high.gain, 0.05);
				if (lowOk && highOk) {
					break;
				}
				if (!lowOk) {
					if (gains[LOW] > hz_low.gain) { // too sharp
						highPassFreq *= 1.1;
					}
					else {
						highPassFreq /= 1.01;
					}
				}
				if (!highOk) {
					if (gains[HIGH] > hz_high.gain) { // too sharp
						lowPassFreq /= 1.1;
					}
					else {
						lowPassFreq *= 1.01;
					}
				}
			}
		}
	};
};



} /* End of name space tdap */

#endif /* TDAP_WEIGHTING_HEADER_GUARD */
