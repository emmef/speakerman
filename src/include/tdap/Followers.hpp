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

#include <algorithm>
#include <tdap/Integration.hpp>
#ifdef TDAP_FOLLOWERS_DEBUG_LOGGING
#include <cstdio>
#define TDAP_FOLLOWERS_INFO(...) std::printf(__VA_ARGS__)
#else
static inline int dummyLog(...) { return 0; }
#define TDAP_FOLLOWERS_INFO(...)
#endif

#if TDAP_FOLLOWERS_DEBUG_LOGGING > 1
#define TDAP_FOLLOWERS_DEBUG(...) std::printf(__VA_ARGS__)
#else
#define TDAP_FOLLOWERS_DEBUG(...) dummyLog(__VA_ARGS__)
#endif

#if TDAP_FOLLOWERS_DEBUG_LOGGING > 2
#define TDAP_FOLLOWERS_TRACE(...) std::printf(__VA_ARGS__)
#else
#define TDAP_FOLLOWERS_TRACE(...) dummyLog(__VA_ARGS__)
#endif

namespace tdap {

using namespace std;
/**
 * Follows rises in input and holds those for a number of samples. After that,
 * follows the (lower) input in an integrated fashion.
 */
template <typename S, typename C> class HoldMaxRelease {
  static_assert(is_arithmetic<S>::value, "Sample type S must be arithmetic");

  size_t holdSamples;
  size_t toHold;
  S holdValue;
  IntegratorFilter<C> integrator_;

public:
  HoldMaxRelease(size_t holdForSamples, C integrationSamples,
                 S initialHoldValue = 0)
      : holdSamples(holdForSamples), toHold(0), holdValue(initialHoldValue),
        integrator_(integrationSamples) {}

  void resetHold() { toHold = 0; }

  void setHoldCount(size_t newCount) {
    holdSamples = newCount;
    if (toHold > holdSamples) {
      toHold = holdSamples;
    }
  }

  S apply(S input) {
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

  IntegratorFilter<C> &integrator() { return integrator_; }
};

template <typename S, typename C> class HoldMaxIntegrated {
  static_assert(is_arithmetic<S>::value, "Sample type S must be arithmetic");
  HoldMax<S> holdMax;
  IntegratorFilter<C> integrator_;

public:
  HoldMaxIntegrated(size_t holdForSamples, C integrationSamples,
                    S initialHoldValue = 0)
      : holdMax(holdForSamples, initialHoldValue),
        integrator_(integrationSamples, initialHoldValue) {}

  void resetHold() { holdMax.resetHold(); }

  void setHoldCount(size_t newCount) { holdMax.setHoldCount(newCount); }

  S apply(S input) { return integrator_.integrate(holdMax.apply(input)); }

  IntegratorFilter<C> &integrator() { return integrator_; }
};

template <typename S> class HoldMaxDoubleIntegrated {
  static_assert(is_arithmetic<S>::value, "Sample type S must be arithmetic");
  HoldMax<S> holdMax;
  IntegrationCoefficients<S> coeffs;
  S i1, i2;

public:
  HoldMaxDoubleIntegrated(size_t holdForSamples, S integrationSamples,
                          S initialHoldValue = 0)
      : holdMax(holdForSamples, initialHoldValue), coeffs(integrationSamples),
        i1(initialHoldValue), i2(initialHoldValue) {}

  HoldMaxDoubleIntegrated() : HoldMaxDoubleIntegrated(15, 10, 1.0) {}

  void resetHold() { holdMax.resetHold(); }

  void setMetrics(double integrationSamples, size_t holdCount) {
    holdMax.setHoldCount(holdCount);
    coeffs.setCharacteristicSamples(integrationSamples);
  }

  S apply(S input) {
    return coeffs.integrate(coeffs.integrate(holdMax.apply(input), i1), i2);
  }

  S setValue(S x) { i1 = i2 = x; }

  S applyWithMinimum(S input, S minimum) {
    return coeffs.integrate(
        coeffs.integrate(holdMax.apply(Values::max(input, minimum)), i1), i2);
  }
};

template <typename C> class HoldMaxAttackRelease {
  static_assert(is_arithmetic<C>::value, "Sample type S must be arithmetic");
  HoldMax<C> holdMax;
  AttackReleaseFilter<C> integrator_;

public:
  HoldMaxAttackRelease(size_t holdForSamples, C attackSamples, C releaseSamples,
                       C initialHoldValue = 0)
      : holdMax(holdForSamples, initialHoldValue),
        integrator_(attackSamples, releaseSamples, initialHoldValue) {}

  void resetHold() { holdMax.resetHold(); }

  void setHoldCount(size_t newCount) { holdMax.setHoldCount(newCount); }

  C apply(C input) { return integrator_.integrate(holdMax.apply(input)); }

  AttackReleaseFilter<C> &integrator() { return integrator_; }
};

template <typename T>
class FastSmoothHoldFollower {
  IntegrationCoefficients<T> attack_;
  IntegrationCoefficients<T> release_;
  T releaseInt1_ = 1;
  T releaseInt2_ = 1;
  T attackInt1_ = 1;
  T attackInt2_ = 1;
  T attackInt3_ = 1;
  T attackInt4_ = 1;
  T overshoot_ = 1.5;
  T holdPeak_ = 1;
  T threshold_ = 1;
  size_t prediction_ = 1;
  size_t count_ = 0;

  T calculateOverShoot(size_t predictionSamples) {
    T m1, m2, m3, m4;
    m1 = m2 = m3 = m4 = 0;
    for (size_t s = 0; s < predictionSamples; s++) {
      attack_.integrate(1.0, m1);
      attack_.integrate(m1, m2);
      attack_.integrate(m2, m3);
      attack_.integrate(m3, m4);
    }
    return 1.0 / m4;
  }

public:
  void setPredictionAndThreshold(T predictionSeconds, T threshold,
                                 T sampleRate, T releaseSeconds, T initialValue = -1)  {
    T initValue = std::clamp(initialValue, threshold, threshold *  100);
    threshold_ = threshold;
    releaseInt1_ = initValue;
    releaseInt2_ = initValue;
    attackInt1_ = initValue;
    attackInt2_ = initValue;
    attackInt3_ = initValue;
    attackInt4_ = initValue;
    holdPeak_ = initValue;
    prediction_ = 0.5 + predictionSeconds * sampleRate;
    attack_.setCharacteristicSamples(std::max(prediction_ / 6, 8lu));
    overshoot_ = calculateOverShoot(prediction_);
    release_.setCharacteristicSamples(sampleRate * std::clamp(releaseSeconds, 0.001, 0.1));
    count_ = 0;
  }

  size_t latency() const noexcept { return prediction_; }

  T threshold() const noexcept { return threshold_; }

  T getDetection(T sample) noexcept  {
    T limitValue = std::max(threshold_, sample);
    if (limitValue > holdPeak_) {
      holdPeak_ = limitValue;
      count_ = prediction_;
    }
    else if (count_ > 0) {
      count_--;
    }
    else {
      holdPeak_ = limitValue;
    }
    T correctedValue = threshold_ + (holdPeak_ - threshold_) * overshoot_;
    if (correctedValue > releaseInt2_) {
      releaseInt2_ = releaseInt1_ = correctedValue;
    }
    else {
      release_.integrate(correctedValue, releaseInt1_);
      release_.integrate(releaseInt1_, releaseInt2_);
    }
    attack_.integrate(releaseInt2_, attackInt1_);
    attack_.integrate(attackInt1_, attackInt2_);
    attack_.integrate(attackInt2_, attackInt3_);
    attack_.integrate(attackInt3_, attackInt4_);

    return attackInt4_;
  }

  T getGain(T sample) noexcept { return threshold() / getDetection(sample); }
};


template <typename C> class SmoothHoldMaxAttackRelease {
  static_assert(is_arithmetic<C>::value, "Sample type S must be arithmetic");
  HoldMax<C> holdMax;
  AttackReleaseSmoothFilter<C> integrator_;

public:
  SmoothHoldMaxAttackRelease(size_t holdForSamples, C attackSamples,
                             C releaseSamples, C initialHoldValue = 0)
      : holdMax(holdForSamples, initialHoldValue),
        integrator_(attackSamples, releaseSamples, initialHoldValue) {}

  SmoothHoldMaxAttackRelease() = default;

  void resetHold() { holdMax.reset(); }

  void setHoldCount(size_t newCount) { holdMax.hold_count_ = newCount; }

  C apply(C input) {
    return integrator_.integrate(holdMax.get_value(input, integrator_.output));
  }

  void setValue(C x) { integrator_.setOutput(x); }

  size_t getHoldSamples() const { return holdMax.hold_count_; }

  AttackReleaseSmoothFilter<C> &integrator() { return integrator_; }
};

template <typename S> class TriangularFollower {
  class Node {
    size_t position_;
    S value_;
    S delta_;

    S projectedValue(ssize_t pointPosition) const {
      return value_ +
             delta_ * (pointPosition - static_cast<ssize_t>(position_));
    }

  public:
    /**
     *Constructs a new node with the exact given values.
     * @param pos The position of the Node
     * @param v The value of the Node
     * @param d The delta before the position of the node
     */
    Node(size_t pos, S v, S d) : position_(pos), value_(v), delta_(d) {}
    Node() : Node(0, 1, 1) {}
    /**
     * Constructs a node from the current situation to the given one.
     *
     * @param currentPosition The position where we want to construct route to
     * node
     * @param currentValue The (detected) value at the current position
     * @param newPosition The position of the Node
     * @param newValue The value at the new position
     */
    Node(size_t currentPosition, S currentValue, size_t newPosition, S newValue)
        : position_(newPosition), value_(newValue),
          delta_((newValue - currentValue) /
                 (1 + newPosition - currentPosition)) {
      TDAP_FOLLOWERS_TRACE("# \t\t\t Node from (%zu, %lf) -> (%zu, %lf)\n",
                           currentPosition, currentValue, newPosition,
                           newValue);
    }
    Node(const Node &from, const Node &to)
        : Node(from.position(), from.value(), to.position(), to.value()) {}
    Node(const Node *from, const Node *to)
        : Node(from->position(), from->value(), to->position(), to->value()) {}

    size_t position() const { return position_; }
    S value() const { return value_; }
    S delta() const { return delta_; }
    /**
     * Projects the value of the point position, using the node properties.
     * @param pointPosition The point position
     * @return the projected value
     */
    inline S project(size_t pointPosition) const {
      TDAP_FOLLOWERS_TRACE("# \t\t ");
      print();
      S result = projectedValue(pointPosition);
      TDAP_FOLLOWERS_TRACE(".project(%zu) = %lf\n", pointPosition, result);
      return result;
    }

    /**
     * Projects the value of this node's position, projected from the
     * other node's properties.
     * @param other The other node.
     * @return the projected value
     */
    inline S projectedFrom(const Node &other) const {
      return projectedFrom(&other);
    }
    inline S projectedFrom(const Node *other) const {
      TDAP_FOLLOWERS_TRACE("# \t\t ");
      print();
      TDAP_FOLLOWERS_TRACE(".projectedFrom(");
      other->print();
      S result = other->projectedValue(position_);
      TDAP_FOLLOWERS_TRACE(")=%lf\n", result);
      return result;
    }
    inline void print() const { print(position_, value_, delta_); }
    static inline void print(size_t position, S value, S delta) {
      TDAP_FOLLOWERS_TRACE("{position=%zu, value=%lf, delta=%lf}", position,
                           value, delta);
    }
    static bool isIligibleShortcut(const Node *from, const Node *earlier,
                                   S &minimumDelta) {
      Node constructed = {earlier, from};
      if (constructed.delta() < minimumDelta ||
          isCloseTo(constructed.delta(), minimumDelta) ||
          (fabs(minimumDelta) < 1e-6 && fabs(constructed.delta() < 1e-6))) {
        minimumDelta = constructed.delta();
        return true;
      }
      return false;
    }

    static bool isCloseTo(S v1, S v2) {
      return fabs(v2 - v1) / (fabs(v2) + fabs(v1)) < 1e-6;
    }

    bool isIligableShortcut(const Node *from, S &minimumDelta) const {
      return isIligibleShortcut(from, this, minimumDelta);
    }
  };

  class NodeManager {
    size_t maxNodes_;
    Node *const node_;
    size_t count_;
    size_t start_;

    size_t validMaxNodes(size_t maxNodes) {
      if (maxNodes < 2) {
        throw invalid_argument("TriangularFollower::NodeManager::<init>: "
                               "Number of nodes must be larger than 2.");
      }
      if (maxNodes > Count<Node>::max()) {
        throw invalid_argument("TriangularFollower::NodeManager::<init>: "
                               "Number of nodes exceeds maximum.");
      }
      return maxNodes;
    }
    inline size_t fromFirstIndex(size_t index) const {
      return start_ + IndexPolicy::array(index, count_);
    }
    inline size_t lastIndex() const { return start_ + count_ - 1; }
    inline size_t fromLastIndex(size_t index) const {
      return lastIndex() - IndexPolicy::array(index, count_);
    }

  public:
    NodeManager(size_t maxNodes)
        : maxNodes_(validMaxNodes(maxNodes)), node_(new Node[maxNodes_]),
          count_(0), start_(0) {}

    inline size_t count() const { return count_; }
    inline bool hasNodes() const { return count_ > 0; }
    inline const Node *first() const {
      return count_ > 0 ? node_ + start_ : nullptr;
    }
    inline const Node *last() const {
      return count_ > 0 ? node_ + start_ + count_ - 1 : nullptr;
    }
    inline const Node *fromFirst(size_t index) const {
      if (count_ > 0) {
        return node_ + fromFirstIndex(index);
      }
      return nullptr;
    }
    inline const Node *fromLast(size_t index) const {
      return count_ > 0 ? node_ + fromLastIndex(index) : nullptr;
    }
    inline const Node *next() {
      if (count_ == 0) {
        throw runtime_error(
            "TriangularFollower::NodeManager::add: No nodes to proceed to");
      }
      if (--count_ == 0) {
        start_ = 0;
      } else {
        start_++;
      }
      return first();
    }
    inline void reset() {
      TDAP_FOLLOWERS_TRACE("# \t TriangularFollower::NodeManager::reset()\n");
      count_ = 0;
      start_ = 0;
    }
    inline void add(size_t at, const Node &source) {
      if (at > count_) {
        throw runtime_error("TriangularFollower::NodeManager::add: Number of "
                            "nodes exceeded: cannot create new one");
      }
      count_ = at;
      if (count_ == 0) {
        start_ = 0;
      }
      add(source);
    }
    inline void add(const Node &source) {
      if (count_ >= maxNodes_) {
        fprintf(stderr,
                "\"TriangularFollower::NodeManager::add: Number of nodes (%zu) "
                "exceeded\n",
                maxNodes_);
        for (size_t i = 0; i < count(); i++) {
          const Node *node = fromFirst(i);
          printf("%5zu {position=%zu, value=%lf, delta=%lf)\n", i,
                 node->position(), node->value(), node->delta());
        }
        throw runtime_error("TriangularFollower::NodeManager::add: Number of "
                            "nodes exceeded: cannot create new one");
      }
      TDAP_FOLLOWERS_TRACE("# \t TriangularFollower::NodeManager::add(%zu, ",
                           count());
      source.print();
      node_[start_ + count_++] = source;
      TDAP_FOLLOWERS_TRACE(") { count=%zu; }\n", count_);
    }
    inline bool isInReleaseOrIdle() const {
      bool result = count_ <= 1;
      TDAP_FOLLOWERS_TRACE("# \t isInReleaseOrIdle() = %i\n", result);
      return result;
    }
    inline bool isBelowReleaseEnvelope(size_t newSamplePtr,
                                       const S newPeakValue) const {
      TDAP_FOLLOWERS_TRACE(
          "# \t isBelowReleaseEnvelope(new-peak-at=%zu, new-peak-value=%lf)\n",
          newSamplePtr, newPeakValue);
      const Node *l = last();
      bool result =
          l != nullptr && newPeakValue <= last()->project(newSamplePtr);
      TDAP_FOLLOWERS_TRACE("# \t isBelowReleaseEnvelope = %i { nodes=%zu; }\n",
                           result, count());
      return result;
    }
    inline bool isBelowPeakAttack(const Node &newPeak, size_t lastIndex) const {
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
    inline ssize_t getLastAbovePeakAttackEnvelope(const Node &newPeak) {
      TDAP_FOLLOWERS_TRACE("# \tgetLastAbovePeakAttackEnvelope(");
      newPeak.print();
      if (count() < 2) {
        TDAP_FOLLOWERS_TRACE(") : first peak\n");
        return -1;
      }
      TDAP_FOLLOWERS_TRACE(") : search\n");
      for (ssize_t index = fromLastIndex(1); index >= (ssize_t)start_;
           index--) {
        const Node *node = node_ + index;
        S nodeValue = node->value();
        TDAP_FOLLOWERS_TRACE("# \t\t (%zu) ", index - start_);
        node->print();
        TDAP_FOLLOWERS_TRACE("\n");
        S projected = newPeak.project(node->position());
        if (nodeValue > projected) {
          TDAP_FOLLOWERS_TRACE("# \t\t FOUND %zu: ", index - start_);
          node->print();
          TDAP_FOLLOWERS_TRACE("\n");
          return index - start_;
        }
      }
      TDAP_FOLLOWERS_TRACE("# \t\t NOT FOUND\n");
      return -1;
    }
    ~NodeManager() {
      if (node_) {
        delete[] node_;
      }
    }
  };

  NodeManager nodes_;
  S threshold_ = 1;
  S detect_ = 0;
  size_t position_ = 0;
  size_t attSamples_ = 1;
  size_t relSamples_ = 1;

  S constructNewFirstPeak(const S value) {
    TDAP_FOLLOWERS_TRACE(
        "# \t constructNewFirstPeak(value=%lf): reached towards node\n", value);
    size_t newPtr = position_ + attSamples_ - 1;
    nodes_.add(0, {position_, detect_, newPtr, value});
    nodes_.add(
        {newPtr + relSamples_, threshold_, (threshold_ - value) / relSamples_});
    return detect_; // continueAsNormal();
  }
  S constructFromNode(const size_t nodePtr, const size_t position,
                      const S value) {
    TDAP_FOLLOWERS_TRACE(
        "# \t constructFromNode(nodePtr=%zu, position=%zu, value=%lf)\n",
        nodePtr, position, value);
    if (nodePtr == 0) {
      return constructNewFirstPeak(value);
    }
    const Node *from = nodes_.fromFirst(nodePtr);

    nodes_.add(nodePtr + 1, {from->position(), from->value(), position, value});
    nodes_.add({position, value, position + relSamples_, threshold_});
    nodes_ = nodePtr + 2;
    return continueAsNormal();
  }

  S continueAsNormal() {
    TDAP_FOLLOWERS_TRACE("# \t continueAsNormal()\n");
    if (!nodes_.hasNodes()) {
      TDAP_FOLLOWERS_TRACE("# \t\t nodes=0\n");
      position_++; // Might want to reset to zero instead
      return threshold_;
    }
    const Node *towards = nodes_.first();
    TDAP_FOLLOWERS_TRACE("# \t\t detect=%lf, pos=%zu, towards=", detect_,
                         position_);
    towards->print();
    TDAP_FOLLOWERS_TRACE("\n");
    detect_ = towards->project(position_);
    if (position_++ >= towards->position()) {
      towards = nodes_.next();
      if (towards == nullptr) {
        TDAP_FOLLOWERS_TRACE("# \t\t\t -- Last node reached: reset\n");
        return detect_;
      }
      TDAP_FOLLOWERS_TRACE("# \t\t\t -- Move to next node ");
      towards->print();
      nodes_.first()->print();
      TDAP_FOLLOWERS_TRACE("\n");
    }
    return detect_;
  }

  inline S followAlgorithm(const S value) {
    if (value < threshold_) {
      TDAP_FOLLOWERS_TRACE("# \t below threshold %lf\n", threshold_);
      return continueAsNormal();
    }
    size_t newPtr = position_ + attSamples_;
    if (nodes_.isBelowReleaseEnvelope(newPtr, value)) {
      return continueAsNormal();
    }
    if (nodes_.count() == 0) {
      nodes_.add(0, {position_ + 1, threshold_, newPtr, value});
      nodes_.add({newPtr + 1, value, newPtr + relSamples_, threshold_});
      return continueAsNormal();
    }
    if (nodes_.count() == 1) {
      nodes_.add(0, {position_, detect_, newPtr, value});
      nodes_.add({newPtr + 1, value, newPtr + relSamples_, threshold_});
      return continueAsNormal();
    }
    S result = continueAsNormal();
    const Node newPeak = {position_, threshold_, newPtr, value};
    const Node newRelease = {newPtr + 1, value, newPtr + relSamples_,
                             threshold_};
    ssize_t higherNode = nodes_.getLastAbovePeakAttackEnvelope(newPeak);
    if (higherNode < 0) {
      TDAP_FOLLOWERS_TRACE("# \t\t Recreate\n");
      nodes_.add(0, {position_, detect_, newPtr, value});
      nodes_.add(newRelease);
    } else {
      const Node *from = nodes_.fromFirst(higherNode);
      TDAP_FOLLOWERS_TRACE("# \t\t Add from existing: ");
      from->print();
      TDAP_FOLLOWERS_TRACE("\n");
      Node backProjected{newPeak.position(), newPeak.value(),
                         (newPeak.value() - from->value()) /
                             (newPeak.position() - from->position())};
      // Merge additional previous peaks if the delta to reach them stays equal
      // or keeps falling (negative time)
      ssize_t mergeNode = -1;
      ssize_t shortcutNode = higherNode - 1;
      S minimumDelta = backProjected.delta();
      while (shortcutNode > 0 &&
             nodes_.fromFirst(shortcutNode)
                 ->isIligableShortcut(&backProjected, minimumDelta)) {
        mergeNode = shortcutNode;
        shortcutNode--;
      }
      if (mergeNode >= 0 && mergeNode != shortcutNode) {
        from = nodes_.fromFirst(mergeNode);
        TDAP_FOLLOWERS_TRACE("# Ditching %zu peaks until peak %zu ",
                             higherNode - mergeNode, mergeNode);
        from->print();
        TDAP_FOLLOWERS_TRACE("\n");
        backProjected = {newPeak.position(), newPeak.value(),
                         (newPeak.value() - from->value()) /
                             (newPeak.position() - from->position())};
        higherNode = mergeNode;
      }
      nodes_.add(higherNode + 1, backProjected);
      //                nodes_.add(higherNode + 1, {newPeak.position(),
      //                newPeak.value(), (newPeak.value() - from->value())/
      //                (newPeak.position() - from->position())});
      nodes_.add(newRelease);
    }
    return result;
  }

public:
  TriangularFollower(size_t maxNodes) : nodes_(maxNodes) {
    TDAP_FOLLOWERS_DEBUG("# TriangularFollower(%zu)\n", maxNodes);
  }

  ~TriangularFollower() {}

  inline S follow(const S value) {
    TDAP_FOLLOWERS_TRACE("### follow(%zu = %lf) // nodes=%zu\n", position_,
                         value, nodes_.count());
    S result = followAlgorithm(value);
    TDAP_FOLLOWERS_TRACE("#   follow(%zu = %lf) = %lf\n", position_, value,
                         result);
    return result;
  }

  void setTimeConstantAndSamples(size_t attackSamples, size_t releaseSamples,
                                 S threshold) {
    attSamples_ = Sizes::valid_positive(attackSamples);
    relSamples_ = Sizes::valid_positive(releaseSamples);
    threshold_ = threshold;
    detect_ = threshold_;
    nodes_.reset();
    position_ = 0;
  }
};

template <typename S> class CompensatedAttack {
  S compensation = 1;
  S previousPeak = 0;
  S compensatedPeak = 0;
  S detectedPeak = 0;
  IntegrationCoefficients<S> coefficients;

public:
  void setTimeConstantAndSamples(size_t timeConstantSamples, size_t samples,
                                 S initialValue) {
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

  S follow(const S peak) {
    if (peak < previousPeak) {
      compensatedPeak = peak;
      previousPeak = peak;
    } else if (peak > previousPeak) {
      compensatedPeak = detectedPeak + compensation * (peak - detectedPeak);
      previousPeak = peak;
    }
    detectedPeak +=
        coefficients.inputMultiplier() * (compensatedPeak - detectedPeak);
    return detectedPeak;
  }
};

} // namespace tdap

#endif /* TDAP_FOLLOWERS_HEADER_GUARD */
