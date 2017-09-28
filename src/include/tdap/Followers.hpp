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
//    template<typename S>
//    class HoldMax
//    {
//        static_assert(is_arithmetic<S>::value,
//                      "Sample type S must be arithmetic");
//        size_t holdSamples;
//        size_t toHold;
//        S holdValue;
//
//    public:
//        HoldMax(size_t holdForSamples, S initialHoldValue = 0) :
//                holdSamples(holdForSamples), toHold(0),
//                holdValue(initialHoldValue)
//        {}
//
//        void resetHold()
//        {
//            toHold = 0;
//        }
//
//        void setHoldCount(size_t newCount)
//        {
//            holdSamples = newCount;
//            if (toHold > holdSamples) {
//                toHold = holdSamples;
//            }
//        }
//
//        size_t getHoldSamples() const
//        {
//            return holdSamples;
//        }
//
//        S apply(S input)
//        {
//            if (input > holdValue) {
//                toHold = holdSamples;
//                holdValue = input;
//                return holdValue;
//            }
//            if (toHold > 0) {
//                toHold--;
//                return holdValue;
//            }
//            return input;
//        }
//    };

/**
 * Follows rises in input and holds those for a number of samples. After that,
 * follows the (lower) input in an integrated fashion.
 */
    template<typename S, typename C>
    class HoldMaxRelease
    {
        static_assert(is_arithmetic<S>::value,
                      "Sample type S must be arithmetic");

        size_t holdSamples;
        size_t toHold;
        S holdValue;
        IntegratorFilter <C> integrator_;

    public:
        HoldMaxRelease(size_t holdForSamples, C integrationSamples,
                       S initialHoldValue = 0) :
                holdSamples(holdForSamples), toHold(0),
                holdValue(initialHoldValue), integrator_(integrationSamples)
        {}

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

        IntegratorFilter <C> &integrator()
        {
            return integrator_;
        }
    };

    template<typename S, typename C>
    class HoldMaxIntegrated
    {
        static_assert(is_arithmetic<S>::value,
                      "Sample type S must be arithmetic");
        HoldMax<S> holdMax;
        IntegratorFilter <C> integrator_;

    public:
        HoldMaxIntegrated(size_t holdForSamples, C integrationSamples,
                          S initialHoldValue = 0) :
                holdMax(holdForSamples, initialHoldValue),
                integrator_(integrationSamples, initialHoldValue)
        {}

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

        IntegratorFilter <C> &integrator()
        {
            return integrator_;
        }
    };

    template<typename S>
    class HoldMaxDoubleIntegrated
    {
        static_assert(is_arithmetic<S>::value,
                      "Sample type S must be arithmetic");
        HoldMax<S> holdMax;
        IntegrationCoefficients <S> coeffs;
        S i1, i2;

    public:
        HoldMaxDoubleIntegrated(size_t holdForSamples, S integrationSamples,
                                S initialHoldValue = 0) :
                holdMax(holdForSamples, initialHoldValue),
                coeffs(integrationSamples),
                i1(initialHoldValue), i2(initialHoldValue)
        {}

        HoldMaxDoubleIntegrated() : HoldMaxDoubleIntegrated(15, 10, 1.0)
        {}

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
            return coeffs.integrate(coeffs.integrate(holdMax.apply(input), i1),
                                    i2);
        }

        S setValue(S x)
        {
            i1 = i2 = x;
        }

        S applyWithMinimum(S input, S minimum)
        {
            return coeffs.integrate(
                    coeffs.integrate(holdMax.apply(Values::max(input, minimum)),
                                     i1), i2);
        }
    };

    template<typename C>
    class HoldMaxAttackRelease
    {
        static_assert(is_arithmetic<C>::value,
                      "Sample type S must be arithmetic");
        HoldMax<C> holdMax;
        AttackReleaseFilter <C> integrator_;

    public:
        HoldMaxAttackRelease(size_t holdForSamples, C attackSamples,
                             C releaseSamples, C initialHoldValue = 0) :
                holdMax(holdForSamples, initialHoldValue),
                integrator_(attackSamples, releaseSamples, initialHoldValue)
        {}

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

        AttackReleaseFilter <C> &integrator()
        {
            return integrator_;
        }
    };

    template<typename C>
    class SmoothHoldMaxAttackRelease
    {
        static_assert(is_arithmetic<C>::value,
                      "Sample type S must be arithmetic");
        HoldMax<C> holdMax;
        AttackReleaseSmoothFilter <C> integrator_;

    public:
        SmoothHoldMaxAttackRelease(size_t holdForSamples, C attackSamples,
                                   C releaseSamples, C initialHoldValue = 0) :
                holdMax(holdForSamples, initialHoldValue),
                integrator_(attackSamples, releaseSamples, initialHoldValue)
        {}

        void resetHold()
        {
            holdMax.reset();
        }

        void setHoldCount(size_t newCount)
        {
            holdMax.hold_count_ = newCount;
        }

        C apply(C input)
        {
            return integrator_.integrate(holdMax.get_value(input, integrator_.output));
        }

        void setValue(C x)
        {
            integrator_.setOutput(x);
        }

        size_t getHoldSamples() const
        {
            return holdMax.hold_count_;
        }

        AttackReleaseSmoothFilter <C> &integrator()
        {
            return integrator_;
        }
    };

    template<typename sample_t>
    class Limiter
    {

        Array <sample_t> attack_envelope_;
        Array <sample_t> release_envelope_;
        Array <sample_t> peaks_;

        sample_t threshold_;
        sample_t smoothness_;
        sample_t current_peak_;

        size_t release_count_;
        size_t current_sample;
        size_t attack_samples_;
        size_t release_samples_;

        static void
        create_smooth_semi_exponential_envelope(sample_t *envelope,
                                                const size_t length,
                                                size_t periods)
        {
            const sample_t periodExponent = exp(-1.0 * periods);
            size_t i;
            for (i = 0; i < length; i++) {
                envelope[i] = limiter_envelope_value(i, length, periods,
                                                     periodExponent);
            }
        }

        template<typename...A>
        static void create_smooth_semi_exponential_envelope(
                ArrayTraits<jack_default_audio_sample_t, A...> envelope,
                const size_t length, size_t periods)
        {
            create_smooth_semi_exponential_envelope(envelope + 0,
                                                    length, periods);
        }

        static inline double
        limiter_envelope_value(const size_t i, const size_t length,
                               const double periods,
                               const double periodExponent)
        {
            const double angle = M_PI * (i + 1) / length;
            const double ePower = 0.5 * periods * (cos(angle) - 1.0);
            const double envelope =
                    (exp(ePower) - periodExponent) / (1.0 - periodExponent);

            return envelope;
        }

        void generate_envelopes_reset(bool recalculate_attack_envelope = true,
                                      bool recalculate_release_envelope = true)
        {
            if (recalculate_attack_envelope) {
                Limiter::create_smooth_semi_exponential_envelope(
                        attack_envelope_ + 0, attack_samples_, smoothness_);
            }
            if (recalculate_release_envelope) {
                Limiter::create_smooth_semi_exponential_envelope(
                        release_envelope_ + 0, release_samples_, smoothness_);
            }
            release_count_ = 0;
            current_peak_ = 0;
            current_sample = 0;
        }

        inline const sample_t
        getAmpAndMoveToNextSample(const sample_t newValue)
        {
            const sample_t pk = peaks_[current_sample];
            peaks_[current_sample] = newValue;
            current_sample = (current_sample + attack_samples_ - 1) % attack_samples_;
            const sample_t threshold = threshold_;
            return threshold / (threshold + pk);
        }

    public :
        Limiter(sample_t threshold, sample_t smoothness,
                const size_t max_attack_samples,
                const size_t max_release_samples)
                :
                threshold_(threshold),
                smoothness_(smoothness),
                attack_envelope_(max_attack_samples),
                release_envelope_(max_release_samples),
                peaks_(max_attack_samples),
                release_count_(0),
                current_peak_(0),
                current_sample(0),
                attack_samples_(max_attack_samples),
                release_samples_(max_release_samples)
        {
            generate_envelopes_reset();
        }

        bool reconfigure(size_t attack_samples, size_t release_samples,
                         sample_t threshold, sample_t smoothness)
        {
            if (attack_samples == 0 ||
                attack_samples > attack_envelope_.capacity()) {
                return false;
            }
            if (release_samples == 0 ||
                release_samples > release_envelope_.capacity()) {
                return false;
            }
            sample_t new_threshold = Value<sample_t>::force_between(threshold,
                                                                    0.01, 1);
            sample_t new_smoothness = Value<sample_t>::force_between(smoothness,
                                                                     1, 4);
            bool recalculate_attack_envelope =
                    attack_samples != attack_samples_ ||
                    new_smoothness != smoothness_;
            bool recalculate_release_envelope =
                    release_samples != release_samples_ ||
                    new_smoothness != smoothness_;
            attack_samples_ = attack_samples;
            release_samples_ = release_samples;
            threshold_ = threshold;
            smoothness_ = smoothness;
            generate_envelopes_reset(recalculate_attack_envelope,
                                     recalculate_release_envelope);
            return true;
        }

        bool set_smoothness(sample_t smoothness)
        {
            return reconfigure(attack_samples_, release_samples_, threshold_, smoothness);
        }

        bool set_attack_samples(size_t samples)
        {
            return reconfigure(samples, release_samples_, threshold_, smoothness_);
        }

        bool set_release_samples(size_t samples)
        {
            return reconfigure(attack_samples_, samples, threshold_, smoothness_);
        }

        bool set_threshold(double threshold)
        {
            return reconfigure(attack_samples_, release_samples_, threshold, smoothness_);
        }

        const sample_t
        limiter_submit_peak_return_amplification(sample_t samplePeakValue)
        {
            static int cnt = 0;
            const size_t prediction = attack_envelope_.size();

            const sample_t relativeValue =
                    samplePeakValue - threshold_;
            const int withinReleasePeriod =
                    release_count_ < release_envelope_.size();
            const sample_t releaseCurveValue = withinReleasePeriod ?
                                               current_peak_ *
                                               release_envelope_[release_count_]
                                                                   : 0.0;

            if (relativeValue < releaseCurveValue) {
                /*
                 * The signal is below either the threshold_ or the projected release curve of
                 * the last highest peak. We can just "follow" the release curve.
                 */
                if (withinReleasePeriod) {
                    release_count_++;
                }
                cnt++;
                return getAmpAndMoveToNextSample(releaseCurveValue);
            }
            /**
             * Alas! We can forget about the last peak.
             * We will have alter the prediction values so that the current,
             * new peak, will be "predicted".
             */
            release_count_ = 0;
            current_peak_ = relativeValue;
            /**
             * We will try to project the default attack-predicition curve,
             * (which is the relativeValue (peak) with the nicely smooth
             * attackEnvelope) into the "future".
             * As soon as this projection hits (is not greater than) a previously
             * predicted value, we proceed to the next step.
             */
            const size_t max_t = attack_samples_ - 1;
            size_t tClash; // the hitting point
            size_t t;
            for (tClash = 0, t = current_sample; tClash < prediction; tClash++) {
                t = t < max_t ? t + 1 : 0;
                const sample_t existingValue = peaks_[t];
                const sample_t projectedValue =
                        attack_envelope_[tClash] * relativeValue;
                if (projectedValue <= existingValue) {
                    break;
                }
            }

            /**
             * We have a clash. We will now blend the peak with the
             * previously predicted curve, using the attackEnvelope
             * as blend-factor. If tClash is smaller than the complete
             * prediction-length, the attackEnvelope will be compressed
             * to fit exactly up to that clash point.
             * Due to the properties of the attackEnvelope it can be
             * mathematically proven that the newly produced curve is
             * always larger than the previous one in the clash range and
             * will blend smoothly with the existing curve.
             */
            size_t i;
            for (i = 0, t = current_sample; i < tClash; i++) {
                t = t < max_t ? t + 1 : 0;
                // get the compressed attack_envelope_ value
                const sample_t blendFactor = attack_envelope_[i *
                                                            (prediction - 1) /
                                                            tClash];
                // blend the peak value with the previously calculated peak
                peaks_[t] = relativeValue * blendFactor +
                           (1.0 - blendFactor) * peaks_[t];
            }

            return getAmpAndMoveToNextSample(relativeValue);
        }
    };


} /* End of name space tdap */

#endif /* TDAP_FOLLOWERS_HEADER_GUARD */
