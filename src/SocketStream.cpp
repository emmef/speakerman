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
				recv(fd_, buffer_, STREAM_BUFFER_SIZE, 0) :
				recv(fd_, buffer_, STREAM_BUFFER_SIZE, MSG_DONTWAIT);
		if (r < 0) {
			std::cout << "Error code " << strerror(errno) << std::endl;
			switch (errno) {
			case EAGAIN:
				return EOF;
			case EINTR:
				return INTERRUPTED;
			default:
				throw std::runtime_error(strerror(errno));
			}
		}
		else if (r == 0) {
			std::cout << "No bytes read" << std::endl;
			return EOF;
		}
		else {
			std::cout << "Read " << r << " bytes" << std::endl;

			mark_ = r;
			pos_ = 0;
			return buffer_[pos_++];
		}
	}

	socket_input_stream::socket_input_stream(int fd) { set_file(fd); }
	socket_input_stream::socket_input_stream() : socket_input_stream(-1) {}

	bool socket_input_stream::canReadFromBuffer() const
	{
		return fd_ != -1 && pos_ < mark_;
	}

	int socket_input_stream::read()
	{
		if (fd_ == -1) {
			return NOFILE;
		}
		return unsafe_read();
	}

	void socket_input_stream::set_file(int fd)
	{
		fd_ = fd;
		pos_ = mark_ = 0;
	}

	int socket_input_stream::readLine(char *buffer, size_t size)
	{
		size_t pos = 0;
		if (fd_ == -1) {
			return NOFILE;
		}
		if (size == 0 || !buffer) {
			return INVALID_ARGUMENT;
		}
		buffer[0] = 0;
		if (size == 1) {
			return 0;
		}
		size_t length = size - 1;
		while (pos < length) {
			int rd = unsafe_read();
			switch (rd) {
			case INTERRUPTED:
				buffer[pos] = 0;
				return INTERRUPTED;
			case EOF:
				return terminate_line(buffer, pos);
			case '\n':
			case '\r':
				if (pos != 0) {
					return terminate_line(buffer, pos);
				}
				break;
			default:
				buffer[pos++] = rd;
			}
		}
		return terminate_line(buffer, pos);
	}

	socket_input_stream::~socket_input_stream()
	{
		set_file(-1);
	}

	int socket_output_stream::unsafe_send(size_t offset, size_t count, bool do_throw)
	{
		int w = send(fd_, buffer_ + offset, count, MSG_DONTWAIT);
		if (w == -1) {
			switch (errno) {
			case ECONNRESET:
				return RESET_BY_PEER;
			case EAGAIN:
				return 0;
			case EINTR:
				return INTERRUPTED;
			default:
				if (do_throw) {
					throw std::runtime_error(strerror(errno));
				}
			}
		}
		return w;
	}

	int socket_output_stream::default_flush(bool do_throw)
	{
		if (pos_ == 0) {
			return 0;
		}
		int w = unsafe_send(0, pos_, do_throw);
		if (w < 0) {
			return w;
		}
		if (w > 0) {
			mark_ = w;
		}
		return w;
	}

	int socket_output_stream::unsafe_flush(bool do_throw)
	{
		if (mark_ == STREAM_BUFFER_SIZE) {
			return default_flush(do_throw);
		}
		else if (mark_ < pos_) {
			int w = unsafe_send(mark_, pos_ - mark_, do_throw);
			if (w < 0) {
				return w;
			}
			mark_ += w;
			return w;
		}
		else if (mark_ > pos_) {
			int w = unsafe_send(mark_, STREAM_BUFFER_SIZE - mark_, do_throw);
			if (w < 0) {
				return w;
			}
			mark_ += w;
			if (mark_ != STREAM_BUFFER_SIZE) {
				return w;
			}
			return default_flush(do_throw);
		}
		else {
			return 0;
		}
	}

	int socket_output_stream::unsafe_write(char c)
	{
		if (pos_ < mark_) {
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

	void socket_output_stream::unsafe_set_file(int fd)
	{
		fd_ = fd;
		pos_ = 0;
		mark_ = STREAM_BUFFER_SIZE;
	}

	socket_output_stream::socket_output_stream(int fd) { unsafe_set_file(fd); }
	socket_output_stream::socket_output_stream() : socket_output_stream(-1) {}

	bool socket_output_stream::canWriteToBuffer() const
	{
		return fd_ != -1 && pos_ != mark_;
	}

	int socket_output_stream::write(char c)
	{
		if (fd_ == -1) {
			return NOFILE;
		}
		return unsafe_write(c);
	}

	int socket_output_stream::write_string(const char *str, size_t max_len, size_t *written)
	{
		if (fd_ == -1) {
			if (written) {
				*written = 0;
			}
			return NOFILE;
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

	int socket_output_stream::flush()
	{
		if (fd_ == -1) {
			return NOFILE;
		}
		return unsafe_flush(true);
	}

	int socket_output_stream::set_file_check_flush(int fd, int *flushResult)
	{
		int r = fd_ != -1 ? unsafe_flush(false) : 0;
		if (flushResult) {
			*flushResult = r;
		}

		unsafe_set_file(fd);
		return r == INTERRUPTED ? INTERRUPTED : 0;
	}

	int socket_output_stream::set_file(int fd)
	{
		return set_file_check_flush(fd, nullptr);
	}

	socket_output_stream::~socket_output_stream()
	{
		unsafe_flush(false);
		unsafe_set_file(-1);
	}

	int socket_stream::set_file(int fd)
	{
		istream.set_file(fd);
		int r = ostream.set_file(fd);
		if (fd_ != -1) {
			close(fd_);
		}
		fd_ = fd;
		return r;
	}

	socket_stream::~socket_stream()
	{
		if (fd_ != -1) {
			ostream.unsafe_flush(false);
			ostream.unsafe_set_file(-1);
			istream.set_file(-1);
			close(fd_);
			fd_ = -1;
		}
	}

} /* End of namespace speakerman */

