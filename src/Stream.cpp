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

#include <speakerman/Stream.hpp>
#include <stdexcept>

namespace speakerman {
using namespace std;
static thread_local int last_result;
static thread_local size_t last_count;

int last_stream_result() { return last_result; }

void set_stream_result(int result) { last_result = result; }

size_t last_operation_count() { return last_count; }

void set_last_operation_count(size_t count) { last_count = count; }

signed long read_from_stream(input_stream &stream, void *data, size_t offs,
                             size_t length) {
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

signed long read_line_from_stream(input_stream &stream, char *line,
                                  size_t buff_size) {
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

signed long write_to_stream(output_stream &stream, const void *data,
                            size_t offs, size_t length) {
  const char *buff = static_cast<const char *>(data);
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

signed long write_string_to_stream(output_stream &stream, const char *buff,
                                   size_t length) {
  size_t end = length;
  size_t i;
  int r;
  for (i = 0; i < end; i++) {
    char c = buff[i];
    if (c == 0) {
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

signed long input_stream::read(void *buff, size_t offs, size_t length) {
  return read_from_stream(*this, buff, offs, length);
}

signed long input_stream::read_line(char *line, size_t line_size) {
  return read_line_from_stream(*this, line, line_size);
}

signed long output_stream::write(const void *buff, size_t offs, size_t length) {
  return write_to_stream(*this, buff, offs, length);
}

signed long output_stream::write_string(const char *string, size_t length) {
  return write_string_to_stream(*this, string, length);
}
signed long output_stream::write_string(const char *string) {
  if (!string) {
    return 0;
  }
  auto p = string;
  size_t len;
  for (len = 0; len < 1048576 && *p; len++, p++);
  return write_string(string, len);
}

file_owner::file_owner() : file_descriptor_(-1), owns_file_(false) {}

void file_owner::set_file(int file_descriptor, bool owns_file) {
  //		printf("file_owner(%p)::set_file(%i, %d)\n", this,
  //file_descriptor, owns_file);
  cleanup_file();
  file_descriptor_ = file_descriptor;
  owns_file_ = owns_file;
  on_file_set();
}

file_owner::~file_owner() { cleanup_file(); }

void file_owner::cleanup_file() {
  //		printf("file_owner(%p)::cleanup_file()\n", this);
  if (file_descriptor_ >= 0) {
    before_close_file();
    if (owns_file_) {
      close_file();
    }
  }
}

static size_t valid_capacity(size_t cap) {
  if (cap >= 128 && cap <= 104896000) {
    return cap;
  }
  throw std::invalid_argument("Invalid capacity");
}

buffer_stream::buffer_stream(size_t capacity)
    : capacity_(valid_capacity(capacity)), data_(new char[capacity_]), read_(0),
      write_(0) {}

buffer_stream::~buffer_stream() {
  if (data_) {
    char *p = data_;
    data_ = nullptr;
    delete[] p;
  }
}

size_t buffer_stream::maximum_size() const { return capacity_ - 1; }

size_t buffer_stream::allocated_size() const { return capacity_; }

char *buffer_stream::data() const { return data_; }

int buffer_stream::read() {
  if (read_ != write_) {
    char c = data_[read_++];
    if (read_ == capacity_) {
      read_ = 0;
    }
    return c;
  }
  return stream_result::END_OF_STREAM;
}

size_t buffer_stream::readable_size() const {
  return write_ > read_ ? write_ - read_ : capacity_ + write_ - read_;
}

int buffer_stream::write(char c) {
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

void buffer_stream::flush() { write_ = read_ = 0; }

void buffer_stream::close() { write_ = read_ = 0; }

} /* End of namespace speakerman */
