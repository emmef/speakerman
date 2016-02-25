/*
 * Names.hpp
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

#ifndef SMS_SPEAKERMAN_NAMES_GUARD_H_
#define SMS_SPEAKERMAN_NAMES_GUARD_H_

#include <regex>
#include <mutex>
#include <stdexcept>

#include <jack/jack.h>
#include <tdap/Array.hpp>

namespace speakerman {

using namespace std;
using namespace tdap;

class Names
{
	static void checkLengthOrThrow(size_t bufferSize, int result)
	{
		if (result > 0 && (size_t)result < bufferSize) {
			return;
		}
		throw std::logic_error("Jack port/client name regular expression initialization error");
	}

	static constexpr size_t MAX_SIZE_LENGTH = 20;

	static constexpr size_t minimum_name_length = 2;

	static const size_t client_port_separator_length() { return strlen(client_port_separator()); }

	static const char * template_name_regex() { return "[-_\\.,0-9a-zA-Z ]{%zu,%zu}"; }

	static const size_t template_name_regex_length() { return strlen(template_name_regex()); }


	static const size_t pattern_max_length()
	{
		static const size_t value =
				2 + // Begin and end markers ^$
				2 * template_name_regex_length() + // basic tempates without length
				client_port_separator_length() + // client/port separator
				2 * MAX_SIZE_LENGTH;
		return value;
	}

	static const size_t pattern_max_buffer_size() { return pattern_max_length() + 1; }

	static string get_name_pattern(size_t clientLength, size_t portLength)
	{
		static const int SIZE = pattern_max_buffer_size();

		size_t formatTemplateLength = 2 * template_name_regex_length() + client_port_separator_length();

		char formatTemplate[SIZE];
		char pattern[SIZE];

		if (clientLength > 0 && portLength > 0) {
			checkLengthOrThrow(SIZE, snprintf(formatTemplate, SIZE,
					"^%s%s%s$", template_name_regex(), client_port_separator(), template_name_regex()));
		}
		else {
			checkLengthOrThrow(SIZE, snprintf(formatTemplate, SIZE, "^%s$", template_name_regex()));
		}

		if (clientLength > 0) {
			if (portLength > 0) {
				checkLengthOrThrow(SIZE, snprintf(pattern, SIZE,
						formatTemplate, minimum_name_length, clientLength, minimum_name_length, portLength));
			}
			else {
				checkLengthOrThrow(SIZE, snprintf(pattern, SIZE,
						formatTemplate, minimum_name_length, clientLength));
			}
		}
		else if (portLength > 0) {
			checkLengthOrThrow(SIZE, snprintf(pattern, SIZE,
					formatTemplate, minimum_name_length, portLength));
		}
		else {
			snprintf(pattern, SIZE, "^$");
		}

		return pattern;
	}

	static const char * valid_name(const regex &regex, const char * name, const char * description)
	{
		if (regex_match(name, regex)) {
			return name;
		}
		string message = "Invalid name (";
		message += description;
		message += "): '";
		message += name;
		message += "'";

		throw std::invalid_argument(message);
	}

public:
	static const char * const client_port_separator() { return ":"; }

	static size_t get_full_size()
	{
		static const size_t value = jack_port_name_size();
		return value;
	}

	static size_t get_client_size()
	{
		static const size_t value = jack_client_name_size();
		return value;
	}

	static size_t get_port_size()
	{
		static const size_t value =
				get_full_size() -
				get_client_size() -
				client_port_separator_length();
		return value;
	}

	static const string get_port_pattern()
	{
		static const string pattern = get_name_pattern(0, get_port_size());

		return pattern;
	}

	static const string get_client_pattern()
	{
		static const string pattern = get_name_pattern(get_client_size(), 0);

		return pattern;
	}

	static const string get_full_pattern()
	{
		static const string pattern = get_name_pattern(get_client_size(), get_port_size());

		return pattern;
	}

	static const regex &get_port_regex()
	{
		static const regex NAME_REGEX(get_port_pattern());

		return NAME_REGEX;
	}

	static const regex &get_client_regex()
	{
		static const regex NAME_REGEX(get_client_pattern());

		return NAME_REGEX;
	}

	static const regex &get_full_regex()
	{
		static const regex NAME_REGEX(get_full_pattern());

		return NAME_REGEX;
	}

	static bool is_valid_port(const char *unchecked)
	{
		return std::regex_match(unchecked, get_port_regex());
	}

	static bool is_valid_port_full(const char *unchecked)
	{
		return std::regex_match(unchecked, get_port_regex());
	}

	static bool is_valid_client(const char *unchecked)
	{
		return std::regex_match(unchecked, get_port_regex());
	}

	static const char * valid_port(const char *unchecked)
	{
		return valid_name(get_port_regex(), unchecked, "port");
	}

	static const char * valid_port_full(const char *unchecked)
	{
		return valid_name(get_full_regex(), unchecked, "full port");
	}

	static const char * valid_client(const char *unchecked)
	{
		return valid_name(get_client_regex(), unchecked, "port");
	}

	static const string &valid_port(const string &unchecked)
	{
		valid_name(get_port_regex(), unchecked.c_str(), "port");
		return unchecked;
	}

	static const string &valid_port_full(const string &unchecked)
	{
		valid_name(get_full_regex(), unchecked.c_str(), "full port");
		return unchecked;
	}

	static const string &valid_client(const string &unchecked)
	{
		valid_name(get_client_regex(), unchecked.c_str(), "port");
		return unchecked;
	}

	static string &valid_port(string &unchecked)
	{
		valid_name(get_port_regex(), unchecked.c_str(), "port");
		return unchecked;
	}

	static string &valid_port_full(string &unchecked)
	{
		valid_name(get_full_regex(), unchecked.c_str(), "full port");
		return unchecked;
	}

	static string &valid_client(string &unchecked)
	{
		valid_name(get_client_regex(), unchecked.c_str(), "port");
		return unchecked;
	}
};


class PortNames
{
	const char** const portNames_;
	const size_t count_;

	size_t rangeCheck(size_t index) const {
		if (index < count_) {
			return index;
		}
		throw out_of_range("Port name index out of range");
	}

	static size_t countPorts(const char ** portNames)
	{
		size_t count = 0;
		if (portNames != nullptr) {
			const char** name = portNames;
			while (name[count] != nullptr) {
				count++;
			}
		}
		return count;
	}

public:
	PortNames(jack_client_t* client, const char* namePattern,
			const char* typePattern, unsigned long flags) :
			portNames_(jack_get_ports(client, namePattern, typePattern, flags)),
			count_(countPorts(portNames_))
	{
	}

	size_t count() const {
		return count_;
	}

	const char* get(size_t idx) const {
		return portNames_[rangeCheck(idx)];
	}

	const char* operator [](size_t idx) const {
		return get(idx);
	}

	~PortNames() {
		if (portNames_ != nullptr) {
			jack_free(portNames_);
		}
	}
};


} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_NAMES_GUARD_H_ */

