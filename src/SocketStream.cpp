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

	raw_socket_input_stream::raw_socket_input_stream(int file_descriptor, bool owns_descriptor) :
			file_descriptor_(file_descriptor), owns_descriptor_(owns_descriptor), blocking_(true) {}

	raw_socket_input_stream::raw_socket_input_stream() :
		raw_socket_input_stream(-1, false) {}

	int raw_socket_input_stream::read()
	{
		if (file_descriptor_ == -1) {
			return stream_result::INVALID_HANDLE;
		}
		char result;
		int r = blocking_ ?
				recv(file_descriptor_, &result, 1, 0) :
				recv(file_descriptor_, &result, 1, MSG_DONTWAIT);
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
		if (r == 0) {
			return stream_result::END_OF_STREAM;
		}
		return result;
	}

	signed long raw_socket_input_stream::read(void* destination, size_t offs, size_t length)
	{
		if (file_descriptor_ == -1) {
			return stream_result::INVALID_HANDLE;
		}
		int r = blocking_ ?
				recv(file_descriptor_, destination, STREAM_BUFFER_SIZE, 0) :
				recv(file_descriptor_, destination, STREAM_BUFFER_SIZE, MSG_DONTWAIT);

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
		return r;
	}

	void raw_socket_input_stream::close() throw()
	{
		if (file_descriptor_ != -1) {
			if (owns_descriptor_) {
				::close(file_descriptor_);
			}
			file_descriptor_ = -1;
		}
	}

	void raw_socket_input_stream::set_bocking(bool value)
	{
		blocking_ = value;
	}

	bool raw_socket_input_stream::get_bocking() const
	{
		return blocking_;
	}

	void raw_socket_input_stream::set_file_descriptor(int file_descriptor, bool owns_descriptor)
	{
		close();
		file_descriptor_ = file_descriptor;
		owns_descriptor_ = owns_descriptor;
	}

	raw_socket_input_stream::~raw_socket_input_stream()
	{
		close();
	}

	socket_input_stream::socket_input_stream(size_t buffer_size) :
			socket_input_stream(buffer_size, -1, false)
	{
	}

	socket_input_stream::socket_input_stream(size_t buffer_size, int file_descriptor, bool owns_descriptor) :
			buffered_(buffer_size)
	{
		buffered_.set_resource(&stream_, false);
		stream_.set_file_descriptor(file_descriptor, owns_descriptor);
	}

	void socket_input_stream::close() throw()
	{
		buffered_.close();
	}
	void socket_input_stream::flush()
	{
		buffered_.flush();
	}
	void socket_input_stream::set_file_descriptor(int file_descriptor, bool owns_descriptor)
	{
		buffered_.close();
		stream_.set_file_descriptor(file_descriptor, owns_descriptor);
		buffered_.set_resource(&stream_, false);
	}
	void socket_input_stream::set_bocking(bool value)
	{
		stream_.set_bocking(value);
	}
	bool socket_input_stream::get_bocking()
	{
		return stream_.get_bocking();
	}
	socket_input_stream::~socket_input_stream()
	{
		close();
	}

	raw_socket_output_stream::raw_socket_output_stream() :
			raw_socket_output_stream(-1, false) {}

	raw_socket_output_stream::raw_socket_output_stream(int file_descriptor, bool owns_descriptor) :
			file_descriptor_(file_descriptor), owns_descriptor_(owns_descriptor), blocking_(false) { }

	int raw_socket_output_stream::write(char c)
	{
		char buffer = c;
		int w = blocking_ ?
				send(file_descriptor_, &buffer, 1, 0) :
				send(file_descriptor_, &buffer, 1, MSG_DONTWAIT);
		if (w == -1) {
			switch (errno) {
			case ECONNRESET:
				return stream_result::RESET_BY_PEER;
			case EAGAIN:
				return 0;
			case EINTR:
				return stream_result::INTERRUPTED;
			}
		}
		return w;

	}

	signed long raw_socket_output_stream::write(const void *source, size_t offset, size_t length)
	{
		const char * buffer_ = static_cast<const char *>(source);
		int w = send(file_descriptor_, buffer_ + offset, length, MSG_DONTWAIT);
		if (w == -1) {
			switch (errno) {
			case ECONNRESET:
				return stream_result::RESET_BY_PEER;
			case EAGAIN:
				return 0;
			case EINTR:
				return stream_result::INTERRUPTED;
			}
		}
		return w;
	}

	void raw_socket_output_stream::set_file_descriptor(int file_descriptor, bool owns_descriptor)
	{
		close();
		file_descriptor_ = file_descriptor;
		owns_descriptor_ = owns_descriptor;
	}
	void raw_socket_output_stream::set_bocking(bool value)
	{
		blocking_ = value;
	}
	bool raw_socket_output_stream::get_bocking() const
	{
		return blocking_;
	}
	void raw_socket_output_stream::close() throw()
	{
		if (file_descriptor_ != -1) {
			if (owns_descriptor_) {
				::close(file_descriptor_);
			}
			file_descriptor_ = -1;
		}
	}
	raw_socket_output_stream::~raw_socket_output_stream()
	{
		close();
	}

	socket_output_stream::socket_output_stream(size_t buffer_size, int file_descriptor, bool owns_descriptor) :
		buffered_(buffer_size)
	{
		buffered_.set_resource(&stream_, false);
		stream_.set_file_descriptor(file_descriptor, owns_descriptor);
	}

	socket_output_stream::socket_output_stream(size_t buffer_size) :
		socket_output_stream(buffer_size, -1, false) {}

	void socket_output_stream::set_file_descriptor(int file_descriptor, bool owns_descriptor)
	{
		close();
		stream_.set_file_descriptor(file_descriptor, owns_descriptor);
		buffered_.set_resource(&stream_, false);
	}
	void socket_output_stream::set_bocking(bool value)
	{
		stream_.set_bocking(value);
	}
	bool socket_output_stream::get_bocking()
	{
		return stream_.get_bocking();
	}
	void socket_output_stream::flush()
	{
		buffered_.flush();
	}
	void socket_output_stream::close() throw()
	{
		buffered_.close();
	}
	socket_output_stream::~socket_output_stream()
	{
		close();
	}

	socket_stream::socket_stream(size_t read_buffer_size, size_t write_buffer_size, int file_descriptor, bool owns_descriptor) :
			ibuffered_(read_buffer_size),
			obuffered_(write_buffer_size)
	{
		ibuffered_.set_resource(&istream_, false);
		obuffered_.set_resource(&ostream_, false);
		ostream_.set_file_descriptor(file_descriptor, owns_descriptor);
		istream_.set_file_descriptor(file_descriptor, false);
	}
	socket_stream::socket_stream(size_t read_buffer_size, size_t write_buffer_size) :
			socket_stream(read_buffer_size, write_buffer_size, -1, false) {}

	socket_stream::socket_stream(size_t buffer_size) :
			socket_stream(buffer_size, buffer_size) {}

	void socket_stream::flush()
	{
		ibuffered_.flush();
		obuffered_.flush();
	}
	void socket_stream::close() throw()
	{
		ibuffered_.close();
		obuffered_.close();
	}
	void socket_stream::set_file_descriptor(int file_descriptor, bool owns_descriptor)
	{
		close();
		istream_.set_file_descriptor(file_descriptor, false);
		ostream_.set_file_descriptor(file_descriptor, owns_descriptor);
		ibuffered_.set_resource(&istream_, false);
		obuffered_.set_resource(&ostream_, false);
	}
	void socket_stream::set_read_bocking(bool value)
	{
		istream_.set_bocking(value);
	}
	bool socket_stream::get_read_bocking()
	{
		return istream_.get_bocking();
	}
	void socket_stream::set_write_bocking(bool value)
	{
		ostream_.set_bocking(value);
	}
	bool socket_stream::get_write_bocking()
	{
		return ostream_.get_bocking();
	}
	socket_stream::~socket_stream()
	{
		close();
	}


} /* End of namespace speakerman */

