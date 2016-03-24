/*
 * Stream.hpp
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

#ifndef SMS_SPEAKERMAN_STREAM_GUARD_H_
#define SMS_SPEAKERMAN_STREAM_GUARD_H_

#include <cstddef>
#include <type_traits>

namespace speakerman
{
	struct stream_result
	{
		static constexpr int END_OF_STREAM = -1;
		static constexpr int INTERRUPTED = END_OF_STREAM - 1;
		static constexpr int INVALID_HANDLE = END_OF_STREAM - 2;
		static constexpr int RESET_BY_PEER = END_OF_STREAM - 3;
		static constexpr int INVALID_ARGUMENT = END_OF_STREAM - 4;
		static constexpr int DATA_TRUNCATED = END_OF_STREAM - 5;
	};

	int last_stream_result();
	void set_stream_result(int result);
	size_t last_operation_count();
	void set_last_operation_count(size_t count);

	class closeable
	{
	public:
		virtual void close() throw() = 0;
		virtual ~closeable() = default;
	};

	class input_stream : public closeable
	{
	public:
		virtual int read() = 0;
		virtual signed long read(void* destination, size_t offs, size_t length);
		virtual signed long read_line(char* destination_line, size_t line_size = 1024);
	};

	signed long read_from_stream(input_stream &stream, void* destination, size_t offs, size_t length);
	signed long read_line_from_stream(input_stream &stream, char* destination_line, size_t line_size);

	class output_stream : public closeable
	{
	public:
		virtual int write(char c) = 0;
		virtual signed long write(const void *source, size_t offs, size_t length);
		virtual signed long write_string(const char*source_string, size_t length = 1024);
	};

	signed long write_to_stream(output_stream & stream, const void* source, size_t offs, size_t length);
	signed long write_string_to_stream(output_stream & stream, const char* source_string, size_t length);

	class buffered_input_stream :
			public input_stream
	{
		size_t size_;
		size_t wr_;
		size_t rd_;
		char *data_;
		input_stream *stream_;
		bool owns_stream_;
	public:
		buffered_input_stream(size_t buffer_size);
		buffered_input_stream(size_t buffer_size, input_stream *stream, bool owns_stream);
		virtual int read() override;
		virtual signed long read(void* destination, size_t offs, size_t length) override;
		virtual void close() throw() override;
		void flush();
		void set_resource(input_stream *stream, bool owns_stream);
		~buffered_input_stream();
	};

	class buffered_output_stream : public output_stream
	{
		size_t size_;
		size_t wr_;
		size_t rd_;
		char *data_;
		output_stream *stream_;
		bool owns_stream_;
		long signed internal_flush();
	public:
		buffered_output_stream(size_t buffer_size, output_stream *stream, bool owns_stream);
		buffered_output_stream(size_t buffer_size);
		virtual int write(char c) override;
		virtual signed long write(const void *source, size_t offs, size_t length) override;
		void flush();
		virtual void close() throw() override;
		void set_resource(output_stream *stream, bool owns_stream);
		~buffered_output_stream();
	};

	class buffer_stream : public output_stream, public input_stream
	{
		char * data_;
		size_t capacity_;
		size_t read_;
		size_t write_;
	protected :
		char* data() const;
	public:
		buffer_stream(size_t capacity);
		virtual int read() override final;
		virtual int write(char c) override final;
		void flush();
		virtual void close() throw() override final;

		virtual ~buffer_stream();
		size_t readable_size() const;
		size_t maximum_size() const;
		size_t allocated_size() const;
	};


} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_STREAM_GUARD_H_ */
