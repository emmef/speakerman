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
#ifdef TDAP_FOLLOWERS_DEBUG_LOGGING
#include <cstdio>
#define TDAP_FOLLOWERS_INFO(...) std::printf(__VA_ARGS__)
#else
#define TDAP_FOLLOWERS_INFO(...)
#endif

#if TDAP_FOLLOWERS_DEBUG_LOGGING > 1
#define TDAP_FOLLOWERS_DEBUG(...) std::printf(__VA_ARGS__)
#else
#define TDAP_FOLLOWERS_DEBUG(...)
#endif

#if TDAP_FOLLOWERS_DEBUG_LOGGING > 2
#define TDAP_FOLLOWERS_TRACE(...) std::printf(__VA_ARGS__)
#else
#define TDAP_FOLLOWERS_TRACE(...)
#endif

namespace tdap {

    using namespace std;
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

        SmoothHoldMaxAttackRelease() = default;

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

    template<typename S>
    class TriangularFollower
    {
        class Node {
            size_t position_;
            S value_;
            S delta_;

            S projectedValue(ssize_t pointPosition) const
            {
                return value_ + delta_ * (pointPosition - static_cast<ssize_t>(position_));
            }
        public:
            /**
             *Constructs a new node with the exact given values.
             * @param pos The position of the Node
             * @param v The value of the Node
             * @param d The delta before the position of the node
             */
            Node(size_t pos, S v, S d) :
            position_(pos),
            value_(v),
            delta_(d)
            {}
            Node() : Node(0,1,1) {}
            /**
             * Constructs a node from the current situation to the given one.
             *
             * @param currentPosition The position where we want to construct route to node
             * @param currentValue The (detected) value at the current position
             * @param newPosition The position of the Node
             * @param newValue The value at the new position
             */
            Node(size_t currentPosition, S currentValue, size_t newPosition, S newValue) :
                position_(newPosition),
                value_(newValue),
                delta_((newValue - currentValue) / (1 + newPosition - currentPosition))
            {
                TDAP_FOLLOWERS_TRACE("# \t\t\t Node from (%zu, %lf) -> (%zu, %lf)\n",
                        currentPosition, currentValue, newPosition, newValue);
            }
            size_t position() const { return position_; }
            S value() const { return value_; }
            S delta() const { return delta_; }
            /**
             * Projects the value of the point position, using the node properties.
             * @param pointPosition The point position
             * @return the projected value
             */
            inline S project(size_t pointPosition) const
            {
                TDAP_FOLLOWERS_TRACE("# \t\t ");
                print();
                S result = projectedValue(pointPosition);
                TDAP_FOLLOWERS_TRACE(
                        ".project(%zu) = %lf\n",
                        pointPosition, result);
                return result;
            }

            /**
             * Projects the value of this node's position, projected from the
             * other node's properties.
             * @param other The other node.
             * @return the projected value
             */
            inline S projectedFrom(const Node &other) const
            {
                TDAP_FOLLOWERS_TRACE("# \t\t ");
                print();
                TDAP_FOLLOWERS_TRACE(".projectedFrom(");
                other.print();
                S result = other.projectedValue(position_);
                TDAP_FOLLOWERS_TRACE(")=%lf\n", result);
                return result;
            }
            inline void print() const
            {
                print(position_, value_, delta_);
            }
            static inline void print(size_t position, S value, S delta)
            {
                TDAP_FOLLOWERS_TRACE(
                        "{position=%zu, value=%lf, delta=%lf}",
                        position, value, delta);

            }
        };

        class NodeManager
        {
            size_t maxNodes_;
            Node * const node_;
            size_t count_;
            size_t start_;

            size_t validMaxNodes(size_t maxNodes)
            {
                if (maxNodes < 2) {
                    throw invalid_argument("TriangularFollower::NodeManager::<init>: Number of nodes must be larger than 2.");
                }
                if (maxNodes > Count<Node>::max()) {
                    throw invalid_argument("TriangularFollower::NodeManager::<init>: Number of nodes exceeds maximum.");
                }
                return maxNodes;
            }
            inline size_t fromFirstIndex(size_t index) const
            {
                return start_ + IndexPolicy::array(index, count_);
            }
            inline size_t lastIndex() const
            {
                return start_ + count_ - 1;
            }
            inline size_t fromLastIndex(size_t index) const
            {
                return lastIndex() - IndexPolicy::array(index, count_);
            }
        public:
            NodeManager(size_t maxNodes)
            :
                maxNodes_(validMaxNodes(maxNodes)),
                node_(new Node[maxNodes_]),
                count_(0),
                start_(0)
                {}

