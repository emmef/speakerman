/*
 * tdap/Transport.hpp
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

#ifndef TDAP_TRANSPORT_HEADER_GUARD
#define TDAP_TRANSPORT_HEADER_GUARD

#include <chrono>
#include <mutex>
#include <tdap/MemoryFence.hpp>
#include <thread>
#include <type_traits>

namespace tdap {

using namespace std;

/**
 * Defines a transport that corresponds data between a thread that is
 * required to be lock-free and a thread that is not.
 */
template <typename Data> class Transport {
  static_assert(is_trivially_copyable<Data>::value,
                "Expected Data parameter that is trivial to copy");

  mutex m_;
  volatile bool read_ = false;
  volatile bool write_ = false;
  volatile bool shutdown_ = false;
  Data data_[2];

public:
  class LockFreeData {
    Data *data_;
    bool write_;
    volatile bool *read_;
    bool fenced_;

    LockFreeData(Data &data, const bool write, volatile bool &read,
                 bool useFence)
        : data_(&data) {
      write_ = write;
      read_ = &read;
      fenced_ = useFence;
    }

  public:
    LockFreeData(LockFreeData &&source)
        : data_(source.data_), write_(source.write_), read_(source.read_),
          fenced_(source.fenced_) {
      source.data_ = nullptr;
    }

    static LockFreeData create(Data &data, const bool write,
                               volatile bool &read, bool useFence) {
      if (useFence) {
        MemoryFence::acquire();
      }
      return LockFreeData(data, write, read, useFence);
    }

    bool modified() const { return write_ != *read_; }

    Data &data() { return *data_; }

    ~LockFreeData() {
      if (!data_) {
        return;
      }
      if (modified()) {
        *read_ = write_;
      }
      if (fenced_) {
        MemoryFence::release();
      }
    }
  };

  /**
   * Creates a transport and initialized the data with an original value.
   */
  Transport() {}

  /**
   * Creates a transport and initialized the data with an original value.
   */
  Transport(const Data original, bool startModified) {
    init(original, startModified);
  }

  void init(const Data original, bool startModified) {
    data_[0] = original;
    data_[1] = original;
    if (startModified) {
      read_ = !write_;
    }
  }

  /**
   * Obtain lock-free data, that will bre "released" on destruction.
   * This call makes sure the scope of the returned object is
   * memory fenced, i.e. a memory acquire is done on construction and
   * a release on destruction.
   */
  LockFreeData getLockFree() {
    return LockFreeData::create(write_ ? data_[1] : data_[0], write_, read_,
                                true);
  }

  /**
   * Obtain lock-free data, that will bre "released" on destruction.
   * This call does no acquire-release memory fence and assumes that
   * the call site is already within another memory fence that is
   * relevant for the transport parties.
   */
  LockFreeData getLockFreeNoFence() {
    return LockFreeData::create(write_ ? data_[1] : data_[0], write_, read_,
                                false);
  }

  /**
   * Gets the last completely written version of the data on the
   * non-locking half and sets it to a new value.
   * In order to work correctly, we wait until the last
   * set value is successfully read by the non-locking half or the
   * duration is exceeded.
   * The method returns true if the information was written and false
   * if not (duration exceeded).
   */
  bool getAndSet(Data set, Data &get, chrono::milliseconds duration) {
    const chrono::microseconds sleep = duration / 10;
    const auto expire = chrono::steady_clock::now() + duration;
    unique_lock<mutex> lock(m_);

    while (write_ != read_ && chrono::steady_clock::now() < expire &&
           !shutdown_) {
      this_thread::sleep_for(sleep);
      // mutex only syncs when it is entered, not in-between
      MemoryFence::acquire();
    }
    if (write_ != read_) {
      return false;
    }
    Data &data = write_ ? data_[0] : data_[1];
    get = data;
    data = set;
    write_ = !write_;

    return true;
  }

  void shutdown() {
    MemoryFence fence;
    // volatile only prevents reorder, does not guarantee inter-thread
    // visibility
    shutdown_ = true;
  }
};

} // namespace tdap

#endif /* TDAP_TRANSPORT_HEADER_GUARD */
