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

namespace speakerman
{
	static constexpr size_t STREAM_BUFFER_SIZE = 1024;
	static constexpr int INTERRUPTED = EOF - 1;
	static constexpr int NOFILE = EOF - 2;
	static constexpr int RESET_BY_PEER = EOF - 3;
	static constexpr int INVALID_ARGUMENT = EOF - 4;

	class socket_input_stream
	{
		int fd_;
		char buffer_[STREAM_BUFFER_SIZE];
		int pos_;
		int mark_;
		bool blocking_;

		int unsafe_read();

	public:
		socket_input_stream(int fd);
		socket_input_stream();

		bool canReadFromBuffer() const;
		int read();
		void set_file(int fd);
		int readLine(char *buffer, size_t size);
		~socket_input_stream();
	};

	class socket_stream;
	class socket_output_stream
	{
		int fd_;
		char buffer_[STREAM_BUFFER_SIZE];
		int pos_;
		int mark_;
		friend class socket_stream;

		int unsafe_send(size_t offset, size_t count, bool do_throw);
		int default_flush(bool do_throw);
		int unsafe_flush(bool do_throw);
		int unsafe_write(char c);
		void unsafe_set_file(int fd);

	public:
		socket_output_stream(int fd);
		socket_output_stream();

		bool canWriteToBuffer() const;
		int write(char c);
		int write_string(const char *str, size_t max_len, size_t *written);
		int flush();
		int set_file_check_flush(int fd, int *flushResult);
		int set_file(int fd);
		~socket_output_stream();
	};

	class socket_stream
	{
		socket_input_stream istream;
		socket_output_stream ostream;
		int fd_;

	public:
		socket_stream() {}
		socket_stream(int fd) { set_file(fd); }

		int set_file(int fd);

		int read() { return istream.read(); }
		int read_line(char *line, size_t max_size) { return istream.readLine(line, max_size); }
		bool canReadFromBuffer() const { istream.canReadFromBuffer(); }

		int flush() { return ostream.flush(); }
		int write(char c) { return ostream.write(c); }
		int write_string(const char *str, size_t max_len, size_t *written) { return ostream.write_string(str, max_len, written); }
		bool canWriteToBuffer() const { return ostream.canWriteToBuffer(); }

		~socket_stream();
	};
} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_SOCKET_STREAM_GUARD_H_ */
