#ifndef TDAP_M_MEMORY_FENCE_HPP
#define TDAP_M_MEMORY_FENCE_HPP
/*
 * tdap/MemoryFence.hpp
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

#include <atomic>

namespace tdap {

class MemoryFence {
  enum class Action { ENTER, LEAVE };

  static bool innerOrOuter(Action type) {
    static thread_local int level = 0;

    return !(type == Action::ENTER ? level++ : --level);
  }

  const bool forceAcquireRelease_;

public:
  /**
   * Force an explicit acquire memory barrier where all data that was written
   * to main memory by other threads before this barrier, for instance by
   * using a release memory barrier, will become visible for this thread.
   */
  static void acquire() { atomic_thread_fence(std::memory_order_acquire); }

  /**
   * Force an explicit release memory barrier where all data is written to
   * main memory so it becomes visible for other threads when those use a
   * acquire memory barrier.
   */
  static void release() { atomic_thread_fence(std::memory_order_release); }

  /**
   * Creates a memory fence that will use an acquire memory barrier on
   * construction and a release memory barrier on destruction if it is the
   * outermost fence in this thread.
   * <p>
   * If additional barriers are required, the static methods #acquire() and
   * #release() can be used to force them explicitly.
   * </p>
   * @param forceAquireRelease will do the acquire and release even if this
   *     fence is not the outermost one.
   */
  MemoryFence(bool forceAquireRelease = false)
      : forceAcquireRelease_(forceAquireRelease) {
    if (forceAquireRelease || innerOrOuter(Action::ENTER)) {
      acquire();
    }
  }

  ~MemoryFence() {
    if (forceAcquireRelease_ || innerOrOuter(Action::LEAVE)) {
      release();
    }
  }
};

} // namespace tdap

#endif // TDAP_M_MEMORY_FENCE_HPP
