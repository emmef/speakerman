/*
 * HttpMessage.hpp
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

#ifndef SMS_SPEAKERMAN_HTTP_MESSAGE_GUARD_H_
#define SMS_SPEAKERMAN_HTTP_MESSAGE_GUARD_H_

#include <atomic>
#include <speakerman/SocketStream.hpp>

namespace speakerman
{
	typedef int (*write_to_stream_function)(char c, void *stream_object);
	typedef int (*read_from_stream_function)(void *stream_object);
	typedef int (*close_stream_function)(void *stream_object);

	struct http_status
	{
		static constexpr unsigned OK = 200;
		static constexpr unsigned PARTIAL_CONTENT = 206;
		static constexpr unsigned BAD_REQUEST = 400;
		static constexpr unsigned NOT_FOUND = 404;
		static constexpr unsigned METHOD_NOT_ALLOWED = 405;
		static constexpr unsigned REQUEST_URI_TOO_LONG = 414;
		static constexpr unsigned INTERNAL_SERVER_ERROR = 500;
		static constexpr unsigned SERVICE_UNAVAILABLE = 503;
		static constexpr unsigned HTTP_VERSION_NOT_SUPPORTED = 505;

		static const char* status_name(unsigned status);
		static bool is_ok(unsigned status);

		static int format_message(output_stream &stream, unsigned status);
		static int format_message(char* buffer, size_t max_length, unsigned status);
		static int format_message_extra(char* buffer, size_t max_length, unsigned status, const char * extraMessage);
		static int format_message_extra(output_stream &stream, unsigned status, const char* extraMessage);
	};

	class http_message
	{
		void read_method();
		void read_url();
		void read_version();
		void handle_header(const char * header, const char *value);
		void read_headers();
		void write_status(socket_stream& stream, unsigned status, const char* additional_message);
		unsigned handle_ok();
		long signed handle_content_prefix(size_t length);
		static size_t valid_buffer_size(size_t size);

		char * read_buffer() { return response_.buffer(); }
		char & read_buffer(size_t i) { return read_buffer()[i]; }
		size_t read_buffer_size() { return response_.allocated_size(); }
		size_t read_buffer_length() { return response_.maximum_size(); }

		int write_header(const char * header, const char *value);
		int write_content_length(size_t value);
		int write_content_type(const char *value);

	protected:
		/**
		 * Invoked when method was read from the request line.
		 *
		 * @param method The request method
		 * @return nullptr on success or a list of allowed methods on failure
		 */
		virtual const char * on_method(const char* method);

		/**
		 * Invoked when URL was read from the request line.
		 *
		 * @param url The request URL
		 * @return nullptr on success or additional message on failure
		 */
		virtual const char * on_url(const char* url);

		/**
		 * Invoked when http-version was read from request line.
		 *
		 * @param version The http version
		 * @return nullptr on success or additional message on failure
		 */
		virtual const char* on_version(const char* version);

		/**
		 * Invoked when a header was read from the request headers.
		 *
		 * @param header header name
		 * @param value value of the header
		 */
		virtual void on_header(const char* header, const char* value);

		/**
		 * If request and its headers have been read, start the hard work
		 * @param stream request and response stream
		 */
		virtual void handle_request() { };

		virtual bool content_stream_delete() const = 0;

		void set_error(unsigned status, const char *additional_message = nullptr);
		void set_success();
		void set_content_type(const char *type);
		void handle_content(size_t contentLength, input_stream *stream);
		void cleanup_content_stream();
		int set_header(const char * header, const char *value);

		output_stream &response() { return response_; }

	public:
		http_message(size_t buffer_size, size_t headers_size) :
			busy_(false),
			response_(buffer_size),
			headers_(headers_size)
		{}

		/**
		 * Handles the request and returns the http status code.
		 * @param stream Stream to use
		 * @return zero or the negative status code, which shall not be -200
		 */
		unsigned handle(socket_stream &stream);

		virtual ~http_message() = default;

	private:
		class response_stream : public buffer_stream
		{
		public:
			response_stream(size_t capacity) : buffer_stream(capacity) {}
			char* buffer() const { return data(); }
		};

		std::atomic_flag busy_;
		socket_stream *stream_;
		response_stream response_;
		buffer_stream headers_;
		size_t content_stream_length_;
		const char * content_type_;
		input_stream *content_stream_;
	};


} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_HTTP_MESSAGE_GUARD_H_ */
