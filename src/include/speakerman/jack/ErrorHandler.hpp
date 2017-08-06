/*
 * ErrorHandler.hpp
 *
 * Part of 'Speaker management system'
 *
 * Copyright (C) 2013-2014 Michel Fleur.
 * https://github.com/emmef/simpledsp
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

#ifndef SMS_SPEAKERMAN_ERROR_HANDLER_GUARD_H_
#define SMS_SPEAKERMAN_ERROR_HANDLER_GUARD_H_

#include <atomic>
#include <stdexcept>

#include <jack/types.h>


namespace speakerman {

    using namespace std;

    class ErrorHandler
    {
        static thread_local const char *message_;
        static thread_local bool force_log_;
        static atomic_bool callback_installed_;

        static void error_callback(const char *message);


    public:
        static void clear();

        static void clear_ensure();

        static void setForceLogNext();

        static const char *get_message();

        static const char *get_message_clear();

        /**
         * Checks if the value is zero and throws runtime_error otherwise.
         * The format of the error is one of
         *   [value] Unspecified error
         *   [value] Description
         *   [value] jack_message
         *   [value] Description: jack_message
         *
         * @param value The value to check
         * @param description Additional description
         */
        static void checkZeroOrThrow(int value, const char *description);

        /**
         * Throws runtime_exception if the pointer ptr is nullptr.
         * @param ptr The pointer
         * @param description Additional message in error
         * @return the non-nullptr pointer
         */
        template<typename T>
        static T *checkNotNullOrThrow(T *const ptr, const char *description)
        {
            if (ptr) {
                return ptr;
            }
            const char *error = get_message_clear();
            if (error) {
                string message = description ? description : "Error";
                message += ": ";
                message += error;
                throw std::runtime_error(message);
            }
            else if (description) {
                throw std::runtime_error(description);
            }
            else {
                throw std::runtime_error("Jack error");
            }
        }

        /**
         * Throws runtime_exception if the pointer ptr is nullptr.
         * @param ptr The pointer
         * @param description Additional message in error
         * @return the non-nullptr pointer
         */
        template<typename T>
        bool returnIfZero(int value, int *result)
        {
            if (result) {
                *result = value;
            }
            return value == 0;
        }

        /**
         * Returns whether the value is zero.
         * @param value The value to check
         * @param result Stores the value if it is not nullptr.
         * @return true if the value is zero.
         */
        static bool returnIfZero(int value, int *result);
    };


} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_ERROR_HANDLER_GUARD_H_ */