            inline size_t count() const { return count_; }
            inline bool hasNodes() const { return count_ > 0; }
            inline const Node *first() const { return count_ > 0 ? node_ + start_: nullptr; }
            inline const Node *last() const { return count_ > 0 ? node_ + start_ + count_ - 1 : nullptr; }
            inline const Node *fromFirst(size_t index) const
            {
                if (count_ > 0) {
                    return node_ + fromFirstIndex(index);
                }
                return nullptr;
            }
            inline const Node *fromLast(size_t index) const
            {
                return count_ > 0 ? node_ + fromLastIndex(index) : nullptr;
            }
            inline const Node *next()
            {
                if (count_ == 0) {
                    throw runtime_error("TriangularFollower::NodeManager::add: No nodes to proceed to");
                }
                if (--count_ == 0) {
                    start_ = 0;
                }
                else {
                    start_++;
                }
                return first();
            }
            inline void reset()
            {
                TDAP_FOLLOWERS_TRACE("# \t TriangularFollower::NodeManager::reset()\n");
                count_ = 0;
                start_ = 0;
            }
            inline void add(size_t at, const Node &source)
            {
                if (at > count_) {
                    throw runtime_error("TriangularFollower::NodeManager::add: Number of nodes exceeded: cannot create new one");
                }
                count_ = at;
                if (count_ == 0) {
                    start_ = 0;
                }
                add(source);
            }
            inline void add(const Node &source)
            {
                if (count_ >= maxNodes_) {
                    throw runtime_error("TriangularFollower::NodeManager::add: Number of nodes exceeded: cannot create new one");
                }
                TDAP_FOLLOWERS_TRACE("# \t TriangularFollower::NodeManager::add(");
                source.print();
                node_[count_++] = source;
                TDAP_FOLLOWERS_TRACE(") { count=%zu; }\n", count_);
            }
            inline bool isInReleaseOrIdle() const
            {
                bool result = count_ <= 1;
                TDAP_FOLLOWERS_TRACE("# \t isInReleaseOrIdle() = %i\n", result);
                return result;
            }
            inline bool isBelowReleaseEnvelope(size_t newSamplePtr, const S newPeakValue) const
            {
                TDAP_FOLLOWERS_TRACE("# \t isBelowReleaseEnvelope(new-peak-at=%zu, new-peak-value=%lf)\n", newSamplePtr, newPeakValue);
                S projected = last()->project(newSamplePtr);
                bool result = newPeakValue <= projected;
                TDAP_FOLLOWERS_TRACE("# \t isBelowReleaseEnvelope = %i { nodes=%zu; }\n", result, count());
                return result;
            }
            inline bool isBelowPeakAttack(const Node &newPeak, size_t lastIndex) const
            {
                TDAP_FOLLOWERS_TRACE("# \t isBelowPeakAttack(");
                newPeak.print();
                TDAP_FOLLOWERS_TRACE(", nodeIndex=%zu)", lastIndex);
                Node *at = fromLast(lastIndex);
                if (at == nullptr) {
                    return false;
                }
                S projected = at->projectedFrom(newPeak);
                bool result = at->value() < projected;
                TDAP_FOLLOWERS_TRACE("# \t isBelowPeakAttack = %i\n", result);
                return result;
            }
            inline ssize_t getLastAbovePeakAttackEnvelope(const Node &newPeak)
            {
                TDAP_FOLLOWERS_TRACE("# \tgetLastAbovePeakAttackEnvelope(");
                newPeak.print();
                if (count() < 2) {
                    TDAP_FOLLOWERS_TRACE(") : first peak\n");
                    return -1;
                }
                TDAP_FOLLOWERS_TRACE(") : search\n");
                for (ssize_t index = fromLastIndex(1); index >= (ssize_t )start_; index--) {
                    const Node * node = node_ + index;
                    S nodeValue = node->value();
                    TDAP_FOLLOWERS_TRACE("# \t\t (%zu) ", index);
                    node->print();
                    TDAP_FOLLOWERS_TRACE("\n");
                    S projected = newPeak.project(node->position());
                    if (nodeValue > projected) {
                        TDAP_FOLLOWERS_TRACE("# \t\t FOUND\n");
                        return index - start_;
                    }
                }
                return -1;
            }
            ~NodeManager()
            {
                if (node_) {
                    delete [] node_;
                }
            }
        };

        NodeManager nodes_;
        S threshold_ = 1;
        S detect_ = 0;
        size_t position_ = 0;
        size_t attSamples_ = 1;
        size_t relSamples_ = 1;

        S constructNewFirstPeak(const S value)
        {
            TDAP_FOLLOWERS_TRACE("# \t constructNewFirstPeak(value=%lf): reached towards node\n", value);
            size_t newPtr = position_ + attSamples_ - 1;
            nodes_.add(0, { position_, detect_, newPtr, value });
            nodes_.add({newPtr + relSamples_, threshold_,
                           (threshold_ - value) / relSamples_});
            return detect_;//continueAsNormal();
        }
        S constructFromNode(const size_t nodePtr, const size_t position, const S value)
        {
            TDAP_FOLLOWERS_TRACE("# \t constructFromNode(nodePtr=%zu, position=%zu, value=%lf)\n", nodePtr, position, value);
            if (nodePtr == 0) {
                return constructNewFirstPeak(value);
            }
            const Node *from = nodes_.fromFirst(nodePtr);

            nodes_.add(nodePtr + 1, { from->position(), from->value(), position, value});
            nodes_.add({ position, value, position + relSamples_, threshold_ });
            nodes_ = nodePtr + 2;
            return continueAsNormal();
        }

