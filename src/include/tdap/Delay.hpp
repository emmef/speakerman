/*
 * tdap/Delay.hpp
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

#ifndef TDAP_DELAY_HEADER_GUARD
#define TDAP_DELAY_HEADER_GUARD

#include <tdap/Array.hpp>
#include <tdap/Power2.hpp>

namespace tdap {
using namespace std;

template <typename S> class Delay {
  static_assert(is_scalar<S>::value, "Expected scalar type parameter");

  Array<S> buffer_;
  size_t read_;
  size_t write_;
  size_t delay_;

  static size_t validMaxDelay(size_t maxDelay) {
    if (maxDelay > 1 && maxDelay < Count<S>::max() / 2) {
      return maxDelay;
    }
    throw std::invalid_argument(
        "Maximum delay must be positive and have next larger power of two");
  }

  size_t validDelay(size_t delay) {
    if (delay <= maxDelay()) {
      return delay;
    }
    throw std::invalid_argument("Delay ");
  }

public:
  Delay(size_t maxDelay)
      : buffer_(1 + validMaxDelay(maxDelay)), read_(0), write_(0), delay_(0) {}

  Delay() : Delay(4000) {}

  [[nodiscard]] size_t maxDelay() const noexcept { return buffer_.size() - 1; }

  [[nodiscard]] size_t delay() const noexcept { return delay_; }

  void setDelay(size_t newDelay) {
    validDelay(newDelay);
    buffer_.zero();
    read_ = 0;
    write_ = delay_;
  }

  void zero() noexcept { buffer_.zero(); }

  [[nodiscard]] S setAndGet(S value) noexcept {
    buffer_[write_++] = value;
    S result = buffer_[read_++];
    if (write_ > delay_) {
      write_ = 0;
    }
    if (read_ > delay_) {
      read_ = 0;
    }
    return result;
  }
};

template <typename S> class MultiChannelDelay {
  size_t maxChannels_, maxDelay_;
  Array<S> buffer_;
  size_t read_, write_, channels_, delay_, end_;

  static size_t getValidMaxChannels(size_t maxChannels, size_t maxDelay) {
    if (maxDelay == 0 || !Count<S>::is_valid_sum(maxDelay, 1)) {
      throw std::runtime_error(
          "MultiChannelDelay::<init> Maximum delay invalid");
    }
    if (Count<S>::product(maxChannels, maxDelay) > 0) {
      return maxChannels;
    }
    throw std::runtime_error("MultiChannelDelay::<init> Combination of maximum "
                             "channels and maximum delay invalid");
  }

  void setMetrics(size_t channels, size_t delay) noexcept {
    buffer_.zero();
    channels_ = channels;
    delay_ = delay;
    read_ = 0;
    write_ = channels_ * delay_;
    end_ = channels_ * (delay_ + 1);
  }

public:
  MultiChannelDelay(size_t maxChannels, size_t maxDelay)
      : maxChannels_(getValidMaxChannels(maxChannels, maxDelay)),
        maxDelay_(maxDelay), buffer_(maxChannels_ * (maxDelay_ + 1)) {
    setMetrics(maxChannels_, 0);
  }

  void zero() noexcept { buffer_.zero(); }

  void setChannels(size_t channels) {
    if (channels == 0 || channels > maxChannels_) {
      throw std::runtime_error(
          "MultiChannelDelay::setChannels invalid number of channels");
    }
    setMetrics(channels, delay_);
  }

  void setDelay(size_t delay) {
    if (delay > maxDelay_) {
      throw std::runtime_error("MultiChannelDelay::setChannels invalid delay");
    }
    setMetrics(channels_, delay);
  }

  [[nodiscard]] size_t getChannels() const noexcept { return channels_; }

  [[nodiscard]] S setAndGet(size_t channel, S value) noexcept {
    IndexPolicy::array(channel, channels_);
    buffer_[write_ + channel] = value;
    return buffer_[read_ + channel];
  }

  void next() noexcept {
    write_ += channels_;
    if (write_ > end_) {
      write_ = 0;
    }
    read_ += channels_;
    if (read_ > end_) {
      read_ = 0;
    }
  }
};

template <typename S> struct MultiChannelAndTimeDelay {
  struct Entry {
    size_t read_, write_, delay_, end_;

    inline void init(size_t channels, size_t channel) noexcept {
      setDelay(channels, channel, 0);
    }

    inline void reset(size_t channels, size_t channel) noexcept {
      setDelay(channels, channel, delay_);
    }

    inline void setDelay(size_t channels, size_t channel, size_t delay) noexcept {
      read_ = channel;
      write_ = read_ + delay * channels;
      end_ = channels * (delay + 1);
      delay_ = delay;
    }

    inline void next(size_t channels) noexcept {
      read_ = (read_ + channels) % end_;
      write_ = (write_ + channels) % end_;
    }

    [[nodiscard]] inline size_t delay() const noexcept { return delay_; }
  };

  size_t maxChannels_, maxDelay_, channels_, delay_;
  Array<S> buffer_;
  Array<Entry> entry_;

  static size_t getValidMaxChannels(size_t maxChannels, size_t maxDelay) {
    if (maxDelay == 0 || !Count<S>::is_valid_sum(maxDelay, 1)) {
      throw std::runtime_error(
          "MultiChannelDelay::<init> Maximum delay invalid");
    }
    if (Count<S>::product(maxChannels, maxDelay) > 0) {
      return maxChannels;
    }
    throw std::runtime_error("MultiChannelDelay::<init> Combination of maximum "
                             "channels and maximum delay invalid");
  }

  void setMetrics(size_t channels) {
    buffer_.zero();
    channels_ = channels;
    entry_.setSize(channels_);
    for (size_t channel; channel < channels_; channel++) {
      entry_[channel].reset(channels_, channel);
    }
  }

public:
  MultiChannelAndTimeDelay(size_t maxChannels, size_t maxDelay)
      : maxChannels_(getValidMaxChannels(maxChannels, maxDelay)),
        maxDelay_(maxDelay), buffer_(maxChannels_ * (maxDelay_ + 1)),
        entry_(maxChannels_) {
    for (size_t channel = 0; channel < maxChannels_; channel++) {
      entry_[channel].init(maxChannels_, channel);
    }
    buffer_.zero();
    channels_ = maxChannels_;
    entry_.setSize(channels_);
  }

  void zero() { buffer_.zero(); }

  void setChannels(size_t channels) {
    if (channels == 0 || channels > maxChannels_) {
      throw std::runtime_error(
          "MultiChannelDelay::setChannels invalid number of channels");
    }
    setMetrics(channels);
  }

  void setDelay(size_t channel, size_t delay) {
    if (delay > maxDelay_) {
      throw std::runtime_error("MultiChannelDelay::setChannels invalid delay");
    }
    entry_[channel].setDelay(channels_, channel, delay);
    delay_ = delay;
  }

  [[nodiscard]] size_t getDelay(size_t channel) const { return entry_[channel].delay(); }

  [[nodiscard]] size_t getChannels() const noexcept { return channels_; }

  [[nodiscard]] S setAndGet(size_t channel, S value) noexcept {
    Entry &entry = entry_[channel];
    buffer_[entry.write_] = value;
    return buffer_[entry.read_];
  }

  void next() noexcept {
    for (size_t channel = 0; channel < channels_; channel++) {
      entry_[channel].next(channels_);
    }
  }
};

} // namespace tdap

#endif /* TDAP_DELAY_HEADER_GUARD */
