/*
 * tdap/denormal.hpp
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

#ifndef TDAP_DENORMAL_HEADER_GUARD
#define TDAP_DENORMAL_HEADER_GUARD

#include <cinttypes>
#include <cstddef>
#include <limits>
#include <type_traits>

namespace tdap {

namespace helpers_tdap {

	template <typename FPTYPE, size_t selector>
	struct Normalize
	{
		static_assert(std::is_floating_point<FPTYPE>::value, "FPTYPE must be a floating-point type");

		static inline FPTYPE getFlushedToZero(FPTYPE &value) { return value; }

		static inline void flushToZero(FPTYPE value) { }

		static constexpr bool normalizes = false;

		static constexpr size_t bits = 8 * sizeof(FPTYPE);

		static const char * method()
		{
			return "None: IEEE-559 compliant, but denormal definition for this size not known";
		}
	};
	/**
	 * Specialization for non ieee-559/754 floating point formats.
	 */
	template <typename FPTYPE>
	struct Normalize<FPTYPE, 0>
	{
		static inline FPTYPE getFlushedToZero(FPTYPE &value) { return value; }

		static inline void flushToZero(FPTYPE value) { }

		static constexpr bool normalizes = false;

		static constexpr size_t bits = 8 * sizeof(FPTYPE);

		static const char * method()
		{
			return "None: Not IEEE-559 compliant";
		}
	};
	/**
	 * Specialization for 32-bit single-precision floating
	 */
	template <typename FPTYPE>
	struct Normalize<FPTYPE, 4>
	{
		static inline FPTYPE getFlushedToZero(FPTYPE value)
		{
			union {
				FPTYPE f;
				int32_t i;
			} v;

			v.f = value;

			return v.i & 0x7f800000 ? value : 0.0f;
		}
		static inline void flushToZero(FPTYPE &value)
		{
			union {
				FPTYPE f;
				int32_t i;
			} v;

			v.f = value;
			if (v.i & 0x7f800000) {
				return;
			}
			value = 0.0;
		}

		static constexpr bool normalizes = true;

		static constexpr size_t bits = 8 * sizeof(FPTYPE);

		static const char * method()
		{
			return "IEEE-559 32-bit single precision";
		}
	};
	template <typename FPTYPE>
	struct Normalize<FPTYPE, 8>
	{
		static inline FPTYPE getFlushedToZero(FPTYPE &value)
		{
			union {
				FPTYPE f;
				int64_t i;
			} v;

			v.f = value;

			return v.i & 0x7ff0000000000000L ? value : 0;
		}

		static inline void flushToZero(FPTYPE value)
		{
			union {
				FPTYPE f;
				int64_t i;
			} v;

			v.f = value;

			if (v.i & 0x7ff0000000000000L) {
				return;
			}
			value = 0.0;
		}

		static constexpr bool normalizes = false;

		static constexpr size_t bits = 8 * sizeof(FPTYPE);

		static const char * method()
		{
			return "IEEE-559 64-bit double precision";
		}
	};

} /* End of namespace helpers_tdap */

class Denormal
{
	template<typename FPTYPE>
	struct Selector
	{
		static_assert(std::is_floating_point<FPTYPE>::value, "FPTYPE must be a floating-point type");

		static constexpr size_t CLASS_SELECTOR =
				!std::numeric_limits<FPTYPE>::is_iec559 ?
						0 : sizeof(FPTYPE);

		typedef helpers_tdap::Normalize<FPTYPE, CLASS_SELECTOR> Helper;

	};

public:
	template<typename F>
	static inline const char * method()
	{
		return Selector<F>::Helper::method();
	}

	template<typename F>
	static constexpr bool normalizes()
	{
		return Selector<F>::Helper::normalizes;
	}

	template<typename F>
	static constexpr bool bits()
	{
		return Selector<F>::Helper::bits;
	}

	template <typename T>
	static const T& flush(T &v)
	{
		Selector<T>::Helper::flushToZero(v);
		return v;
	}

	template <typename T>
	static const T& get_flushed(const T v)
	{
		return Selector<T>::getFlushedToZero(v);
	}
};

} /* End of name space tdap */

#endif /* TDAP_DENORMAL_HEADER_GUARD */