        S continueAsNormal()
        {
            TDAP_FOLLOWERS_TRACE("# \t continueAsNormal()\n");
            if (!nodes_.hasNodes()) {
                TDAP_FOLLOWERS_TRACE("# \t\t nodes=0\n");
                position_++; // Might want to reset to zero instead
                return threshold_;
            }
            const Node *towards = nodes_.first();
            TDAP_FOLLOWERS_TRACE("# \t\t detect=%lf, towards=", detect_);
//            detect_ += towards->delta();
            detect_ = towards->project(position_);
            if (position_++ >= towards->position()) {
                towards = nodes_.next();
                if (towards == nullptr) {
                    TDAP_FOLLOWERS_TRACE("# \t\t\t -- Last node encountered: reset\n");
                    return detect_;
                }
                TDAP_FOLLOWERS_TRACE("# \t\t\t -- Move to next node\n");
            }
            return detect_;
        }

        inline S followAlgorithm(const S value)
        {
            if (value < threshold_) {
                TDAP_FOLLOWERS_TRACE("# \t below threshold %lf\n", threshold_);
                return continueAsNormal();
            }
            size_t newPtr = position_ + attSamples_;
            if (nodes_.count() == 0) {
                nodes_.add(0, { position_, threshold_, newPtr, value });
                nodes_.add({ newPtr + 1, value, newPtr + relSamples_, threshold_ });
                return continueAsNormal();
            }
            if (nodes_.isBelowReleaseEnvelope(newPtr, value)) {
                return continueAsNormal();
            }
            if (nodes_.count() == 1) {
                nodes_.add(0, { position_, detect_, newPtr, value });
                nodes_.add({ newPtr + 1, value, newPtr + relSamples_, threshold_ });
                return continueAsNormal();
            }
            const Node newPeak = { position_, threshold_, newPtr, value };
            const Node newRelease = { newPtr + 1, value, newPtr + relSamples_, threshold_ };
            ssize_t higherNode = nodes_.getLastAbovePeakAttackEnvelope(newPeak);
            if (higherNode < 0) {
                nodes_.add(0, { position_, detect_, newPtr, value});
                nodes_.add(newRelease);
            }
            else {
                const Node * from = nodes_.fromFirst(higherNode);
                nodes_.add(higherNode + 1, {from->position() + 1, from->value(), newPeak.position(), newPeak.value()});
                nodes_.add(newRelease);
            }
            return continueAsNormal();
        }

    public:
        TriangularFollower(size_t maxNodes)
        :
            nodes_(maxNodes) {
            TDAP_FOLLOWERS_DEBUG("# TriangularFollower(%zu)\n", maxNodes);
        }

        ~TriangularFollower()
        {
        }

        inline S follow(const S value)
        {
            TDAP_FOLLOWERS_TRACE("### follow(%lf) // ptr=%zu, nodes=%zu\n", value, position_, nodes_.count());
            S result = followAlgorithm(value);
            TDAP_FOLLOWERS_TRACE("### follow = %lf\n", result);
            return result;
        }

        void setTimeConstantAndSamples(size_t attackSamples, size_t releaseSamples, S threshold)
        {
            attSamples_ = Sizes::valid_positive(attackSamples);
            relSamples_ = Sizes::valid_positive(releaseSamples);
            threshold_ = threshold;
            detect_ = threshold_;
            nodes_.reset();
            position_ = 0;
        }
    };

    template<typename S>
    class CompensatedAttack
    {
        S compensation = 1;
        S previousPeak = 0;
        S compensatedPeak = 0;
        S detectedPeak = 0;
        IntegrationCoefficients<S> coefficients;
    public:

        void setTimeConstantAndSamples(size_t timeConstantSamples, size_t samples, S initialValue)
        {
            coefficients.setCharacteristicSamples(timeConstantSamples);
            S output = 0.0;
            for (size_t sample = 0; sample < samples; sample++) {
                output += coefficients.inputMultiplier() * (1.0 - output);
            }
            compensation = 1.0 / output;
            previousPeak = initialValue;
            compensatedPeak = initialValue;
            detectedPeak = initialValue;
        }

        S follow(const S peak)
        {
            if (peak < previousPeak) {
                compensatedPeak = peak;
                previousPeak = peak;
            }
            else if (peak > previousPeak) {
                compensatedPeak = detectedPeak + compensation * (peak - detectedPeak);
                previousPeak = peak;
            }
            detectedPeak += coefficients.inputMultiplier() * (compensatedPeak - detectedPeak);
            return detectedPeak;
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
