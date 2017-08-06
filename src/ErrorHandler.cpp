/*
 * ErrorHandler.cpp
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

#include <string>
#include <iostream>
#include <jack/jack.h>
#include <speakerman/jack/ErrorHandler.hpp>

namespace speakerman {

    thread_local const char *ErrorHandler::message_ = 0;
    thread_local bool ErrorHandler::force_log_;
    atomic_bool ErrorHandler::callback_installed_;

    void ErrorHandler::error_callback(const char *message)
    {
        message_ = message;
        if (force_log_) {
            force_log_ = false;
            std::cerr << "Forced log: " << message << std::endl;
        }
    }

    void ErrorHandler::clear()
    {
        message_ = nullptr;
    }

    void ErrorHandler::clear_ensure()
    {
        bool v = false;
        if (std::atomic_compare_exchange_strong(&callback_installed_, &v, true)) {
            jack_set_error_function(ErrorHandler::error_callback);
        }
        clear();
    }

    void ErrorHandler::setForceLogNext()
    {
        ErrorHandler::force_log_ = true;
    }

    const char *ErrorHandler::get_message()
    {
        return message_;
    }

    const char *ErrorHandler::get_message_clear()
    {
        const char *result = message_;
        message_ = 0;
        return result;
    }

    void ErrorHandler::checkZeroOrThrow(int value, const char *description)
    {
        if (value == 0) {
            return;
        }
        char ws[30];
        snprintf(ws, 30, "%i", value);
        string message = "[";
        message += ws;
        message += "]";
        const char *error = get_message_clear();
        if (description) {
            message += " ";
            message += description;
            if (error) {
                message += ": ";
                message += error;
            }
        }
        else if (error) {
            message += error;
        }
        else {
            message += "Unspecified error";
        }

        throw std::runtime_error(message);
    }

    bool ErrorHandler::returnIfZero(int value, int *result)
    {
        if (result) {
            *result = value;
        }
        return value == 0;
    }

} /* End of namespace speakerman */
