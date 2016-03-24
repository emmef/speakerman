/*
 * SocketStream.cpp
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

#include <cstring>
#include <cerrno>
#include <stdexcept>
#include <netdb.h>
#include <unistd.h>

#include <iostream>

#include <speakerman/SocketStream.hpp>

namespace speakerman {

	static int terminate_line(char *buffer, size_t pos)
	{
		buffer[pos] = 0;
		return pos;
	}

	int socket_input_stream::unsafe_read()
	{
		if (pos_ < mark_) {
			return buffer_[pos_++];
		}

		int r = blocking_ ?
				recv(file_descriptor(), buffer_, STREAM_BUFFER_SIZE, 0) :
				recv(file_descriptor(), buffer_, STREAM_BUFFER_SIZE, MSG_DONTWAIT);
		if (r < 0) {
			switch (errno) {
			case EAGAIN:
				return EOF;
			case EINTR:
				return stream_result::INTERRUPTED;
			default:
				throw std::runtime_error(strerror(errno));
			}
		}
		else if (r == 0) {
			return EOF;
		}
		else {
			mark_ = r;
			pos_ = 0;
			return buffer_[pos_++];
		}
	}

	socket_input_stream::socket_input_stream(int file_descriptor, bool owns_file) :
			blocking_(true)
	{
		set_file(file_descriptor, owns_file);
	}

	socket_input_stream::socket_input_stream() : socket_input_stream(-1, false)
	{
	}

	void socket_input_stream::set_bocking(bool value)
	{
		blocking_ = value;
	}

	bool socket_input_stream::get_bocking()
	{
		return blocking_;
	}

	bool socket_input_stream::canReadFromBuffer() const
	{
		return file_descriptor() != -1 && pos_ < mark_;
	}

	int socket_input_stream::read()
	{
		if (file_descriptor() == -1) {
			return stream_result::INVALID_HANDLE;
		}
		return unsafe_read();
	}

	void socket_input_stream::close()
	{
		set_file(-1, 0);
	}

	void socket_input_stream::on_file_set()
	{
		pos_ = mark_ = 0;
	}

	void socket_input_stream::close_file()
	{
		::close(file_descriptor());
	}

	void socket_input_stream::before_close_file()
	{
	}


	socket_input_stream::~socket_input_stream()
	{
		close();
	}

	int socket_output_stream::unsafe_send(size_t offset, size_t count, bool do_throw)
	{
		int w = send(file_descriptor(), buffer_ + offset, count, MSG_DONTWAIT);
		if (w == -1) {
			switch (errno) {
			case ECONNRESET:
				return stream_result::RESET_BY_PEER;
			case EAGAIN:
				return 0;
			case EINTR:
				return stream_result::INTERRUPTED;
			default:
				if (do_throw) {
					throw std::runtime_error(strerror(errno));
				}
			}
		}
		return w;
	}

	int socket_output_stream::unsafe_flush(bool do_throw)
	{
		size_t written = 0;
		while (written < pos_) {
			long signed w = unsafe_send(written, pos_ - written, do_throw);
			if (w < 0) {
				return w;
			}
			written += w;
		}
		pos_ = 0;
		return written;
	}

	int socket_output_stream::unsafe_write(char c)
	{
		if (pos_ < STREAM_BUFFER_SIZE) {
			buffer_[pos_++] = c;
			return 1;
		}
		int f = unsafe_flush(true);
		if (f <= 0) {
			return f;
		}
		if (pos_ == STREAM_BUFFER_SIZE) {
			buffer_[0] = c;
			pos_ = 1;
		}
		else {
			buffer_[pos_++] = c;
		}
		return 1;
	}

	socket_output_stream::socket_output_stream(int fd, bool owns_file)
	{
		set_file(fd, owns_file);
	}
	socket_output_stream::socket_output_stream() : socket_output_stream(-1, false) {}

	void socket_output_stream::on_file_set()
	{
		pos_ = 0;
	}

	bool socket_output_stream::canWriteToBuffer() const
	{
		return file_descriptor() != -1 && pos_ < STREAM_BUFFER_SIZE;
	}

	int socket_output_stream::write(char c)
	{
		if (file_descriptor() == -1) {
			return stream_result::INVALID_HANDLE;
		}
		return unsafe_write(c);
	}

	int socket_output_stream::write_string(const char *str, size_t max_len, size_t *written)
	{
		if (file_descriptor() == -1) {
			if (written) {
				*written = 0;
			}
			return stream_result::INVALID_HANDLE;
		}
		if (max_len == 0) {
			max_len == std::numeric_limits<size_t>::max();
		}
		for (size_t i = 0; max_len == 0 || i < max_len; i++) {
			char c = str[i];
			if (c == 0) {
				if (written) {
					*written = i;
				}
				return i;
			}
			int w = unsafe_write(c);
			if (w < 0) {
				if (written) {
					*written = i;
				}
				return w;
			}
		}
	}

	void socket_output_stream::flush()
	{
		if (file_descriptor() >= 0) {
			unsafe_flush(false);
		}
	}

	void socket_output_stream::close()
	{
		set_file(-1, false);
	}

	void socket_output_stream::close_file()
	{
		::close(file_descriptor());
	}
	void socket_output_stream::before_close_file()
	{
		flush();
	}

	socket_output_stream::~socket_output_stream()
	{
		close();
	}

} /* End of namespace speakerman */

