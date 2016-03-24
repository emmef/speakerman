/*
 * Stream.cpp
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

#include <stdexcept>
#include <cstring>
#include <speakerman/Stream.hpp>

namespace speakerman
{
	using namespace std;
	static thread_local int last_result;
	static thread_local size_t last_count;

	int last_stream_result()
	{
		return last_result;
	}

	void set_stream_result(int result)
	{
		last_result = result;
	}

	size_t last_operation_count()
	{
		return last_count;
	}
	void set_last_operation_count(size_t count)
	{
		last_count = count;
	}

	static size_t valid_capacity(size_t cap)
	{
		if (cap >= 8 && cap <= 104896000) {
			return cap;
		}
		throw std::invalid_argument("Invalid capacity");
	}

	template <typename T>
	static T * non_null(T *ptr)
	{
		if (ptr) {
			return ptr;
		}
		throw std::invalid_argument("Null pointer argument");
	}

	template<typename T>
	static T minimum_of(T v1, T v2)
	{
		return v1 < v2 ? v1 : v2;
	}

	template<typename T>
	static T maximum_of(T v1, T v2)
	{
		return v1 >= v2 ? v1 : v2;
	}

	signed long read_from_stream(input_stream &stream, void* data, size_t offs, size_t length)
	{
		char *buff = static_cast<char *>(data);
		size_t end = offs + length;
		size_t i;
		int r;
		for (i = offs; i < end; i++) {
			r = stream.read();
			if (r < 0) {
				break;
			}
			buff[i] = r;
		}
		size_t l = i - offs;
		set_last_operation_count(l);
		if (r >= 0 || r == stream_result::END_OF_STREAM) {
			return l;
		}
		set_stream_result(r);
		return r;
	}

	signed long read_line_from_stream(input_stream &stream, char* line, size_t buff_size)
	{
		if (buff_size < 1) {
			return 0;
		}
		size_t end = buff_size - 1;
		size_t i;
		int r;
		for (i = 0; i < end; i++) {
			r = stream.read();
			if (r < 0) {
				break;
			}
			if (r == '\r' || r == '\n') {
				break;
			}
			line[i] = r;
		}
		line[i] = 0;
		set_last_operation_count(i);
		if (r == '\r' || r == '\n') {
			return i;
		}
		if (r < 0) {
			return r;
		}
		return stream_result::DATA_TRUNCATED;
	}


	signed long write_to_stream(output_stream & stream, const void * data, size_t offs, size_t length)
	{
		const char * buff = static_cast<const char *>(data);
		size_t end = offs + length;
		size_t i;
		int r;
		for (i = offs; i < end; i++) {
			r = stream.write(buff[i]);
			if (r < 0) {
				break;
			}
		}
		size_t l = i - offs;
		set_last_operation_count(l);
		if (r >= 0 || r == stream_result::END_OF_STREAM) {
			return l;
		}
		set_stream_result(r);
		return r;
	}

	signed long write_string_to_stream(output_stream & stream, const char* buff, size_t length)
	{
		size_t end = length;
		size_t i;
		int r;
		for (i = 0; i < end; i++) {
			char c = buff[i];
			if(c == 0) {
				set_last_operation_count(i);
				return i;
			}
			r = stream.write(buff[i]);
			if (r < 0) {
				set_last_operation_count(i);
				return r;
			}
		}

		return buff[i] == 0 ? i : stream_result::DATA_TRUNCATED;
	}

	signed long input_stream::read(void* buff, size_t offs, size_t length)
	{
		return read_from_stream(*this, buff, offs, length);
	}

	signed long input_stream::read_line(char* line, size_t line_size)
	{
		return read_line_from_stream(*this, line, line_size);
	}

	signed long output_stream::write(const void* buff, size_t offs, size_t length)
	{
		return write_to_stream(*this, buff, offs, length);
	}

	signed long output_stream::write_string(const char*string, size_t length)
	{
		return write_string_to_stream(*this, string, length);
	}


	buffered_input_stream::buffered_input_stream(size_t buffer_size, input_stream *stream, bool owns_stream) :
			size_(valid_capacity(buffer_size)), wr_(0), rd_(0), data_(new char[size_]), stream_(stream), owns_stream_(owns_stream)
	{
	}

	buffered_input_stream::buffered_input_stream(size_t buffer_size) :
			size_(valid_capacity(buffer_size)), wr_(0), rd_(0), data_(new char[size_]), stream_(nullptr), owns_stream_(false)
	{
	}

	int buffered_input_stream::read()
	{
		if (rd_ < wr_) {
			return data_[rd_++];
		}
		if (!stream_) {
			return stream_result::INVALID_HANDLE;
		}
		long signed r = stream_->read(data_, 0, size_);
		if (r < 0) {
			return r == stream_result::END_OF_STREAM ? 0 : r;
		}
		wr_ = r;
		rd_ = 0;
		if (rd_ < wr_) {
			return data_[rd_++];
		}
		return stream_result::END_OF_STREAM;
	}

	signed long buffered_input_stream::read(void* buff, size_t offs, size_t length)
	{
		if (length == 0) {
			return 0;
		}
		char *dest = static_cast<char*>(buff) + offs;
		size_t to_read = length;
		if (rd_ < wr_) {
			size_t moves = wr_ - rd_;
			if (moves > length) {
				memmove(dest, data_ + rd_, length);
				rd_ += length;
				return length;
			}
			memmove(dest, data_ + rd_, moves);
			to_read -= moves;
			dest += moves;
			rd_ = wr_ = 0; // buffer depleted
		}
		if (!stream_) {
			return stream_result::INVALID_HANDLE;
		}
		// bypass own buffer and write to target!
		for (int retry = 0; (to_read > 0) && (retry < 3); retry++) {
			long signed r = stream_->read(dest, 0, to_read);
			if (r < 0) {
				return r == stream_result::END_OF_STREAM ? length - to_read : r;
			}
			to_read -= r;
			dest += r;
		}
		return length - to_read;
	}

	void buffered_input_stream::close() throw()
	{
		flush();
		if (stream_) {
			stream_->close();
			if (owns_stream_) {
				delete stream_;
			}
			stream_ = nullptr;
		}
	}

	void buffered_input_stream::flush()
	{
		rd_ = wr_ = 0;
	}

	void buffered_input_stream::set_resource(input_stream *stream, bool owns_stream)
	{
		close();
		stream_ = stream;
		owns_stream_ = owns_stream;
	}

	buffered_input_stream::~buffered_input_stream()
	{
		if (data_) {
			delete [] data_;
			data_ = nullptr;
		}
		close();
	}

	buffered_output_stream::buffered_output_stream(size_t buffer_size, output_stream *stream, bool owns_stream) :
			size_(valid_capacity(buffer_size)), wr_(0), rd_(0), data_(new char[size_]), stream_(stream), owns_stream_(owns_stream)
	{

	}

	buffered_output_stream::buffered_output_stream(size_t buffer_size) :
			size_(valid_capacity(buffer_size)), wr_(0), rd_(0), data_(new char[size_]), stream_(nullptr), owns_stream_(false)
	{

	}

	int buffered_output_stream::write(char c)
	{
		if (wr_ < size_) {
			data_[wr_++] = c;
			return 1;
		}
		long signed w = internal_flush();
		if (w <= 0) {
			return w;
		}
		data_[wr_++] = c;
		return 1;
	}

	signed long buffered_output_stream::write(const void *buff, size_t offs, size_t length)
	{
		if (length == 0) {
			return 0;
		}
		const char *source = static_cast<const char*>(buff) + offs;
		if (wr_ + length <= size_) {
			memmove(data_, source, length);
			wr_ += length;
			return length;
		}
		if (!stream_) {
			return stream_result::INVALID_HANDLE;
		}
		long signed w = internal_flush();
		if (w < 0) {
			return w;
		}
		else if (w == 0 && wr_ != rd_) {
			return 0;
		}
		size_t to_write = length;
		for (int retry = 0; (to_write >= 0) && (retry < 10); retry++) {
			w = stream_->write(source, 0, to_write);
			if (w < 0) {
				return w == stream_result::END_OF_STREAM ? length - to_write : w;
			}
			source += w;
			to_write -= w;
		}
		return length - to_write;
	}

	long signed buffered_output_stream::internal_flush()
	{
		if (wr_ == rd_) {
			return 0;
		}
		size_t rd = rd_;
		for (int retry = 0; (wr_ != rd_) && retry < 10; retry++) {
			long signed w = stream_->write(data_, rd_, wr_ - rd_);
			if (w < 0) {
				if (w != stream_result::END_OF_STREAM) {
					return w;
				}
				rd_ += w;
			}
		}
		size_t written = rd_ - rd;
		if (rd_ == wr_) {
			rd_ = wr_ = 0;
		}
		return written;
	}

	void buffered_output_stream::flush()
	{
		if (stream_) {
			internal_flush();
		}
	}

	void buffered_output_stream::close() throw()
	{
		flush();
		if (stream_) {
			stream_->close();
			if (owns_stream_) {
				delete stream_;
			}
			stream_ = nullptr;
		}
	}

	void buffered_output_stream::set_resource(output_stream *stream, bool owns_stream)
	{
		close();
		stream_ = stream;
		owns_stream_ = owns_stream;
	}

	buffered_output_stream::~buffered_output_stream()
	{
		if (data_) {
			delete [] data_;
			data_ = nullptr;
		}
		close();
	}

	buffer_stream::buffer_stream(size_t capacity) :
			capacity_(valid_capacity(capacity)),
			data_(new char[capacity_]),
			read_(0),
			write_(0) {}

	buffer_stream::~buffer_stream()
	{
		if (data_) {
			char *p = data_;
			data_ = nullptr;
			delete [] p;
		}
	}

	size_t buffer_stream::maximum_size() const {
		return capacity_ - 1;
	}

	size_t buffer_stream::allocated_size() const {
		return capacity_;
	}

	char* buffer_stream::data() const
	{
		return data_;
	}

	int buffer_stream::read()
	{
		if (read_ != write_) {
			char c = data_[read_++];
			if (read_ == capacity_) {
				read_ = 0;
			}
			return c;
		}
		return stream_result::END_OF_STREAM;
	}

	size_t buffer_stream::readable_size() const
	{
		return write_ > read_ ? write_ - read_ : capacity_ + write_ - read_;
	}

	int buffer_stream::write(char c)
	{
		size_t nextWrite = write_ + 1;
		if (nextWrite == capacity_) {
			nextWrite = 0;
		}
		if (nextWrite != read_) {
			data_[write_] = c;
			write_ = nextWrite;
			return 1;
		}
		return stream_result::END_OF_STREAM;
	}

	void buffer_stream::flush()
	{
		write_ = read_ = 0;
	}

	void buffer_stream::close() throw()
	{
		write_ = read_ = 0;
	}

} /* End of namespace speakerman */
