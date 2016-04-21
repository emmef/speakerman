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
	static constexpr size_t STREAM_BUFFER_SIZE = 1024;

	class socket_input_stream : public file_owner, input_stream
	{
		char buffer_[STREAM_BUFFER_SIZE];
		int pos_;
		int mark_;
		bool blocking_;

		int unsafe_read();
	protected:
		virtual void close_file() override;
		virtual void before_close_file() override;
		virtual void on_file_set() override;

	public:
		socket_input_stream(int file_descriptor, bool owns_file);
		socket_input_stream();

		void set_bocking(bool value);
		bool get_bocking();
		bool canReadFromBuffer() const;
		virtual int read() override;
		virtual void close() override;
		~socket_input_stream();
	};

	class socket_stream;
	class socket_output_stream : public file_owner, output_stream
	{
		char buffer_[STREAM_BUFFER_SIZE];
		size_t pos_;
		friend class socket_stream;

		int unsafe_send(size_t offset, size_t count, bool do_throw);
		int unsafe_flush(bool do_throw);
		int unsafe_write(char c);
		void unsafe_set_file(int fd);
	protected:
		virtual void close_file() override;
		virtual void before_close_file() override;
		virtual void on_file_set() override;

	public:
		socket_output_stream(int fd, bool owns_file);
		socket_output_stream();

		bool canWriteToBuffer() const;
		virtual int write(char c) override;
		int write_string(const char *str, size_t max_len, size_t *written);
		virtual void flush() override;
		virtual void close() override;
		~socket_output_stream();
	};

	class socket_stream : public input_stream, public output_stream
	{
		socket_input_stream istream;
		socket_output_stream ostream;

	public:
		socket_stream(int fd, bool owns_file) { set_file(fd, owns_file); }
		socket_stream() : socket_stream(-1, false) {}

		virtual int read() { return istream.read(); }
		virtual void close() {	istream.close(); ostream.close(); }
		virtual int write(char c) { return ostream.write(c); }
		virtual void flush() { ostream.flush(); }

		int set_file(int fd, bool owns_file)
		{
			istream.set_file(fd, false);
			ostream.set_file(fd, owns_file);
			return 0;
		}

		bool canReadFromBuffer() const { return istream.canReadFromBuffer(); }

		bool canWriteToBuffer() const { return ostream.canWriteToBuffer(); }

		virtual ~socket_stream() {

		}
	};
} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_SOCKET_STREAM_GUARD_H_ */
