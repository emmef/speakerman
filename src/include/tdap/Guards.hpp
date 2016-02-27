/*
 * tdap/Guards.hpp
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

#ifndef TDAP_GUARDS_HEADER_GUARD
#define TDAP_GUARDS_HEADER_GUARD

#include <atomic>
#include <mutex>
#include <type_traits>
#include <tdap/Value.hpp>

namespace tdap {

template<typename T>
static constexpr bool isMutexType()
{
	return
			std::is_same<std::mutex, T>::value ||
			std::is_same<std::timed_mutex, T>::value ||
			std::is_same<std::recursive_mutex, T>::value ||
			std::is_same<std::recursive_timed_mutex, T>::value;
}


template<typename M>
class Guard
{
	static_assert(isMutexType<M>(), "Expected mutex type parameter M");

	M &mutex_;
public:
	Guard(M &mutex) : mutex_(mutex)
	{
		mutex_.lock();
	}
	~Guard()
	{
		mutex_.unlock();
	}
};

template<typename M, typename S>
class ExpectedStateGuard
{
	static_assert(TriviallyCopyable<S>::value, "Expected type S that is trivial to copy");

	Guard<M> guard;
	S &actual_;
	bool setOnExit_;
	S exitState_;

public:
	ExpectedStateGuard(M &mutex, S expectedState, S &actualState) :
		guard(mutex), actual_(actualState), setOnExit_(false)
	{
		if (actualState != expectedState) {
			throw std::runtime_error("ExpectStateGuard: not in expected state");
		}
	}
	void setOnExit(S value)
	{
		setOnExit_ = true;
		exitState_ = value;
	}
	void setActual(S value)
	{
		actual_ = value;
	}
	~ExpectedStateGuard()
	{
		if (setOnExit_) {
			actual_ = exitState_;
			setOnExit_ = false;
		}
	}
};

class TryEnter
{
	std::atomic_flag &flag_;
	bool enter_;
public:
	TryEnter(std::atomic_flag &flag) : flag_(flag)
	{
		enter_ = !flag_.test_and_set();
	}
	bool entered() const
	{
		return enter_;
	}
	void failOnNotEntered() const
	{
		if (!enter_) {
			throw std::runtime_error("Busy");
		}
	}
	~TryEnter()
	{
		if (enter_) {
			flag_.clear();
		}
	}
};

class SpinlockEnter
{
	std::atomic_flag &flag_;
public:
	SpinlockEnter(std::atomic_flag &flag) : flag_(flag)
	{
		while(flag_.test_and_set());
	}
	~SpinlockEnter()
	{
		flag_.clear();
	}
};

struct Guards
{
	template<typename M>
	static Guard<M> guard(M &mutex)
	{
		return Guard<M>(mutex);
	}
	template<typename M, typename S>
	static ExpectedStateGuard<M, S> create(M &mutex, S expected, S &actual)
	{
		return ExpectedStateGuard<M, S>(mutex, expected, actual);
	}
};


} /* End of name space tdap */

#endif /* TDAP_GUARDS_HEADER_GUARD */

