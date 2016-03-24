/*
 * SocketStream.hpp
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

#ifndef SMS_SPEAKERMAN_SOCKET_STREAM_GUARD_H_
#define SMS_SPEAKERMAN_SOCKET_STREAM_GUARD_H_

#include <limits>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <speakerman/Stream.hpp>

namespace speakerman
{
	static constexpr size_t STREAM_BUFFER_SIZE = 128;

	class raw_socket_input_stream : public input_stream
	{
		bool owns_descriptor_ = false;
		int file_descriptor_ = -1;
		bool blocking_ = true;
	public:
		raw_socket_input_stream();
		raw_socket_input_stream(int file_descriptor, bool owns_descriptor);
		virtual int read() override;
		virtual signed long read(void* destination, size_t offs, size_t length) override;
		virtual void close() throw() override;
		void set_bocking(bool value);
		bool get_bocking() const;
		void set_file_descriptor(int file_descriptor, bool owns_descriptor);
		virtual ~raw_socket_input_stream();
	};

	class socket_input_stream : public input_stream
	{
		raw_socket_input_stream stream_;
		buffered_input_stream buffered_;

	public:
		socket_input_stream(size_t buffer_size, int file_descriptor, bool owns_descriptor);
		socket_input_stream(size_t buffer_size);
		virtual int read() override final
		{
			return buffered_.read();
		}
		virtual signed long read(void* destination, size_t offs, size_t length) override final
		{
			return buffered_.read(destination, offs, length);
		}
		virtual void close() throw() override;
		void set_file_descriptor(int file_descriptor, bool owns_descriptor);
		void set_bocking(bool value);
		bool get_bocking();
		void flush();
		~socket_input_stream();
	};

	class raw_socket_output_stream : public output_stream
	{
		bool owns_descriptor_ = false;
		int file_descriptor_ = -1;
		bool blocking_ = true;
	public:
		raw_socket_output_stream();
		raw_socket_output_stream(int file_descriptor, bool owns_descriptor);
		virtual int write(char c) override final;
		virtual signed long write(const void *source, size_t offs, size_t length) override final;
		void set_file_descriptor(int file_descriptor, bool owns_descriptor);
		void set_bocking(bool value);
		bool get_bocking() const;
		virtual void close() throw() override;
		virtual ~raw_socket_output_stream();
	};

	class socket_output_stream : public output_stream
	{
		raw_socket_output_stream stream_;
		buffered_output_stream buffered_;

	public:
		socket_output_stream(size_t buffer_size, int file_descriptor, bool owns_descriptor);
		socket_output_stream(size_t buffer_size);
		virtual int write(char c) override final
		{
			return buffered_.write(c);
		}
		virtual signed long write(const void* source, size_t offs, size_t length) override final
		{
			return buffered_.write(source, offs, length);
		}
		void set_file_descriptor(int file_descriptor, bool owns_descriptor);
		void set_bocking(bool value);
		bool get_bocking();
		void flush();
		virtual void close() throw() override;
		~socket_output_stream();
	};


	class socket_stream : public input_stream, public output_stream
	{
		raw_socket_input_stream istream_;
		raw_socket_output_stream ostream_;
		buffered_input_stream ibuffered_;
		buffered_output_stream obuffered_;

	public:
		socket_stream(size_t read_buffer_size, size_t write_buffer_size, int file_descriptor, bool owns_descriptor);
		socket_stream(size_t read_buffer_size, size_t write_buffer_size);
		socket_stream(size_t buffer_size);

		virtual int read() override final
		{
			return ibuffered_.read();
		}
		virtual signed long read(void* destination, size_t offs, size_t length) override final
		{
			return ibuffered_.read(destination, offs, length);
		}
		virtual int write(char c) override final
		{
			return obuffered_.write(c);
		}
		virtual signed long write(const void* source, size_t offs, size_t length) override final
		{
			return obuffered_.write(source, offs, length);
		}
		virtual void close() throw() override;
		void set_file_descriptor(int file_descriptor, bool owns_descriptor);
		void set_read_bocking(bool value);
		bool get_read_bocking();
		void set_write_bocking(bool value);
		bool get_write_bocking();
		void flush();
		~socket_stream();
	};


} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_SOCKET_STREAM_GUARD_H_ */
